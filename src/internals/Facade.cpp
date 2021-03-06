/*
 * Copyright (C) 2013-2014, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2013-2014, Athena Elafrou
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */

/**
 * \file Facade.cpp
 * \brief Wrappers of the SparseMatrix routines
 *
 * \author Computing Systems Laboratory (CSLab), NTUA
 * \date 2011&ndash;2014
 * \copyright This file is distributed under the BSD License. See LICENSE.txt
 * for details.
 */

#include <sparsex/internals/CsxGetSet.hpp>
#include <sparsex/internals/CsxSaveRestore.hpp>
#include <sparsex/internals/CsxUtil.hpp>
#include <sparsex/internals/Facade.hpp>
#include <sparsex/internals/Runtime.hpp>
#include <sparsex/internals/SparseMatrix.hpp>
#include <sparsex/internals/ThreadPool.hpp>

using namespace std;
using namespace sparsex;

/**
 *  Wrapper routines.
 */
void *CreateCSR(const spx_index_t *rowptr, const spx_index_t *colind,
                const spx_value_t *values, spx_index_t nr_rows,
                spx_index_t nr_cols, int zero_based)
{
  SparseMatrix<CSR<spx_index_t, spx_value_t> > *matrix =
    new SparseMatrix<CSR<spx_index_t, spx_value_t> >(rowptr, colind, values,
						     nr_rows, nr_cols,
						     zero_based);
  return (void *) matrix;
}

void *CreateMMF(const char *filename, spx_index_t *nr_rows,
                spx_index_t *nr_cols, spx_index_t *nnz)
{
  SparseMatrix<MMF<spx_index_t, spx_value_t> > *matrix =
    new SparseMatrix<MMF<spx_index_t, spx_value_t> >(filename);
  *nr_rows = static_cast<spx_index_t>(matrix->GetNrRows());
  *nr_cols = static_cast<spx_index_t>(matrix->GetNrCols());
  *nnz = static_cast<spx_index_t>(matrix->GetNrNonzeros());
    
  return (void *) matrix;
}

void *ReorderCSR(void *matrix, spx_index_t **permutation)
{
  SparseMatrix<CSR<spx_index_t, spx_value_t> > *mat = 
    (SparseMatrix<CSR<spx_index_t, spx_value_t> > *) matrix;
  vector<size_t> perm;

  mat->Reorder(perm);
  if (!perm.empty()) {
    *permutation = new spx_index_t[mat->GetNrRows()];
    copy(perm.begin(), perm.end(), *permutation);
  }

  return (void *) mat;
}

void *ReorderMMF(void *matrix, spx_index_t **permutation) 
{
  SparseMatrix<MMF<spx_index_t, spx_value_t> > *mat = 
    (SparseMatrix<MMF<spx_index_t, spx_value_t> > *) matrix;
  vector<size_t> perm;

  mat->Reorder(perm);
  if (!perm.empty()) {
    *permutation = new spx_index_t[mat->GetNrRows()];
    copy(perm.begin(), perm.end(), *permutation);
  }

  return (void *) mat;
}

void *TuneCSR(void *matrix, int *symmetric)
{
  RtConfig &config = RtConfig::GetInstance();
  config.CheckProperties<spx_index_t, spx_value_t>();

  RtCtx &rt_context = RtCtx::GetInstance();
  rt_context.SetRtCtx(config);

  SparseMatrix<CSR<spx_index_t, spx_value_t> > *mat = 
    (SparseMatrix<CSR<spx_index_t, spx_value_t> > *) matrix;

  spm_mt_t *spm_mt = mat->CreateCsx();
  if (config.GetProperty<bool>(RtConfig::MatrixSymmetric)) {
    *symmetric = 1;
  }

  return (void *) spm_mt;
}

void *TuneMMF(void *matrix, int *symmetric)
{
  RtConfig &config = RtConfig::GetInstance();
  config.CheckProperties<spx_index_t, spx_value_t>();

  RtCtx &rt_context = RtCtx::GetInstance();
  rt_context.SetRtCtx(config);

  SparseMatrix<MMF<spx_index_t, spx_value_t> > *mat = 
    (SparseMatrix<MMF<spx_index_t, spx_value_t> > *) matrix;

  spm_mt_t *spm_mt = mat->CreateCsx();
  if (config.GetProperty<bool>(RtConfig::MatrixSymmetric)) {
    *symmetric = 1;
  }

  return (void *) spm_mt;
}

void DestroyCSR(void *matrix)
{
  SparseMatrix<CSR<spx_index_t, spx_value_t> > *mat = 
    (SparseMatrix<CSR<spx_index_t, spx_value_t> > *) matrix;
  delete mat;
}

void DestroyMMF(void *matrix)
{
  SparseMatrix<MMF<spx_index_t, spx_value_t> > *mat = 
    (SparseMatrix<MMF<spx_index_t, spx_value_t> > *) matrix;
  delete mat;
}

void SaveTuned(void *matrix, const char *filename, spx_index_t *permutation)
{
  spm_mt_t *spm_mt = (spm_mt_t *) matrix;
  SaveCsx<spx_index_t, spx_value_t>(spm_mt, filename, permutation);
}

void *LoadTuned(const char *filename,
                spx_index_t *nr_rows, spx_index_t *nr_cols,
                spx_index_t *nnz, spx_index_t **permutation)
{
  spm_mt_t *spm_mt = 0;
  CsxMatrix<spx_index_t, spx_value_t> *csx = 0;

  spm_mt = RestoreCsx<spx_index_t, spx_value_t>(filename, permutation);
  for (unsigned int i = 0; i < spm_mt->nr_threads; i++) {
    csx =
      (CsxMatrix<spx_index_t, spx_value_t> *) spm_mt->spm_threads[i].csx;
    *nr_rows += csx->nrows;
    *nnz += csx->nnz;
  }
  
  *nr_cols = csx->ncols;
  return (void *) spm_mt;
}

int GetValue(void *matrix, spx_index_t row, spx_index_t col, spx_value_t *value)
{
  spm_mt_t *spm_mt = (spm_mt_t *) matrix;
  if (GetValueCsx<spx_index_t, spx_value_t>(spm_mt, row, col, value))
    return 0;
  return -1;
}

int SetValue(void *matrix, spx_index_t row, spx_index_t col, spx_value_t value)
{
  spm_mt_t *spm_mt = (spm_mt_t *) matrix;
  if (SetValueCsx<spx_index_t, spx_value_t>(spm_mt, row, col, value))
    return 0;
  return -1;
}

void DestroyCsx(void *matrix)
{
  spm_mt_t *spm_mt = (spm_mt_t *) matrix;
  PutSpmMt<spx_index_t, spx_value_t>(spm_mt);
}

void SetPropertyByMnemonic(const char *key, const char *value)
{
  RtConfig &rt_config = RtConfig::GetInstance();
  rt_config.SetProperty(rt_config.GetPropertyByMnemonic(string(key)),
			string(value));
}

void SetPropertiesFromEnv()
{
  RtConfig &config = RtConfig::GetInstance();
  config.LoadFromEnv();
}

void GetNodes(int *nodes)
{
  RtCtx &rt_context = RtCtx::GetInstance();

  for (size_t i = 0; i < rt_context.GetNrThreads(); i++) {
    nodes[i] = numa_node_of_cpu(rt_context.GetAffinity(i));
  }
}

void CreatePool()
{
  RtCtx &rt_context = RtCtx::GetInstance();
  ThreadPool &pool = ThreadPool::GetInstance();
  pool.InitThreads(rt_context.GetNrThreads() - 1);
}
