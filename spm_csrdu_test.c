#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "spm_csrdu.h"

int main(int argc, char **argv)
{
	if (argc < 2){
		fprintf(stderr, "Usage: %s <mmf_file>\n", argv[0]);
		exit(1);
	}

	uint64_t nrows0, ncols0, nnz0;
	spm_csrdu_double_t *csrdu;
	csrdu = spm_csrdu_double_init_mmf(argv[1], &nrows0, &ncols0, &nnz0);

	#if 0
	uint64_t nrows1, ncols1, nnz1;
	spm_crs32_double_t *crs;
	crs = spm_crs_double_init_mmf(argv[1], &nrows1, &ncols1, &nnz1);
	#endif

	printf("%lu %lu %lu\n", nrows0, ncols0, nnz0);

	uint8_t *ctl = csrdu->ctl;
	uint64_t row=0, col=1;
	double *vals = csrdu->values;
	double *vals_end = vals + nnz0;
	uint64_t i=0;
	for (;;){
		uint8_t flags = u8_get(ctl);
		uint8_t size = u8_get(ctl);

		if (spm_csrdu_fl_isnr(flags)){
			row++;
			col=1;
		}

		#define __fmt "%lu %lu %-14lf\n"
		switch (flags & SPM_CSRDU_FL_UNIT_MASK){
			case SPM_CSRDU_FL_UNIT_DENSE:
			col += uc_get_ul(ctl);
			for (i=0; i<size; i++){
				printf(__fmt, row, col+i, *vals++);
			}
			col += (size-1);
			break;

			case SPM_CSRDU_FL_UNIT_SP_U8:
			for (i=0; i<size; i++){
				col += u8_get(ctl);
				printf(__fmt, row, col, *vals++);
			}
			break;

			case SPM_CSRDU_FL_UNIT_SP_U16:
			for (i=0; i<size; i++){
				col += u16_get(ctl);
				printf(__fmt, row, col, *vals++);
			}
			break;

			case SPM_CSRDU_FL_UNIT_SP_U32:
			for (i=0; i<size; i++){
				col += u32_get(ctl);
				printf(__fmt, row, col, *vals++);
			}
			break;

			case SPM_CSRDU_FL_UNIT_SP_U64:
			for (i=0; i<size; i++){
				col += u64_get(ctl);
				printf(__fmt, row, col, *vals++);
			}
			break;

			default:
			assert(0);
		}


		if (vals == vals_end)
			break;
	}

	return 0;
}
