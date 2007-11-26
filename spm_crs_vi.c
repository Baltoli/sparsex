#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <time.h>
	
#ifndef SPM_CRSVI_CI_BITS
#define SPM_CRSVI_CI_BITS 32
#endif

#if SPM_CRSVI_CI_BITS == 32
#define  SPM_CRSVI_CI_TYPE uint32_t
#elif SPM_CRSVI_CI_BITS == 64
#define SPM_CRSVI_CI_TYPE uint64_t
#else
#error "SPM_CRSVI_CI_BITS not 32 or 64"
#endif

#ifndef SPM_CRSVI_VI_BITS
#define SPM_CRSVI_VI_BITS 8
#endif

#if SPM_CRSVI_VI_BITS == 32
#define  SPM_CRSVI_VI_TYPE uint32_t
#elif SPM_CRSVI_VI_BITS == 16
#define SPM_CRSVI_VI_TYPE uint16_t
#elif SPM_CRSVI_VI_BITS == 8
#define SPM_CRSVI_VI_TYPE uint8_t
#else
#error "SPM_CRSVI_VI_BITS not 8 or 16 or 32"
#endif

#include "vector.h"
#include "mmf.h"
#include "dynarray.h"
#include "ext_prog.h"
#include "spm_crs_vi.h"
#include "spmv_method.h"
#include "phash.h"

#define _CON7(a,b,c,d,e,f,g) a ## b ## c ## d ## e ## f ## g
#define CON7(a,b,c,d,e,f,g) _CON7(a,b,c,d,e,f,g)
#define SPM_CRS_VI_NAME(name) \
	CON7(spm_crs, SPM_CRSVI_CI_BITS, _vi, SPM_CRSVI_VI_BITS, _, ELEM_TYPE, name)
#define SPM_CRSVI_TYPE SPM_CRS_VI_NAME(_t)

struct crs_vi_state_s {
	SPM_CRSVI_TYPE *crs_vi;
	dynarray_t   *sp_values, *sp_colind, *sp_rowptr, *sp_valind;
	phash_t      *vhash;
	FILE         *vals_in, *vals_out;
};
typedef struct crs_vi_state_s crs_vi_state_t;

static void crsvi_initialize(crs_vi_state_t **crs_vi_state_ptr,
                             unsigned long rows_nr, unsigned long cols_nr,
                             unsigned long nz_nr)
{
	SPM_CRSVI_TYPE *crsvi;
	crs_vi_state_t *crsvi_st;

	crsvi = malloc(sizeof(SPM_CRSVI_TYPE));
	crsvi_st = malloc(sizeof(crs_vi_state_t));
	if ( !crsvi || !crsvi_st ){
		perror("spm_crs_vi_init: malloc");
		exit(1);
	}

	crsvi->ncols = cols_nr;
	crsvi->nrows = rows_nr;
	crsvi->nz = 0;

	crsvi_st->crs_vi = crsvi;

	crsvi_st->sp_values = dynarray_create(sizeof(ELEM_TYPE), 4096);
	crsvi_st->sp_valind = dynarray_create(sizeof(SPM_CRSVI_VI_TYPE), nz_nr);
	crsvi_st->sp_colind = dynarray_create(sizeof(SPM_CRSVI_CI_TYPE), nz_nr);
	crsvi_st->sp_rowptr = dynarray_create(sizeof(SPM_CRSVI_CI_TYPE), rows_nr);
	
	/* if this fails we need to make phash code more generic */
	assert(sizeof(double) == sizeof(unsigned long));
	crsvi_st->vhash = phash_new(12, 0);

	SPM_CRSVI_CI_TYPE *rowptr = dynarray_alloc(crsvi_st->sp_rowptr);
	*rowptr = 0;
	*crs_vi_state_ptr = crsvi_st;
}

static void *crsvi_finalize(crs_vi_state_t *crsvi_st)
{
	SPM_CRSVI_TYPE *crs_vi = crsvi_st->crs_vi;
	SPM_CRSVI_CI_TYPE *rowptr = dynarray_alloc(crsvi_st->sp_rowptr);

	assert(crs_vi->nz == dynarray_size(crsvi_st->sp_valind));
	*rowptr = crs_vi->nz;

	crs_vi->values = dynarray_destroy(crsvi_st->sp_values);
	crs_vi->val_ind = dynarray_destroy(crsvi_st->sp_valind);
	crs_vi->col_ind = dynarray_destroy(crsvi_st->sp_colind);
	crs_vi->row_ptr = dynarray_destroy(crsvi_st->sp_rowptr);

	phash_free(crsvi_st->vhash);

	free(crsvi_st);

	return crs_vi;
}

#define BUFLEN 1024
static void add_val(crs_vi_state_t *crsvi_st, double val)
{
	static unsigned long key;
	unsigned long idx;
	
	memcpy(&key, &val, sizeof(unsigned long));
	int exists = phash_lookup(crsvi_st->vhash, key, &idx);
	if (!exists){
		idx = phash_elements(crsvi_st->vhash);
		phash_insert(crsvi_st->vhash, key, idx);
		assert(idx == dynarray_size(crsvi_st->sp_values));
		ELEM_TYPE *v = dynarray_alloc(crsvi_st->sp_values);
		*v = (ELEM_TYPE)val;
	}

	SPM_CRSVI_VI_TYPE *val_idx = dynarray_alloc(crsvi_st->sp_valind);
	*val_idx = (SPM_CRSVI_CI_TYPE)idx;
}

SPM_CRSVI_TYPE *SPM_CRS_VI_NAME(_init_mmf) (char *mmf_file, 
                                       unsigned long *rows_nr, unsigned long *cols_nr,
                                       unsigned long *nz_nr)
{
	crs_vi_state_t *crsvi_st;
	FILE *f;
	double val;
	unsigned long row, col, prev_row=0, i;
	time_t t0, tn;
	char *report;
	unsigned long report_rows;

	f = mmf_init(mmf_file, rows_nr, cols_nr, nz_nr);

	crsvi_initialize(&crsvi_st, *rows_nr, *cols_nr, *nz_nr);
	SPM_CRSVI_TYPE *crsvi = crsvi_st->crs_vi;

	SPM_CRSVI_CI_TYPE *rowptr = dynarray_alloc(crsvi_st->sp_rowptr);
	*rowptr = 0;
	prev_row = 0;

	report = getenv("SPM_CRSVI_REPORT");
	if (report != NULL){
		report_rows = atol(report);
		if (!report_rows) 
			report_rows = 1024;
	} else {
		report_rows = 0;
	}

	unsigned long empty_rows;
	t0 = time(NULL);
	while (mmf_get_next(f, &row, &col, &val)){
		crsvi->nz++;
		
		/* row indices */
		if (prev_row < row){
			#if 1
			if (report_rows && row % report_rows == 0){
				tn = time(NULL);
				double ratio = (double)row/(tn - t0);
				unsigned long remaining = *rows_nr - row;
				printf("%s [ %lf m] remaining: %lu rows/sec:%lf ETA:%lf m\n", 
				        mmf_file, (double)(tn-t0)/60.0, remaining, ratio, (double)remaining/(ratio*60.0));
			}
			#endif
			empty_rows = row -prev_row -1;
			rowptr = dynarray_alloc_nr(crsvi_st->sp_rowptr,1+empty_rows);
			for (i=0; i<empty_rows; i++){
				*(rowptr+i) = *(rowptr+i-1);
			}
			*(rowptr+i) = crsvi->nz;
			prev_row = row;
		}

		/* column indices */
		SPM_CRSVI_CI_TYPE *colind = dynarray_alloc(crsvi_st->sp_colind);
		*colind = col;

		/* values */
		add_val(crsvi_st, val);
	}

	crsvi_finalize(crsvi_st);
	fclose(f);

	return crsvi;
}

void SPM_CRS_VI_NAME(_destroy)(SPM_CRSVI_TYPE *crs_vi)
{
	free(crs_vi->values);
	free(crs_vi->val_ind);
	free(crs_vi->col_ind);
	free(crs_vi->row_ptr);
	free(crs_vi);
}

void SPM_CRS_VI_NAME(_multiply) (void *spm, VECTOR_TYPE *in, VECTOR_TYPE *out)
{
	SPM_CRSVI_TYPE *crs_vi = (SPM_CRSVI_TYPE *)spm;
	ELEM_TYPE *y = out->elements;
	ELEM_TYPE *x = in->elements;
	ELEM_TYPE *values = crs_vi->values;
	const register SPM_CRSVI_CI_TYPE  *row_ptr = crs_vi->row_ptr;
	const register SPM_CRSVI_CI_TYPE  *col_ind = crs_vi->col_ind;
	const register SPM_CRSVI_VI_TYPE *val_ind = crs_vi->val_ind;
	const register unsigned long n = crs_vi->nrows;
	register ELEM_TYPE yr;

	register unsigned long i, j=0;
	for(i=0; i<n; i++) {
		yr = (ELEM_TYPE)0;
		__asm__ __volatile__ ("# loop start\n\t");
		for(j=row_ptr[i]; j<row_ptr[i+1]; j++) { 
			yr += (values[val_ind[j]] * x[col_ind[j]]);
		}
		y[i] = yr;
		__asm__ __volatile__ ("# loop end\n\t");
	}
}

#define XSPMV_METH_INIT(x,y,z) SPMV_METH_INIT(x,y,z)
XSPMV_METH_INIT(
	SPM_CRS_VI_NAME(_multiply),
	SPM_CRS_VI_NAME(_init_mmf),
	sizeof(ELEM_TYPE)
)
