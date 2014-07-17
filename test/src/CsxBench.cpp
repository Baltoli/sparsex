/*
 * Copyright (C) 2011-2014, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2011-2012, Vasileios Karakasis
 * Copyright (C) 2011-2012, Theodoros Gkountouvas
 * Copyright (C) 2013-2014, Athena Elafrou
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */

/*
 * \file CsxBench.cpp
 * \brief Benchmark utilities
 *
 * \author Vasileios Karakasis
 * \author Theodoros Gkountouvas
 * \author Athena Elafrou
 * \date 2011&ndash;2014
 * \copyright This file is distributed under the BSD License. See LICENSE.txt
 * for details.
 */

#include <sparsex/internals/Config.hpp>
#include "CsxBench.hpp"

static vector_t *x = NULL;
static vector_t *y = NULL;
static vector_t **tmp = NULL;
static size_t nr_loops = 0;
static size_t nr_threads = 0;
static size_t n = 0;
static float secs = 0.0;

using namespace sparsex::io;
using namespace sparsex::runtime;
using namespace sparsex::timing;

#define SWAP(x,y) \
	do { \
		__typeof__(x) _tmp; \
		_tmp = x; \
		x = y; \
		y = _tmp; \
	} while (0)

int GetOptionOuterLoops()
{
    const char *loops_env = getenv("OUTER_LOOPS");
    int ret = 1;
    
    if (loops_env) {
        ret = atoi(loops_env);
        if (ret < 0)
            ret = 0;
    }
    
    return ret;
}

static void do_spmv_thread(spm_mt_thread_t *thread, boost::barrier &cur_barrier)
{
	setaffinity_oncpu(thread->cpu);
	spmv_fn_t spmv_mt_fn = thread->spmv_fn;
    Timer timer_thr("Thread time");

	for (size_t i = 0; i < nr_loops; i++) {
		cur_barrier.wait();
        timer_thr.Start();
		spmv_mt_fn(thread->spm, x, y, 1, NULL);
        timer_thr.Pause();
		cur_barrier.wait();
	}

    thread->secs = timer_thr.ElapsedTime();
}

#if SPX_DISABLE_POOL
static void do_spmv_thread_main_swap(spm_mt_thread_t *thread,
                                     boost::barrier &cur_barrier)
{
	setaffinity_oncpu(thread->cpu);
	spmv_fn_t spmv_mt_fn = thread->spmv_fn;
    Timer timer_total("Total time");
    Timer timer_thr("Thread time");

	VecInitRandRange(x, (spx_value_t) -1000, (spx_value_t) 1000);

	// Assert this is a square matrix and swap is ok.
	assert(x->size == y->size);
    timer_total.Start();
	for (size_t i = 0; i < nr_loops; i++) {
		cur_barrier.wait();
        timer_thr.Start();
		spmv_mt_fn(thread->spm, x, y, 1, NULL);
        timer_thr.Pause();
		cur_barrier.wait();
		SWAP(x, y);
	}

    timer_total.Pause();
	thread->secs = timer_thr.ElapsedTime();
	secs = timer_total.ElapsedTime();
}
#endif

static void do_spmv_thread_sym(spm_mt_thread_t *thread,
                               boost::barrier &cur_barrier)
{
	setaffinity_oncpu(thread->cpu);
	spmv_fn_t spmv_mt_sym_fn = thread->spmv_fn;
	int id = thread->id;
    Timer timer_thr("Thread time");
	// Switch Reduction Phase.
	/* int j, start, end*/;

	// Switch Reduction Phase.
	/*
	start = (id * n) / ncpus;
	end = ((id + 1) * n) / ncpus;
	*/

	for (size_t i = 0; i < nr_loops; i++) {
		VecInitFromMap(tmp, 0, thread->map);
		cur_barrier.wait();
        timer_thr.Start();
		spmv_mt_sym_fn(thread->spm, x, y, 1, tmp[id]);
        timer_thr.Pause();
		cur_barrier.wait();
		VecAddFromMap(y, tmp, y, thread->map);
		cur_barrier.wait();
	}

    thread->secs = timer_thr.ElapsedTime();
}

#if SPX_DISABLE_POOL
static void do_spmv_thread_sym_main_swap(spm_mt_thread_t *thread,
                                         boost::barrier &cur_barrier)
{
	setaffinity_oncpu(thread->cpu);
	spmv_fn_t spmv_mt_sym_fn = thread->spmv_fn;
	int id = thread->id;
    
    Timer timer_thr("Thread time");
    Timer timer_total("Total time");

#ifdef SPMV_PRFCNT
	prfcnt_t *prfcnt = (prfcnt_t *) thread->data;
#endif

	VecInitRandRange(x, (spx_value_t) -1000, (spx_value_t) 1000);

	// Assert that the matrix is square and swap is OK.
	assert(x->size == y->size);
    timer_total.Start();
	for (size_t i = 0; i < nr_loops; i++) {
		// Switch Reduction Phase
		VecInitFromMap(tmp, 0, thread->map);
		cur_barrier.wait();
        timer_thr.Start();
		spmv_mt_sym_fn(thread->spm, x, y, 1, y);
		// spmv_mt_sym_fn(thread->spm, x, y, 1, tmp[id]);
        timer_thr.Pause();
		cur_barrier.wait();
		// Switch Reduction Phase
		VecAddFromMap(y, tmp, y, thread->map);
		cur_barrier.wait();
		SWAP(x, y);
	}
    
    timer_total.Pause();

	thread->secs = timer_thr.ElapsedTime();
	secs = timer_total.ElapsedTime();
}
#endif

static void csr_spmv(CSR<spx_uindex_t, spx_value_t> *spm, vector_t *in,
                     vector_t *out)
{
	spx_value_t *x = in->elements;
	spx_value_t *y = out->elements;
	spx_value_t *values = spm->values_;
	spx_uindex_t *row_ptr = spm->rowptr_;
	spx_uindex_t *col_ind = spm->colind_;
	const spx_uindex_t row_start = 0;
	const spx_uindex_t row_end = spm->GetNrRows();
	register spx_value_t yr;
	spx_uindex_t i,j;

	for (i = row_start; i < row_end; i++) {
		yr = (spx_value_t) 0;
		for (j = row_ptr[i]; j < row_ptr[i+1]; j++)
			yr += (values[j] * x[col_ind[j]]);

		y[i] = yr;
	}
}

float spmv_bench_mt(spm_mt_t *spm_mt, size_t loops, size_t nr_rows,
                    size_t nr_cols)
{
	secs = 0.0;
	nr_loops = loops;
    nr_threads = spm_mt->nr_threads;

#ifdef SPMV_PRFCNT
	for (size_t i = 0; i < nr_threads; i++) {
		spm_mt->spm_threads[i].data = malloc(sizeof(prfcnt_t));
		if (!spm_mt->spm_threads[i].data) {
			perror("malloc");
			exit(1);
		}
	}
#endif

#if SPX_USE_NUMA
	size_t *xparts = (size_t *) malloc(sizeof(*xparts) * nr_threads);
	size_t *yparts = (size_t *) malloc(sizeof(*yparts) * nr_threads);
	int *xnodes = (int *) malloc(sizeof(*xnodes) * nr_threads);
	int *ynodes = (int *) malloc(sizeof(*ynodes) * nr_threads);
	if (!xparts || !yparts || !xnodes || !ynodes) {
		perror("malloc");
		exit(1);
	}

	for (size_t i = 0; i < nr_threads; i++) {
		spm_mt_thread_t *spm = spm_mt->spm_threads + i;
		xparts[i] = spm->nr_rows * sizeof(spx_value_t);
		yparts[i] = spm->nr_rows * sizeof(spx_value_t);
		xnodes[i] = spm->node;
		ynodes[i] = spm->node;
    }

	// Allocate an interleaved x.
	x = VecCreateInterleaved(nr_cols, xparts, nr_threads, xnodes);
	VecInitRandRange(x, (spx_value_t) -1000, (spx_value_t) 1000);

	// Allocate an interleaved y.
	y = VecCreateInterleaved(nr_rows, yparts, nr_threads, ynodes);
	VecInit(y, 0);

	int alloc_err = 0;
	alloc_err = check_interleaved(x->elements, xparts, nr_threads, xnodes);
	print_alloc_status("input vector", alloc_err);
	alloc_err = check_interleaved(y->elements, yparts, nr_threads, ynodes);
	print_alloc_status("output vector", alloc_err);
#else
	x = VecCreate(nr_cols);
	y = VecCreate(nr_rows);
#endif  // SPX_USE_NUMA

	VecInitRandRange(x, (spx_value_t) -1000, (spx_value_t) 1000);

#if SPX_DISABLE_POOL
	/*
	 * spawn two kind of threads:
	 *	- 1	: do_spmv_thread_main_swap -> computes and does swap.
	 *	- N-1 : do_spmv_thread -> just computes.
	 */
    boost::barrier cur_barrier(nr_threads);
    vector<std::shared_ptr<boost::thread> > threads(nr_threads);
    threads[0] = make_shared<boost::thread>
        (boost::bind(&do_spmv_thread_main_swap, spm_mt->spm_threads,
                     boost::ref(cur_barrier)));
    for (size_t i = 1; i < nr_threads; ++i) {
        threads[i] = make_shared<boost::thread>
            (boost::bind(&do_spmv_thread, spm_mt->spm_threads + i,
                         boost::ref(cur_barrier)));
    }

    for (size_t i = 0; i < nr_threads; ++i) {
        threads[i]->join();
    }
#else
    ThreadPool &pool = ThreadPool::GetInstance();
    pool.InitThreads(nr_threads - 1);
    Timer timer_total("Total time");
    timer_total.Start();
    for (size_t i = 0; i < loops; i++) {
        MatVecKernel(spm_mt, x, 1, y, 1);
    }

    timer_total.Pause();
    secs = timer_total.ElapsedTime();
#endif

	VecDestroy(x);
	VecDestroy(y);
#if SPX_USE_NUMA
	free(xparts);
	free(yparts);
	free(xnodes);
	free(ynodes);
#endif

	return secs;
}

void spmv_check_mt(CSR<spx_uindex_t, spx_value_t> *csr, spm_mt_t *spm_mt,
                   size_t loops, size_t nr_rows, size_t nr_cols)
{
	vector_t *y_tmp;
	nr_loops = loops;
    nr_threads = spm_mt->nr_threads;

#if SPX_USE_NUMA
	size_t *parts = (size_t *) malloc(sizeof(*parts)*nr_threads);
	int *nodes = (int *) malloc(sizeof(*nodes)*nr_threads);
	if (!parts || !nodes) {
		perror("malloc");
		exit(1);
	}

	for (size_t i = 0; i < spm_mt->nr_threads; i++) {
		spm_mt_thread_t *spm = &(spm_mt->spm_threads[i]);
		parts[i] = spm->nr_rows * sizeof(spx_value_t);
		nodes[i] = spm->node;
	}

	int node = spm_mt->spm_threads[0].node;
	x = VecCreateOnnode(nr_cols, node);
	y = VecCreateInterleaved(nr_rows, parts, nr_threads, nodes);
	y_tmp = VecCreate(nr_rows);
#else
	x = VecCreate(nr_cols);
	y = VecCreate(nr_rows);
	y_tmp = VecCreate(nr_rows);
#endif
	VecInitRandRange(x, (spx_value_t) -1000, (spx_value_t) 1000);
	VecInit(y, (spx_value_t) 0);
    VecInit(y_tmp, (spx_value_t) 0);

    vector<std::shared_ptr<boost::thread> > threads(nr_threads);
    boost::barrier cur_barrier(nr_threads + 1);

	for (size_t i = 0; i < nr_threads; ++i) {
#ifdef SPMV_PRFCNT
		spm_mt->spm_threads[i].data = malloc(sizeof(prfcnt_t));
		if (!spm_mt->spm_threads[i].data) {
			perror("malloc");
			exit(1);
		}
#endif
        threads[i] = make_shared<boost::thread>
            (boost::bind(&do_spmv_thread, spm_mt->spm_threads + i,
                         boost::ref(cur_barrier)));
	}

	for (size_t i = 0; i < loops; i++) {
		cur_barrier.wait();
		cur_barrier.wait();
		csr_spmv(csr, x, y_tmp);
		if (VecCompare(y_tmp, y) < 0)
			exit(1);
	}

    for (size_t i = 0; i < nr_threads; ++i) {
        threads[i]->join();
    }

#ifdef SPMV_PRFCNT
	for (size_t i = 0; i < nr_threads; i++) {
		spm_mt_thread_t spmv_thread = spm_mt->spm_threads[i];
		prfcnt_t *prfcnt = (prfcnt_t *) spmv_thread.data;
		prfcnt_shut(prfcnt);
		free(prfcnt);
	}
#endif

	VecDestroy(x);
	VecDestroy(y);
	VecDestroy(y_tmp);
#if SPX_USE_NUMA
	free(nodes);
	free(parts);
#endif
}

float spmv_bench_sym_mt(spm_mt_t *spm_mt, size_t loops, size_t nr_rows,
                        size_t nr_cols)
{
	secs = 0.0;
	nr_loops = loops;
	nr_threads = spm_mt->nr_threads;
	n = nr_rows;

#ifdef SPMV_PRFCNT
	for (size_t i = 0; i < nr_threads; i++) {
		spm_mt->spm_threads[i].data = malloc(sizeof(prfcnt_t));
		if (!spm_mt->spm_threads[i].data) {
			perror("malloc");
			exit(1);
		}
	}
#endif

#if SPX_USE_NUMA
	size_t *xparts = (size_t *) malloc(sizeof(*xparts) * nr_threads);
	size_t *yparts = (size_t *) malloc(sizeof(*yparts) * nr_threads);
	int *xnodes = (int *) malloc(sizeof(*xnodes) * nr_threads);
	int *ynodes = (int *) malloc(sizeof(*ynodes) * nr_threads);
	if (!xparts || !yparts || !xnodes || !ynodes) {
		perror("malloc");
		exit(1);
	}

	for (size_t i = 0; i < nr_threads; i++) {
		spm_mt_thread_t *spm = spm_mt->spm_threads + i;
		xparts[i] = spm->nr_rows * sizeof(spx_value_t);
		yparts[i] = spm->nr_rows * sizeof(spx_value_t);
		xnodes[i] = spm->node;
		ynodes[i] = spm->node;
    }

	// Allocate an interleaved x.
	x = VecCreateInterleaved(nr_rows, xparts, nr_threads, xnodes);
	VecInitRandRange(x, (spx_value_t) -1000, (spx_value_t) 1000);

	// Allocate an interleaved y.
	y = VecCreateInterleaved(nr_rows, yparts, nr_threads, ynodes);
	VecInit(y, 0);

	int alloc_err = 0;
	alloc_err = check_interleaved(x->elements, xparts, nr_threads, xnodes);
	print_alloc_status("input vector", alloc_err);
	alloc_err = check_interleaved(y->elements, yparts, nr_threads, ynodes);
	print_alloc_status("output vector", alloc_err);
#else
	x = VecCreate(n);
	y = VecCreate(n);
#endif  // SPX_USE_NUMA

	tmp = (vector_t **) malloc(nr_threads * sizeof(vector_t *));
	if (!tmp) {
		perror("malloc");
		exit(1);
	}

    tmp[0] = y;
	for (size_t i = 1; i < nr_threads; i++) {
#if SPX_USE_NUMA
		int tnode = spm_mt->spm_threads[i].node;
		tmp[i] = VecCreateOnnode(n, tnode);
#else
		tmp[i] = VecCreate(n);
#endif
    }

	for (size_t i = 1; i < nr_threads; i++) {
		for (size_t j = 1; j < n; j++)
			tmp[i]->elements[j] = 0;
	}

#if SPX_USE_NUMA
	alloc_err = 0;
	for (size_t i = 1; i < nr_threads; i++) {
		int tnode = spm_mt->spm_threads[i].node;
		alloc_err += check_region(tmp[i]->elements, n*sizeof(spx_value_t),
                                  tnode);
	}
	print_alloc_status("local buffers", alloc_err);
#endif

#if SPX_DISABLE_POOL
    boost::barrier cur_barrier(nr_threads);
    vector<std::shared_ptr<boost::thread> > threads(nr_threads);
    threads[0] = make_shared<boost::thread>
        (boost::bind(&do_spmv_thread_sym_main_swap, spm_mt->spm_threads,
                     boost::ref(cur_barrier)));
    for (size_t i = 1; i < nr_threads; ++i) {
        threads[i] = make_shared<boost::thread>
            (boost::bind(&do_spmv_thread_sym, spm_mt->spm_threads + i,
                         boost::ref(cur_barrier)));
    }

    for (size_t i = 0; i < nr_threads; ++i) {
        threads[i]->join();
    }
#else
    spm_mt->local_buffers =
        (vector_t **) malloc(nr_threads*sizeof(vector_t *));

    unsigned int i;
#if SPX_USE_NUMA
    for (i = 1; i < nr_threads; i++) {
        int node = spm_mt->spm_threads[i].node;
        spm_mt->local_buffers[i] = VecCreateOnnode(n, node);
    }
#else
    for (i = 1; i < nr_threads; i++)
        spm_mt->local_buffers[i] = VecCreate(n);
#endif   // SPX_USE_NUMA

    ThreadPool &pool = ThreadPool::GetInstance();
    pool.InitThreads(nr_threads - 1);
    Timer timer_total("Total time");
    timer_total.Start();
    for (size_t i = 0; i < loops; i++) {
        MatVecKernel_sym(spm_mt, x, 1, y, 1);
    }

    timer_total.Pause();
    secs = timer_total.ElapsedTime();
#endif

	VecDestroy(x);
	VecDestroy(y);
	for (size_t i = 1; i < nr_threads; i++)
		VecDestroy(tmp[i]);
	free(tmp);
#if SPX_USE_NUMA
    free(xparts);
    free(xnodes);
    free(yparts);
    free(ynodes);
#endif

	return secs;
}

void spmv_check_sym_mt(CSR<spx_uindex_t, spx_value_t> *spm, spm_mt_t *spm_mt,
                       size_t loops, size_t nr_rows, size_t nr_cols)
{
	vector_t *y_tmp;

	nr_loops = loops;
	nr_threads = spm_mt->nr_threads;
	n = nr_rows;
    assert(nr_rows == nr_cols);

#if SPX_USE_NUMA
	size_t *parts = (size_t *) malloc(nr_threads * sizeof(*parts));
	int *nodes = (int *) malloc(nr_threads * sizeof(*nodes));
	if (!parts || !nodes) {
		perror("malloc");
		exit(1);
	}

	for (size_t i = 0; i < nr_threads; i++) {
		spm_mt_thread_t *spm = &(spm_mt->spm_threads[i]);
		parts[i] = spm->nr_rows * sizeof(spx_value_t);
		nodes[i] = spm->node;
	}

	x = VecCreateInterleaved(n, parts, nr_threads, nodes);
	y = VecCreateInterleaved(n, parts, nr_threads, nodes);
	y_tmp = VecCreate(n);
#else
	x = VecCreate(n);
	y = VecCreate(n);
	y_tmp = VecCreate(n);
#endif
    VecInitRandRange(x, (spx_value_t) -1000, (spx_value_t) 1000);
    VecInit(y, (spx_value_t) 0);
    VecInit(y_tmp, (spx_value_t) 0);

	tmp = (vector_t **) malloc(nr_threads * sizeof(vector_t *));
	if (!tmp) {
		perror("malloc");
		exit(1);
	}

	tmp[0] = y;
	for (size_t i = 1; i < nr_threads; i++) {
#if SPX_USE_NUMA
		int node = spm_mt->spm_threads[i].node;
        tmp[i] = VecCreateOnnode(n, node);
		for (size_t j = 1; j < n; j++)
			tmp[i]->elements[j] = 0;
#else
		tmp[i] = VecCreate(n);
#endif
    }

    vector<std::shared_ptr<boost::thread> > threads(nr_threads);
    boost::barrier cur_barrier(nr_threads + 1);

	for (size_t i = 0; i < nr_threads; i++) {
#ifdef SPMV_PRFCNT
		spm_mt->spm_threads[i].data = malloc(sizeof(prfcnt_t));
		if (!spm_mt->spm_threads[i].data) {
			perror("malloc");
			exit(1);
		}
#endif
        threads[i] = make_shared<boost::thread>
            (boost::bind(&do_spmv_thread_sym, spm_mt->spm_threads + i,
                         boost::ref(cur_barrier)));
	}

	for (size_t i = 0; i < loops; i++) {
		cur_barrier.wait();
		cur_barrier.wait();
		cur_barrier.wait();
		csr_spmv(spm, x, y_tmp);
		if (VecCompare(y_tmp, y) < 0)
			exit(1);
	}

    for (size_t i = 0; i < nr_threads; ++i) {
        threads[i]->join();
    }

#ifdef SPMV_PRFCNT
	for (size_t i = 0; i < nr_threads; i++) {
		spm_mt_thread_t spmv_thread = spm_mt->spm_threads[i];
		prfcnt_t *prfcnt = (prfcnt_t *) spmv_thread.data;
		prfcnt_shut(prfcnt);
		free(prfcnt);
	}
#endif

	VecDestroy(x);
	VecDestroy(y);
	VecDestroy(y_tmp);
	for (size_t i = 1; i < nr_threads; i++)
		VecDestroy(tmp[i]);
	free(tmp);
#if SPX_USE_NUMA
    free(parts);
    free(nodes);
#endif
}

void check_result(vector_t *result, vector_t *x, char *filename, size_t loops)
{
    CheckResult<spx_uindex_t, spx_value_t>(result, x, filename, loops);
}