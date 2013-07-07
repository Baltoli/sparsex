/* -*- C++ -*-
 *
 * SparsePartition.h --  Internal representation of sparse matrice partition.
 *
 * Copyright (C) 2009-2012, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2009-2011, Kornilios Kourtis
 * Copyright (C) 2009-2012, Vasileios Karakasis
 * Copyright (C) 2011-2012, Theodoros Gkountouvas
 * Copyright (C) 2013,      Athena Elafrou
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */
#ifndef CSX_SPARSEPARTITION_H__
#define CSX_SPARSEPARTITION_H__

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <iterator>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include "SparseUtil.h"
#include "dynarray.h"
#include "timer.h"

namespace bll = boost::lambda;
using namespace std;

namespace csx {

/**
 *  Internal representation of a sparse matrix. This class can also represent a
 *  sub-matrix of a larger one, starting at a specific row of the original
 *  matrix.
 */
template<typename IndexType, typename ValueType>
class SparsePartition
{
public:
    IndexType nr_rows_, nr_cols_, nr_nzeros_;
    IndexType elems_size_;
    size_t rowptr_size_;
    IndexType row_start_;    /* Row of the original matrix, where this
                               sub-matrix starts. */
private:
    IterOrder type_;
    Elem<IndexType, ValueType> *elems_;
    IndexType *rowptr_;
    bool elems_mapped_;

    // Maximum possible rowptr_size after transformations.
    size_t max_rowptr_size_;
    size_t nr_deltas_;

public:
    typedef IndexType index_t;
    typedef ValueType value_t;

    SparsePartition() : type_(NONE), elems_(NULL), rowptr_(NULL),
                        elems_mapped_(false) {}

    ~SparsePartition()
    {
        if (rowptr_)
            free(rowptr_);
    };

    IndexType GetNrRows() const
    {
        return rowptr_size_ - 1;
    }

    IndexType GetNrCols() const
    {
        return nr_cols_;
    }

    IndexType GetNrNonzeros() const
    {
        return nr_nzeros_;
    }

    size_t GetNrDeltas() const
    {
        return nr_deltas_;
    }

    IterOrder GetType() const
    {
        return type_;
    }

    IndexType GetElemsSize() const
    {
        return elems_size_;
    }

    IndexType GetRow(size_t idx) const
    {
        return rowptr_[idx];
    }
    
    IndexType GetRowStart() const
    {
        return row_start_;
    }

    size_t GetRowPtrSize() const
    {
        return rowptr_size_;
    }

    void SetNrRows(IndexType nr_rows)
    {
        nr_rows_ = nr_rows;
    }

    void SetNrCols(IndexType nr_cols)
    {
        nr_cols_ = nr_cols;
    }

    void SetNrNonzeros(IndexType nr_nzeros)
    {
        nr_nzeros_ = nr_nzeros;
    }

    void SetType(IterOrder type)
    {
        type_ = type;
    }
    
    void SetElemsMapped(bool elems_mapped)
    {
        elems_mapped_ = elems_mapped;
    }
 
    void SetRowStart(IndexType row_start)
    {
        row_start_ = row_start;
    }
    
    void SetRowPtrSize(size_t rowptr_size)
    {
        rowptr_size_ = rowptr_size;
    }

    void SetElems(Elem<IndexType, ValueType> *elem)
    {
        elems_ = elem;
    }

    void AddNrDeltas(size_t nr)
    {
        nr_deltas_ += nr;
    }

    Elem<IndexType, ValueType>& operator[](const size_t idx)
    {
        assert(idx >= 0 && idx < nr_nzeros_ && "Index out of bounds!");
        return *(elems_ + idx);
    }

    Elem<IndexType, ValueType> *RowBegin(size_t ridx = 0)
    {
        assert(ridx < rowptr_size_ - 1 && "ridx out of bounds");
        return &elems_[rowptr_[ridx]];
    }

    Elem<IndexType, ValueType> *RowEnd(size_t ridx = 0)
    {
        assert(ridx < rowptr_size_ - 1 && "ridx out of bounds");
        return &elems_[rowptr_[ridx+1]];
    }

    class Builder;
    //class BuilderSym;

    /**
     *  Function for filling the matrix using point iterators
     *
     *  @param pnts_start point iterators start.
     *  @param pnts_end   point iterators end.
     *  @param first_row  first row of the matrix.
     *  @param limit      limit for the number of elements.
     *  @param nr_elems   size for elems allocation.
     *  @param nr_rows    size for rows allocation.
     *  @param Bld        point to a builder responsible for allocations.
     *  @return           number of elements allocated.
     */
    template<typename IterT>
    IndexType SetRows(IterT &pi, const IterT &pnts_end, IndexType first_row,
                      size_t limit, IndexType nr_rows);
    template<typename IterT>
    IndexType SetRows(IterT &pi, const IterT &pnts_end, IndexType first_row,
                      size_t limit, IndexType nr_rows,
                      SparsePartition::Builder *Bld);
    template <typename IterT>
    IndexType SetElems(IterT &pnts_start, const IterT &pnts_end,
                       IndexType first_row, size_t limit = 0,
                       IndexType nr_elems = 0, IndexType nr_rows = 0);
    template <typename IterT>
    IndexType SetElems(IterT &pnts_start, const IterT &pnts_end,
                       IndexType first_row, size_t limit,
                       IndexType nr_elems, IndexType nr_rows,
                       SparsePartition::Builder *Bld);

    /**
     *  Printing methods.
     */
    void Print(std::ostream &out = std::cout);
    void PrintRows(std::ostream &out = std::cout);
    void PrintElems(std::ostream &out = std::cout);
    void PrintStats(std::ostream &out = std::cout);

    class PntIter;

    PntIter PointsBegin(IndexType ridx = 0);
    PntIter PointsEnd(IndexType ridx = 0);

    IndexType FindNewRowptrSize(IterOrder t);

    /**
     *  Transforms matrix to specific type.
     *
     *  @param type type of pattern to transform to.
     *  @param rs   line to start the transformation. If it is zero, it is the
     *              the beggining of matrix.
     *  @param re   line to end the transformation. If it is zero, it is the
     *              the end of matrix.
     */
    void Transform(IterOrder type, IndexType rs = 0, IndexType re = 0);

    /**
     *  Finds the reverse transform function. Transforms from <tt>type</tt> to
     *  <tt>HORIZONTAL</tt>.
     *
     *  @param type type of pattern to transform from.
     *  @return     reverse transform function.
     */
    typename TransformFnType<IndexType, ValueType>::TransformFn
    GetRevXformFn(IterOrder type);

    /**
     *  Finds the transform function. Transforms from HORIZONTAL to
     *  <tt>type</tt>.
     *
     *  @param type type of pattern to transform to.
     *  @return     transform function.
     */
    typename TransformFnType<IndexType, ValueType>::TransformFn
    GetXformFn(IterOrder type);

    /**
     *  Finds the complete transform function. Tranforms from <tt>from</tt> to
     *  <tt>to</tt>.
     *
     *  @param from type of pattern to transform from.
     *  @param to   type of pattern to transform to.
     *  @return     complete transform function.
     */
    typename TransformFnType<IndexType, ValueType>::TransformFn
    GetTransformFn(IterOrder from, IterOrder to);

    /**
     *  Extracts a window (sub-matrix) from this matrix. This methods copies the
     *  nonzero elements of the original matrix to the window.
     *
     *  @param rs     the row in this matrix, from where the window will be
     *                extracted.
     *  @param length number of rows to include in the window.
     *  @return       the selected window.
     *  @see GetWindow()
     */
    SparsePartition *ExtractWindow(IndexType rs, IndexType length);

    /**
     *  Extracts a window (sub-matrix) from this matrix using a backing
     *  store. This method uses the nonzero elements of this matrix without
     *  performing any copy.
     *
     *  @param rs     the row in this matrix, from where the window will be
     *                extracted.
     *  @param length number of rows to include in the window.
     *  @return       the selected window.
     *  @see ExtractWindow()
     */
    SparsePartition *GetWindow(IndexType rs, IndexType length);

    /**
     *  Puts a (possibly processed) window of the matrix back to this matrix.
     *
     *  @param window the (possibly processed) window.
     */
    void PutWindow(const SparsePartition *window);
 
private:
    void print_prod(size_t row_no)
    {
        for (IndexType i = rowptr_[row_no]; i < rowptr_[row_no+1]; ++i) {
            std::cout << "i = " << i << ", " << "elem = " << elems_[i] << "|";
        }
        std::cout << std::endl;
    }
};

/**
 *  Utility class for building a SparsePartition.
 */
template<typename IndexType, typename ValueType>
class SparsePartition<IndexType, ValueType>::Builder
{
public:
    /**
     *  Constructs a Builder object.
     *
     *  @param sp       the matrix to be built.
     *  @param nr_rows  an initial guess of the number of rows.
     *  @param nr_elems an initial guess of the nonzero elements.
     */
    Builder(SparsePartition *sp, IndexType nr_rows = 0, IndexType nr_elems = 0);
    ~Builder();

    /**
     *  Allocates a new element in the new matrix.
     *
     *  @return pointer to element allocated.
     */
    Elem<IndexType, ValueType> *AllocElem();

    /**
     *  Allocates a number of elements in the new matrix.
     *
     *  @param  nr number of elements to be allocated.
     *  @return    pointer to the first allocated element.
     */
    Elem<IndexType, ValueType> *AllocElems(size_t nr);

    /**
     *  Counts elements already allocated in the matrix.
     *
     *  @return number of elements allocated.
     */
    size_t GetElemsCnt()
    {
        return dynarray_size(da_elems_);
    }

    /**
     *  Allocate elements for the next row.
     *
     *  @param rdiff number of empty rows between the current and the new row.
     */
    void NewRow(IndexType rdiff = 1);   

    /**
     *  Finalizes the construction of the new matrix.
     */
    void Finalize();

public:
    SparsePartition *sp_;
    dynarray_t *da_elems_;
    dynarray_t *da_rowptr_;
    IndexType elems_cnt_;
};

#if 0
 
template<typename IndexType, typename ValueType>
class SparsePartition<IndexType, ValueType>::BuilderSym
{
public:
    /**
     *  Constructs a Builder object.
     *
     *  @param sp       the matrix to be built.
     *  @param nr_elems an initial guess of the nonzero elements.
     *  @param nr_rows  an initial guess of the number of rows.
     */
    BuilderSym(SparsePartition *sp, IndexType nr_rows = 0,
               IndexType nr_elems = 0)
    {
        IndexType *rowptr;
    
        if (sp_->elems_mapped_) {
            da_elems_ = dynarray_init_frombuff
                (sizeof(Elem<IndexType, ValueType>), sp_->elems_size_,
                 sp_->elems_, sp_->elems_size_);
            dynarray_seek(da_elems_, 0);
        } else {
            da_elems_ =
                dynarray_create(sizeof(Elem<IndexType, ValueType>), 512,
                                nr_elems);
        }
        da_rowptr_ = dynarray_create(sizeof(IndexType), 512, nr_rows);
        rowptr = (IndexType *) dynarray_alloc(da_rowptr_);
        *rowptr = 0;
    }

    virtual ~BuilderSym()
    {
        assert(da_elems_ == NULL && "da_elems_ not destroyed");
        assert(da_rowptr_ == NULL && "da_rowptr_ not destroyed");
    }

    /**
     *  Allocates a new element in the new matrix.
     *
     *  @return pointer to element allocated.
     */
    virtual Elem<IndexType, ValueType> *AllocElem()
    {
        if (sp_->elems_mapped_)
            assert(dynarray_size(da_elems_) < sp_->elems_size_ &&
                   "out of bounds");
        return (Elem<IndexType, ValueType> *) dynarray_alloc(da_elems_);
    }

    /**
     *  Allocates a number of elements in the new matrix.
     *
     *  @param  nr number of elements to be allocated.
     *  @return    pointer to the first allocated element.
     */
    virtual Elem<IndexType, ValueType> *AllocElems(size_t nr)
    {
        if (sp_->elems_mapped_)
            assert(dynarray_size(da_elems_) + nr < sp_->elems_size_ &&
                   "out of bounds");
        return (Elem<IndexType, ValueType> *) dynarray_alloc_nr(da_elems_, nr);
    }

    /**
     *  Counts elements already allocated in the matrix.
     *
     *  @return number of elements allocated.
     */
    virtual size_t GetElemsCnt()
    {
        return dynarray_size(da_elems_);
    }

    /**
     *  Allocate elements for the next row.
     *
     *  @param rdiff number of empty rows between the current and the new row.
     */
    virtual void NewRow(IndexType rdiff = 1)
    {
        IndexType *rowptr;
        size_t elems_cnt;

        elems_cnt = GetElemsCnt();
        rowptr = (IndexType *) dynarray_alloc_nr(da_rowptr_, rdiff);
        for (IndexType i = 0; i < rdiff; i++)
            rowptr[i] = elems_cnt;
    }

    /**
     *  Finalizes the construction of the new matrix.
     */
    virtual void Finalize()
    {
        IndexType *last_rowptr;

        last_rowptr = (IndexType *) dynarray_get_last(da_rowptr_);
        if (*last_rowptr != dynarray_size(da_elems_)) {
            NewRow();
        }

        if (!sp_->elems_mapped_ && sp_->elems_)
            free(sp_->elems_);

        if (sp_->rowptr_)
            free(sp_->rowptr_);

        if (sp_->elems_mapped_) {
            //assert(sp_->elems_size_ == dynarray_size(da_elems_));
        }
        sp_->elems_size_ = dynarray_size(da_elems_);
    
        if (sp_->elems_mapped_)
            free(da_elems_);
        else
            sp_->elems_ = (Elem<IndexType, ValueType> *)
                dynarray_destroy(da_elems_);

        sp_->rowptr_size_ = dynarray_size(da_rowptr_);
        sp_->rowptr_ = (IndexType *) dynarray_destroy(da_rowptr_);
        da_elems_ = da_rowptr_ = NULL;
    }

public:
    SparsePartition *sp_;
    dynarray_t *da_elems_;
    dynarray_t *da_rowptr_;
};

#endif  // SYM

/**
 *  Iterators for moving between elements inside the matrix.
 */
template<typename IndexType, typename ValueType>
class SparsePartition<IndexType, ValueType>::PntIter 
    : public std::iterator<std::forward_iterator_tag, Elem<IndexType,
                                                           ValueType> >
{
public:
    PntIter(): sp_(NULL), row_idx_(0), elem_idx_(0) { }
    PntIter(SparsePartition *sp, IndexType row_idx);

    bool operator==(const PntIter &pi);
    bool operator!=(const PntIter &pi);
    void operator++();
    Elem<IndexType, ValueType> operator*();
    std::ostream &operator<<(std::ostream &out);

private:
    SparsePartition *sp_;
    IndexType row_idx_;
    IndexType elem_idx_;
};

#if 0   // SYM

/**
 *  Internal representation of a sparse symmetric matrix. This class can also 
 *  represent a sub-matrix of a larger one.
 */
template<typename IndexType, typename ValueType>
class SparsePartitionSym
{
private:
    SparsePartition<IndexType, ValueType>  *lower_matrix_;   // Representation of lower matrix.
    ValueType                              *diagonal_;       // Values of diagonal elements.
    IndexType                               diagonal_size_;  // Size of the diagonal.
    SparsePartition<IndexType, ValueType>  *m1_;             /* Matrix that contains the elems of
                                                                SpmSym for which column of the element
                                                                is smaller than the row start. */
    SparsePartition<IndexType, ValueType>  *m2_;             /* Matrix that contains the rest of the
                                                                elements. */
    
public:
    SparsePartitionSym() 
        : lower_matrix_(NULL),
          diagonal_(NULL),
          m1_(NULL),
          m2_(NULL)
    {
        lower_matrix_ = new SparsePartition<IndexType, ValueType>;
        m1_ = new SparsePartition<IndexType, ValueType>;
        m2_ = new SparsePartition<IndexType, ValueType>;
    }

    ~SparsePartitionSym()
    {
        if (lower_matrix_)
            delete lower_matrix_;
        
        if (m1_)
            delete m1_;
        
        if (m2_)
            delete m2_;
        
        if (diagonal_)
            free(diagonal_);
    };
    
    SparsePartition<IndexType, ValueType> *GetLowerMatrix()
    {
        return lower_matrix_;
    }
    
    SparsePartition<IndexType, ValueType> *GetFirstMatrix()
    {
        return m1_;
    }
    
    SparsePartition<IndexType, ValueType> *GetSecondMatrix()
    {
        return m2_;
    }
    
    ValueType *GetDiagonal()
    {
        return diagonal_;
    }
    
    IndexType GetDiagonalSize()
    {
        return diagonal_size_;
    }
    
    void SetDiagonalSize(IndexType size)
    {
        diagonal_size_ = size;
    }
    
    void SetDiagonal(ValueType *diagonal)
    {
        diagonal_ = diagonal;
    }
    
    /**
     *  Loads matrix from a file specifying the number of threads.
     *
     *  @param filename name of the file that matrix is kept.
     *  @param in       buffer from which the matrix is taken.
     *  @param mmf      handler of MMF class.
     *  @param nr       number of threads to be used.
     *  @return         spmsym class object with the characteristics of the 
     *                  matrix.
     */
    static SparsePartitionSym *LoadMMF_mt(MMF<IndexType, ValueType> &mmf,
                                          const long nr);
    
    /**
     *  Divide the matrix (m) to two submatrices (m1 and m2).
     */
    void DivideMatrix();

    /**
     *  Merge the two submatrices (m1 and m2) into one matrix (m).
     */
    void MergeMatrix();
    
    class Builder;
    
    /**
     *  Function for filling the matrix using point iterators
     *
     *  @param pnts_start   point iterators start.
     *  @param pnts_end     point iterators end.
     *  @param first_row    first row of the matrix.
     *  @param limit        limit for the number of elements.
     *  @param nr_elems     size for elems allocation.
     *  @param nr_rows      size for rows allocation.
     *  @param spm_sym_bld  point to a builder responsible for allocations.
     *  @return             number of elements allocated.
     */
    template <typename IterT>
    IndexType SetElems(IterT &pnts_start, const IterT &pnts_end,
                       IndexType first_row, unsigned long limit = 0,
                       IndexType nr_elems = 0, IndexType nr_rows = 0);
    template <typename IterT>
    IndexType SetElems(IterT &pnts_start, const IterT &pnts_end,
                       IndexType first_row, unsigned long limit,
                       IndexType nr_elems, IndexType nr_rows,
                       SparsePartitionSym::Builder *spm_sym_bld);
                      
    void PrintDiagElems(std::ostream &out = std::cout);
};

template<typename IndexType, typename ValueType>
class SparsePartitionSym<IndexType, ValueType>::Builder
{
public:
    /**
     *  Constructs a Builder object.
     *
     *  @param spm_sym  the matrix to be built.
     *  @param nr_elems an initial guess of the nonzero elements.
     *  @param nr_rows  an initial guess of the number of rows.
     */
    Builder(SparsePartitionSym *spm_sym, IndexType nr_elems = 0,
            IndexType nr_rows = 0);
    virtual ~Builder();
    
    typename SparsePartition<IndexType, ValueType>::BuilderSym *GetLowerBuilder()
    {
        return spm_bld_;
    }
    
    ValueType *AllocDiagElem();
    IndexType GetDiagElemsCnt();
    IndexType GetElemsCnt();
    void Finalize();
    
private:
    SparsePartitionSym<IndexType, ValueType> *spm_sym_;
    typename SparsePartition<IndexType, ValueType>::BuilderSym *spm_bld_;
    dynarray_t *da_dvalues_;
};

#endif  // SYM

template<typename IndexType, typename ValueType>
void TestMMF(SparsePartition<IndexType, ValueType> *sp, const char *mmf_file);

/* Helper functions */
#define BLOCK_ROW_MAP_NAME(r)   pnt_map_bR ## r
#define BLOCK_ROW_RMAP_NAME(r)  pnt_rmap_bR ## r
#define BLOCK_COL_MAP_NAME(c)   pnt_map_bC ## c
#define BLOCK_COL_RMAP_NAME(c)  pnt_rmap_bC ## c

#define DEFINE_BLOCK_ROW_MAP_FN(r)                                      \
    template<typename IndexType, typename ValueType>                    \
    static inline void BLOCK_ROW_MAP_NAME(r)                            \
        (const CooElem<IndexType, ValueType> &src,                      \
         CooElem<IndexType, ValueType> &dst)                            \
    {                                                                   \
        IndexType src_x = src.col;                                      \
        IndexType src_y = src.row;                                      \
                                                                        \
        dst.row = (src_y - 1) / r + 1;                                  \
        dst.col = (src_y - 1) % r + r * (src_x - 1) + 1;                \
    }

#define DEFINE_BLOCK_ROW_RMAP_FN(r)                                     \
    template<typename IndexType, typename ValueType>                    \
    static inline void BLOCK_ROW_RMAP_NAME(r)                           \
        (const CooElem<IndexType, ValueType> &src,                      \
         CooElem<IndexType, ValueType> &dst)                            \
    {                                                                   \
        IndexType src_x = src.col;                                      \
        IndexType src_y = src.row;                                      \
                                                                        \
        dst.row = r * (src_y - 1) + (src_x - 1) % r + 1;                \
        dst.col = (src_x - 1) / r + 1;                                  \
    }

#define DEFINE_BLOCK_COL_MAP_FN(c)                                      \
    template<typename IndexType, typename ValueType>                    \
    static inline void BLOCK_COL_MAP_NAME(c)                            \
        (const CooElem<IndexType, ValueType> &src,                      \
         CooElem<IndexType, ValueType> &dst)                            \
    {                                                                   \
        pnt_map_V(src, dst);                                            \
        BLOCK_ROW_MAP_NAME(c)<IndexType, ValueType>(src, dst);          \
    }

#define DEFINE_BLOCK_COL_RMAP_FN(c)                                     \
    template<typename IndexType, typename ValueType>                    \
    static inline void BLOCK_COL_RMAP_NAME(c)                           \
        (const CooElem<IndexType, ValueType> &src,                      \
         CooElem<IndexType, ValueType> &dst)                            \
    {                                                                   \
        BLOCK_ROW_RMAP_NAME(c)<IndexType, ValueType>(src, dst);         \
        pnt_rmap_V(src, dst);                                           \
    }

template<typename IndexType, typename ValueType>
static inline void pnt_map_V(const CooElem<IndexType, ValueType> &src,
                             CooElem<IndexType, ValueType> &dst)
{
    IndexType src_x = src.col;
    IndexType src_y = src.row;

    dst.col = src_y;
    dst.row = src_x;
}

template<typename IndexType, typename ValueType>
static inline void pnt_rmap_V(const CooElem<IndexType, ValueType> &src,
                              CooElem<IndexType, ValueType> &dst)
{
    pnt_map_V(src, dst);
}

template<typename IndexType, typename ValueType>
static inline void pnt_map_D(const CooElem<IndexType, ValueType> &src,
                             CooElem<IndexType, ValueType> &dst, IndexType nrows)
{
    IndexType src_x = src.col;
    IndexType src_y = src.row;

    assert(nrows + src_x - src_y > 0);
    dst.row = nrows + src_x - src_y;
    dst.col = (src_x < src_y) ? src_x : src_y;
}

template<typename IndexType, typename ValueType>
static inline void pnt_rmap_D(const CooElem<IndexType, ValueType> &src,
                              CooElem<IndexType, ValueType> &dst,
                              IndexType nrows)
{
    IndexType src_x = src.col;
    IndexType src_y = src.row;

    if (src_y < nrows) {
        dst.col = src_x;
        dst.row = nrows + src_x - src_y;
    } else {
        dst.row = src_x;
        dst.col = src_y + src_x - nrows;
    }
}

template<typename IndexType, typename ValueType>
static inline void pnt_map_rD(const CooElem<IndexType, ValueType> &src,
                              CooElem<IndexType, ValueType> &dst,
                              IndexType ncols)
{
    IndexType src_x = src.col;
    IndexType src_y = src.row;
    IndexType dst_y;

    dst.row = dst_y = src_x + src_y - 1;
    dst.col = (dst_y <= ncols) ? src_y : ncols + 1 - src_x;
}

template<typename IndexType, typename ValueType>
static inline void pnt_rmap_rD(const CooElem<IndexType, ValueType> &src,
                               CooElem<IndexType, ValueType> &dst,
                               IndexType ncols)
{
    IndexType src_x = src.col;
    IndexType src_y = src.row;

    if (src_y <= ncols) {
        dst.row = src_x;
        dst.col = src_y + 1 - src_x;
    } else {
        dst.row = src_x + src_y - ncols;
        dst.col = ncols + 1 - src_x;
    }

    dst.row = src_y - dst.col + 1;
}

DEFINE_BLOCK_ROW_MAP_FN(1)
DEFINE_BLOCK_ROW_MAP_FN(2)
DEFINE_BLOCK_ROW_MAP_FN(3)
DEFINE_BLOCK_ROW_MAP_FN(4)
DEFINE_BLOCK_ROW_MAP_FN(5)
DEFINE_BLOCK_ROW_MAP_FN(6)
DEFINE_BLOCK_ROW_MAP_FN(7)
DEFINE_BLOCK_ROW_MAP_FN(8)

DEFINE_BLOCK_ROW_RMAP_FN(1)
DEFINE_BLOCK_ROW_RMAP_FN(2)
DEFINE_BLOCK_ROW_RMAP_FN(3)
DEFINE_BLOCK_ROW_RMAP_FN(4)
DEFINE_BLOCK_ROW_RMAP_FN(5)
DEFINE_BLOCK_ROW_RMAP_FN(6)
DEFINE_BLOCK_ROW_RMAP_FN(7)
DEFINE_BLOCK_ROW_RMAP_FN(8)

DEFINE_BLOCK_COL_MAP_FN(1)
DEFINE_BLOCK_COL_MAP_FN(2)
DEFINE_BLOCK_COL_MAP_FN(3)
DEFINE_BLOCK_COL_MAP_FN(4)
DEFINE_BLOCK_COL_MAP_FN(5)
DEFINE_BLOCK_COL_MAP_FN(6)
DEFINE_BLOCK_COL_MAP_FN(7)
DEFINE_BLOCK_COL_MAP_FN(8)

DEFINE_BLOCK_COL_RMAP_FN(1)
DEFINE_BLOCK_COL_RMAP_FN(2)
DEFINE_BLOCK_COL_RMAP_FN(3)
DEFINE_BLOCK_COL_RMAP_FN(4)
DEFINE_BLOCK_COL_RMAP_FN(5)
DEFINE_BLOCK_COL_RMAP_FN(6)
DEFINE_BLOCK_COL_RMAP_FN(7)
DEFINE_BLOCK_COL_RMAP_FN(8)

}   // end of namespace csx

using namespace csx;

/*
 *  SparsePartition class implementation
 */ 
template<typename IndexType, typename ValueType> template<typename IterT>
IndexType SparsePartition<IndexType, ValueType>::
SetRows(IterT &pi, const IterT &pnts_end, IndexType first_row, size_t limit,
        IndexType nr_rows)
{
    SparsePartition<IndexType, ValueType>::Builder *Bld = new
        SparsePartition<IndexType, ValueType>::Builder(this, nr_rows, 0);

    elems_size_ = SetRows(pi, pnts_end, first_row, limit, nr_rows, Bld);
    Bld->Finalize();
    delete Bld;
    return elems_size_;
}

template<typename IndexType, typename ValueType> template<typename IterT>
IndexType SparsePartition<IndexType, ValueType>::
SetRows(IterT &pi, const IterT &pnts_end, IndexType first_row, size_t limit,
        IndexType nr_rows, SparsePartition::Builder *Bld)
{
    IndexType row_prev, row;

    row_prev = first_row;
    for (; pi != pnts_end; ++pi) {
        row = (*pi).row;
        if (row != row_prev) {
            assert(row > row_prev);
            if (limit && Bld->elems_cnt_ >= limit)
                break;
            Bld->NewRow(row - row_prev);
            row_prev = row;
        }
        Bld->elems_cnt_++;
    }

    return Bld->elems_cnt_;
}

template<typename IndexType, typename ValueType> template<typename IterT>
IndexType SparsePartition<IndexType, ValueType>::
SetElems(IterT &pi, const IterT &pnts_end, IndexType first_row, size_t limit,
         IndexType nr_elems, IndexType nr_rows, SparsePartition::Builder *Bld)
{
    IndexType row_prev, row;
    //Elem<IndexType, ValueType> *elem;
    IndexType cnt;

    cnt = rowptr_[first_row - 1];
    row_prev = first_row;
    for (; pi != pnts_end; ++pi) {
        row = (*pi).row;
        if (row != row_prev) {
            assert(row > row_prev);
            if (limit && Bld->GetElemsCnt() >= limit)
                break;
            Bld->NewRow(row - row_prev);
            row_prev = row;
        }

        Bld->AllocElem();
        MakeRowElem(*pi, elems_ + cnt);
        //elem = Bld->AllocElem();
        //MakeRowElem<IndexType, ValueType>(*pi, elem);
        cnt++;
    }

    return elems_size_;
}

template<typename IndexType, typename ValueType> template<typename IterT>
IndexType SparsePartition<IndexType, ValueType>::
SetElems(IterT &pi, const IterT &pnts_end, IndexType first_row, size_t limit,
         IndexType nr_elems, IndexType nr_rows)
{
    SparsePartition<IndexType, ValueType>::Builder *Bld = new 
        SparsePartition<IndexType, ValueType>::Builder(this, nr_rows, nr_elems);

    SetElems(pi, pnts_end, first_row, limit, nr_elems, nr_rows, Bld);
    Bld->Finalize();
    delete Bld;

    return elems_size_;
}

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::Print(std::ostream &out)
{
    SparsePartition::PntIter p, p_start, p_end;

    p_start = PointsBegin();
    p_end = PointsEnd();
    for (p = p_start; p != p_end; ++p)
        out << " " << (*p);
        
    out << endl;
}

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::PrintRows(std::ostream &out) {
    cout << "Row Ptr: ";
    for (size_t i = 0; i < rowptr_size_; i++)
        cout << rowptr_[i] << " ";
    cout << endl;
}

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::PrintElems(std::ostream &out)
{
    SparsePartition<IndexType, ValueType>::PntIter p, p_start, p_end;
    static int cnt = 1;

    p_start = PointsBegin();
    p_end = PointsEnd();
    for (p = p_start; p != p_end; ++p) {
        if ((*p).pattern == NULL) {
            out << setiosflags(std::ios::scientific)
                << row_start_ + (*p).row << " "
                << (*p).col << " " << (*p).val << " cnt:" << cnt++ << "\n";
            continue;
        }

        boost::function<void (CooElem<IndexType, ValueType> &p)> xform_fn;
        boost::function<void (CooElem<IndexType, ValueType> &p)> rxform_fn;

        DeltaRLE *pat;
        CooElem<IndexType, ValueType> start;
        ValueType *vals;

        pat = (*p).pattern;
        start = static_cast<CooElem<IndexType, ValueType> >(*p);
        vals = start.vals;
        //out << SpmTypesNames[pat->GetType()] << std::endl;
        xform_fn = GetTransformFn(type_, pat->GetType());
        rxform_fn = GetTransformFn(pat->GetType(), type_);
        if (xform_fn)
            xform_fn(start);

        DeltaRLE::Generator *g = (*p).pattern->generator(start);

        while (!g->IsEmpty()) {
            CooElem<IndexType, ValueType> e = g->Next();
            if (rxform_fn)
                rxform_fn(e);
                
            out << setiosflags(std::ios::scientific) << row_start_ + e.row
                << " " << e.col << " " << *vals++ << " cnt:" << cnt++ << "\n";
        }

        out << "=== END OF PATTERN ===" << endl;
    }
}

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::PrintStats(std::ostream& out)
{
    IndexType nr_rows_with_patterns;
    IndexType nr_patterns, nr_patterns_before;
    IndexType nr_nzeros_block;
    IndexType nr_transitions;
    IndexType nr_xform_patterns[XFORM_MAX];

    nr_rows_with_patterns = GetNrRows();
    nr_patterns = 0;
    nr_nzeros_block = 0;
    nr_transitions = 0;
    memset(nr_xform_patterns, 0, sizeof(nr_xform_patterns));
    for (size_t i = 0; i < GetNrRows(); i++) {
        IndexType pt_size, pt_size_before;
        IterOrder pt_type, pt_type_before;

        nr_patterns_before = nr_patterns;

        const Elem<IndexType, ValueType> *elem = RowBegin(i);

        if (elem->pattern) {
            pt_size_before = pt_size = elem->pattern->GetSize();
            pt_type_before = pt_type = elem->pattern->GetType();
        } else {
            pt_size_before = pt_size = 0;
            pt_type_before = pt_type = NONE;
        }

        for (; elem != RowEnd(i); elem++) {
            if (elem->pattern) {
                ++nr_patterns;
                pt_size = elem->pattern->GetSize();
                pt_type = elem->pattern->GetType();
                nr_nzeros_block += pt_size;
                if (pt_type != pt_type_before ||
                    (pt_size_before && pt_size != pt_size_before))
                    ++nr_transitions;
                    
                ++nr_xform_patterns[elem->pattern->GetType()];
                pt_size_before = pt_size;
            } else {
                pt_type = NONE;
                ++nr_xform_patterns[NONE];
                if (pt_type != pt_type_before)
                    ++nr_transitions;
            }

            pt_type_before = pt_type;
        }

        if (nr_patterns == nr_patterns_before)
            --nr_rows_with_patterns;
    }

    int nr_encoded_types = 0;

    for (int i = 1; i < XFORM_MAX; i++)
        if (nr_xform_patterns[i]) {
            ++nr_encoded_types;
            out << SpmTypesNames[i] << ": "
            << nr_xform_patterns[i] << std::endl;
        }

    out << "Encoded types = " << nr_encoded_types << ", "
        << "Avg patterns/row = "
        << nr_patterns / (double) nr_rows_with_patterns << ", "
        << "Avg nonzeros/pattern = "
        << nr_nzeros_block / (double) nr_patterns << ", "
        << " Avg pattern transitions/row = "
        << nr_transitions / (double) nr_rows_with_patterns
        << endl;
}

template<typename IndexType, typename ValueType>
typename SparsePartition<IndexType, ValueType>::PntIter 
SparsePartition<IndexType, ValueType>::PointsBegin(IndexType ridx)
{
    return PntIter(this, ridx);
}

template<typename IndexType, typename ValueType>
typename SparsePartition<IndexType, ValueType>::PntIter 
SparsePartition<IndexType, ValueType>::PointsEnd(IndexType ridx)
{
    if (ridx == 0)
        ridx = GetNrRows();
        
    return PntIter(this, ridx);
}

template<typename IndexType, typename ValueType>
IndexType SparsePartition<IndexType, ValueType>::
FindNewRowptrSize(IterOrder t)
{
    if (t == HORIZONTAL)
        return nr_rows_ + 1;
    else if (t == VERTICAL)
        return nr_cols_ + 1;
    else if (t == DIAGONAL || REV_DIAGONAL)
        return nr_rows_ + nr_cols_ + 1;
    else if (is_row_block(t))
        return nr_rows_ / IsBlockType(t) + 2;
    else if (is_col_block(t))
        return nr_cols_ / IsBlockType(t) + 2;
    else
        return 0;
}

template<typename IndexType, typename ValueType>
inline typename TransformFnType<IndexType, ValueType>::TransformFn
SparsePartition<IndexType, ValueType>::
GetRevXformFn(IterOrder type)
{
    boost::function<void (CooElem<IndexType, ValueType> &p)> ret;

    switch(type) {
    case HORIZONTAL:
        break;
    case VERTICAL:
        ret = bll::bind(pnt_rmap_V<IndexType, ValueType>, bll::_1, bll::_1);
        break;
    case DIAGONAL:
        ret = bll::bind(pnt_rmap_D<IndexType, ValueType>, bll::_1, bll::_1,
                        nr_rows_);
        break;
    case REV_DIAGONAL:
        ret = bll::bind(pnt_rmap_rD<IndexType, ValueType>, bll::_1, bll::_1,
                        nr_cols_);
        break;
    case BLOCK_ROW_TYPE_NAME(1):
        ret = bll::bind(BLOCK_ROW_RMAP_NAME(1)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(2):
        ret = bll::bind(BLOCK_ROW_RMAP_NAME(2)<IndexType, ValueType>,   
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(3):
        ret = bll::bind(BLOCK_ROW_RMAP_NAME(3)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(4):
        ret = bll::bind(BLOCK_ROW_RMAP_NAME(4)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(5):
        ret = bll::bind(BLOCK_ROW_RMAP_NAME(5)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(6):
        ret = bll::bind(BLOCK_ROW_RMAP_NAME(6)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(7):
        ret = bll::bind(BLOCK_ROW_RMAP_NAME(7)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(8):
        ret = bll::bind(BLOCK_ROW_RMAP_NAME(8)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(1):
        ret = bll::bind(BLOCK_COL_RMAP_NAME(1)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(2):
        ret = bll::bind(BLOCK_COL_RMAP_NAME(2)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(3):
        ret = bll::bind(BLOCK_COL_RMAP_NAME(3)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(4):
        ret = bll::bind(BLOCK_COL_RMAP_NAME(4)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(5):
        ret = bll::bind(BLOCK_COL_RMAP_NAME(5)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(6):
        ret = bll::bind(BLOCK_COL_RMAP_NAME(6)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(7):
        ret = bll::bind(BLOCK_COL_RMAP_NAME(7)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(8):
        ret = bll::bind(BLOCK_COL_RMAP_NAME(8)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    default:
        cerr << "Unknown type: " << type << endl;
        assert(false);
    }    

    return ret;
}

template<typename IndexType, typename ValueType>
inline typename TransformFnType<IndexType, ValueType>::TransformFn
SparsePartition<IndexType, ValueType>::
GetXformFn(IterOrder type)
{
    boost::function<void (CooElem<IndexType, ValueType> &p)> ret;

    switch(type) {
    case HORIZONTAL:
        ret = NULL;
        break;
    case VERTICAL:
        ret = bll::bind(pnt_map_V<IndexType, ValueType>, bll::_1, bll::_1);
        break;
    case DIAGONAL:
        ret = bll::bind(pnt_map_D<IndexType, ValueType>, bll::_1, bll::_1,
                        nr_rows_);
        break;
    case REV_DIAGONAL:
        ret = bll::bind(pnt_map_rD<IndexType, ValueType>, bll::_1, bll::_1,
                        nr_cols_);
        break;
    case BLOCK_ROW_TYPE_NAME(1):
        ret = bll::bind(BLOCK_ROW_MAP_NAME(1)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(2):
        ret = bll::bind(BLOCK_ROW_MAP_NAME(2)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(3):
        ret = bll::bind(BLOCK_ROW_MAP_NAME(3)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(4):
        ret = bll::bind(BLOCK_ROW_MAP_NAME(4)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(5):
        ret = bll::bind(BLOCK_ROW_MAP_NAME(5)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(6):
        ret = bll::bind(BLOCK_ROW_MAP_NAME(6)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(7):
        ret = bll::bind(BLOCK_ROW_MAP_NAME(7)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_ROW_TYPE_NAME(8):
        ret = bll::bind(BLOCK_ROW_MAP_NAME(8)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(1):
        ret = bll::bind(BLOCK_COL_MAP_NAME(1)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(2):
        ret = bll::bind(BLOCK_COL_MAP_NAME(2)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(3):
        ret = bll::bind(BLOCK_COL_MAP_NAME(3)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(4):
        ret = bll::bind(BLOCK_COL_MAP_NAME(4)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(5):
        ret = bll::bind(BLOCK_COL_MAP_NAME(5)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(6):
        ret = bll::bind(BLOCK_COL_MAP_NAME(6)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(7):
        ret = bll::bind(BLOCK_COL_MAP_NAME(7)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    case BLOCK_COL_TYPE_NAME(8):
        ret = bll::bind(BLOCK_COL_MAP_NAME(8)<IndexType, ValueType>,
                        bll::_1, bll::_1);
        break;
    default:
        cerr << "Unknown type: " << type << endl;
        assert(false);
    }

    return ret;
}

template<typename IndexType, typename ValueType>
inline typename TransformFnType<IndexType, ValueType>::TransformFn
SparsePartition<IndexType, ValueType>::
GetTransformFn(IterOrder from_type, IterOrder to_type)
{
    boost::function<void (CooElem<IndexType, ValueType> &p)> xform_fn, rxform_fn;

    rxform_fn = GetRevXformFn(from_type);
    xform_fn = GetXformFn(to_type);
    if (xform_fn == NULL)
        return rxform_fn;
        
    if (rxform_fn != NULL)
        xform_fn = bll::bind(xform_fn,(bll::bind(rxform_fn, bll::_1), bll::_1));
        
    return xform_fn;
}

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::
Transform(IterOrder t, IndexType rs, IndexType re)
{
    PntIter p0, pe, p;
    vector<Elem<IndexType, ValueType> > elems;
    typename vector<Elem<IndexType, ValueType> >::iterator e0, ee, es;
    boost::function<void (CooElem<IndexType, ValueType> &p)> xform_fn;

    if (type_ == t)
        return;

    xform_fn = GetTransformFn(type_, t);
    elems.reserve(elems_size_);
   
    p0 = PointsBegin(rs);
    pe = PointsEnd(re); 
    for (p = p0; p != pe; ++p) {
        Elem<IndexType, ValueType> p_new = Elem<IndexType, ValueType>(*p);
        xform_fn(p_new);
        elems.push_back(p_new);
    }
 
    if (((type_ == HORIZONTAL || is_row_block(type_)) &&
         (t == HORIZONTAL || is_row_block(t))) ||
        ((type_ == VERTICAL || is_col_block(type_)) &&
         (t == VERTICAL || is_col_block(t)))) {
        int old_block_align, new_block_align, k;

        if (type_ == HORIZONTAL || type_ == VERTICAL)
            old_block_align = 1;
        else
            old_block_align = IsBlockType(type_);

        if (t == HORIZONTAL || t == VERTICAL)
            new_block_align = 1;
        else
            new_block_align = IsBlockType(t);

        k = lcm(old_block_align, new_block_align);
        k /= old_block_align;
        re = GetNrRows();
        
        e0 = elems.begin();
        ee = e0;
        for (size_t i = k; i < re; i += k) {
            es = ee;
            ee += rowptr_[i] - rowptr_[i-k];
            sort(es, ee, elem_cmp_less<IndexType, ValueType>);
        }
        es = ee;
        ee = elems.end();
        sort(es, ee, elem_cmp_less<IndexType, ValueType>);
    } else {
        e0 = elems.begin();
        ee = elems.end();
        sort(e0, ee, elem_cmp_less<IndexType, ValueType>);
    }

    SetElems(e0, ee, rs + 1, 0, elems_size_, FindNewRowptrSize(t));
    elems.clear();
    type_ = t;
}

template<typename IndexType, typename ValueType>
SparsePartition<IndexType, ValueType> *SparsePartition<IndexType, ValueType>::
GetWindow(IndexType rs, IndexType length)
{
    if (rs + length > rowptr_size_ - 1)
        length = rowptr_size_ - rs - 1;

    SparsePartition<IndexType, ValueType> *ret = new
        SparsePartition<IndexType, ValueType>();
    IndexType es = rowptr_[rs];
    IndexType ee = rowptr_[rs+length];

    ret->rowptr_ = (IndexType *) malloc((length+1) * sizeof(IndexType));
    ret->rowptr_size_ = length + 1;
    for (size_t i = 0; i < ret->rowptr_size_; i++)
        ret->rowptr_[i] = rowptr_[rs+i] - es;

    ret->elems_ = &elems_[es];
    ret->elems_size_ = ee - es;
    ret->nr_rows_ = ret->rowptr_size_ - 1;
    ret->nr_cols_ = nr_cols_;
    ret->nr_nzeros_ = ret->elems_size_;
    ret->row_start_ = row_start_ + rs;
    ret->type_ = type_;
    ret->elems_mapped_ = true;
    assert(ret->rowptr_[ret->rowptr_size_-1] == ret->elems_size_);

    return ret;
}

template<typename IndexType, typename ValueType>
SparsePartition<IndexType, ValueType> *SparsePartition<IndexType, ValueType>::
ExtractWindow(IndexType rs, IndexType length)
{
    vector<Elem<IndexType, ValueType> > elems;
    typename vector<Elem<IndexType, ValueType> >::iterator elem_begin, elem_end;
    PntIter p0, pe, p;
    SparsePartition<IndexType, ValueType> *ret = new
        SparsePartition<IndexType, ValueType>();

    if (rs + length > rowptr_size_ - 1)
        length = rowptr_size_ - rs - 1;

    IndexType es = rowptr_[rs];
    IndexType ee = rowptr_[rs+length];

    assert(es <= ee);
    elems.reserve(ee - es);
    p0 = PointsBegin(rs);
    pe = PointsEnd(rs + length - 1);
    for (p = p0; p != pe; ++p)
        elems.push_back(*p);

    elem_begin = elems.begin();
    elem_end = elems.end();
    ret->SetElems(elem_begin, elem_end, rs + 1, elems_size_);
    elems.clear();
    ret->nr_rows_ = ret->rowptr_size_ - 1;
    ret->nr_cols_ = nr_cols_;
    ret->nr_nzeros_ = ret->elems_size_;
    ret->row_start_ = row_start_ + rs;
    ret->type_ = type_;
    ret->elems_mapped_ = true;
    assert(ret->rowptr_[ret->rowptr_size_-1] == ret->elems_size_);
    
    return ret;
}

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::
PutWindow(const SparsePartition<IndexType, ValueType> *window)
{
    assert(window);
    assert(type_ == window->type_);

    IndexType rs = window->row_start_ - row_start_;
    IndexType es = rowptr_[rs];

    for (size_t i = 0; i < window->rowptr_size_; i++)
        rowptr_[rs+i] = es + window->rowptr_[i];

    if (!window->elems_mapped_)
        memcpy(&elems_[es], window->elems_,
               window->elems_size_*sizeof(Elem<IndexType, ValueType>));
}

/*
 * Builder class implemetation
 */
template<typename IndexType, typename ValueType>
SparsePartition<IndexType, ValueType>::
Builder::Builder(SparsePartition<IndexType, ValueType> *sp,
                 IndexType nr_rows, IndexType nr_elems)
    : sp_(sp), da_elems_(NULL),
      da_rowptr_(NULL),
      elems_cnt_(0)
{
    IndexType *rowptr;
    
    if (sp_->elems_mapped_) {
        da_elems_ = dynarray_init_frombuff
            (sizeof(Elem<IndexType, ValueType>), sp_->elems_size_,
             sp_->elems_, sp_->elems_size_);
        dynarray_seek(da_elems_, 0);
    }
    da_rowptr_ = dynarray_create(sizeof(IndexType), 512, nr_rows);
    rowptr = (IndexType *) dynarray_alloc(da_rowptr_);
    *rowptr = 0;
}

template<typename IndexType, typename ValueType>
SparsePartition<IndexType, ValueType>::
Builder::~Builder()
{
    //assert(da_elems_ == NULL && "da_elems_ not destroyed");
    assert(da_rowptr_ == NULL && "da_rowptr_ not destroyed");
}

template<typename IndexType, typename ValueType>
Elem<IndexType, ValueType> *SparsePartition<IndexType, ValueType>::
Builder::AllocElem()
{
    if (sp_->elems_mapped_)
        assert(dynarray_size(da_elems_) < sp_->elems_size_ &&
               "out of bounds");
    return (Elem<IndexType, ValueType> *) dynarray_alloc(da_elems_);
}

template<typename IndexType, typename ValueType>
Elem<IndexType, ValueType> *SparsePartition<IndexType, ValueType>::
Builder::AllocElems(size_t nr)
{
    if (sp_->elems_mapped_)
        assert(dynarray_size(da_elems_) + nr < sp_->elems_size_ &&
               "out of bounds");    
    return (Elem<IndexType, ValueType> *) dynarray_alloc_nr(da_elems_, nr);
}

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::
Builder::NewRow(IndexType rdiff)
{
    IndexType *rowptr;
        
    if (sp_->elems_mapped_)
        elems_cnt_ = GetElemsCnt();
    rowptr = (IndexType *) dynarray_alloc_nr(da_rowptr_, rdiff);
    for (IndexType i = 0; i < rdiff; i++)
        rowptr[i] = elems_cnt_;
}  

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::
Builder::Finalize()
{
    IndexType *last_rowptr;

    last_rowptr = (IndexType *) dynarray_get_last(da_rowptr_);
    if (sp_->elems_mapped_) {
        if (*last_rowptr != dynarray_size(da_elems_))
            NewRow();
    } else {
        if (*last_rowptr != elems_cnt_)
            NewRow();
    }

    if (sp_->rowptr_)
        free(sp_->rowptr_);

    if (sp_->elems_mapped_) {
        //assert(sp_->elems_size_ == dynarray_size(da_elems_));
        sp_->elems_size_ = dynarray_size(da_elems_);
    } else {
        sp_->elems_size_ = elems_cnt_;
    }

    if (sp_->elems_mapped_)
        free(da_elems_);

    sp_->rowptr_size_ = dynarray_size(da_rowptr_);
    sp_->rowptr_ = (IndexType *) dynarray_destroy(da_rowptr_);
    da_elems_ = da_rowptr_ = NULL;
}

/* 
 * PntIter class implemetation
 */
template<typename IndexType, typename ValueType>
SparsePartition<IndexType, ValueType>::PntIter::PntIter(SparsePartition *sp,
                                                        IndexType r_idx)
    : sp_(sp),    
      row_idx_(r_idx)
{
    IndexType *rp = sp_->rowptr_;
    size_t rp_size = sp_->rowptr_size_;
    
    assert(r_idx < rp_size);
    while (r_idx + 1 < rp_size && rp[r_idx] == rp[r_idx+1])
        r_idx++;
        
    row_idx_ = r_idx;
    elem_idx_ = rp[r_idx];
}

template<typename IndexType, typename ValueType>
bool SparsePartition<IndexType, ValueType>::
PntIter::operator==(const PntIter &pi)
{
    return (sp_ = pi.sp_) && (row_idx_ == pi.row_idx_) &&
        (elem_idx_ == pi.elem_idx_);
}

template<typename IndexType, typename ValueType>
bool SparsePartition<IndexType, ValueType>::
PntIter::operator!=(const PntIter &pi)
{
    return !(*this == pi);
}

template<typename IndexType, typename ValueType>
void SparsePartition<IndexType, ValueType>::PntIter::operator++()
{
    IndexType *rp = sp_->rowptr_;
    size_t rp_size = sp_->rowptr_size_;
    
    assert(elem_idx_ < sp_->elems_size_);
    assert(row_idx_ < rp_size);
    elem_idx_++;
    while (row_idx_ + 1 < rp_size && rp[row_idx_+1] == elem_idx_)
        row_idx_++;
}

template<typename IndexType, typename ValueType>
Elem<IndexType, ValueType> SparsePartition<IndexType, ValueType>::
PntIter::operator*()
{
    Elem<IndexType, ValueType> ret;
    Elem<IndexType, ValueType> *e;
    DeltaRLE *p;
  
    ret.row = row_idx_ + 1;
    e = sp_->elems_ + elem_idx_;
    ret.col = e->col;
    ret.val = e->val;
    p = e->pattern;
    ret.pattern = (p == NULL) ? NULL : p->Clone();
    ret.in_pattern = e->in_pattern;
    ret.pattern_start = e->pattern_start; 
    if (p != NULL)
        delete p;
        
    return ret;
}

template<typename IndexType, typename ValueType>
std::ostream& SparsePartition<IndexType, ValueType>::
PntIter::operator<<(std::ostream &out)
{
    out << "<" << std::setw(2) << row_idx_ << "," << std::setw(2)
        << elem_idx_ << ">";
    return out;
}

#if 0   // SYM

/*
 * SparsePartitionSym class implementation
 */
template<typename IndexType, typename ValueType>
void SparsePartitionSym<IndexType, ValueType>::DivideMatrix()
{
    SparsePartition<IndexType, ValueType> *matrix = lower_matrix_;
    //uint64_t *rowptr = matrix->GetRowPtr();
    //Elem<IndexType, ValueType> *elems = matrix->GetElems();
    IndexType nr_nzeros = matrix->GetNrNonzeros();
    IndexType row_start = matrix->GetRowStart();
    IndexType nr_rows = matrix->GetNrRows();
    IndexType nr_cols = matrix->GetNrCols();
    IndexType rows1 = 0;
    IndexType rows2 = 0;
    
    typename SparsePartition<IndexType, ValueType>::Builder *spmbld1 = new
        typename SparsePartition<IndexType, ValueType>::Builder(m1_, nr_rows + 1,
                                                                nr_nzeros);
    typename SparsePartition<IndexType, ValueType>::Builder *spmbld2 = new 
        typename SparsePartition<IndexType, ValueType>::Builder(m2_, nr_rows + 1,
                                                                nr_nzeros);
    
    //Elem<IndexType, ValueType> *elem;
    Elem<IndexType, ValueType> elem;
    
    m1_->SetType(HORIZONTAL);
    m1_->SetRowStart(row_start);
    m1_->SetNrCols(nr_cols);
    m1_->SetNrNonzeros(0);

    m2_->SetType(HORIZONTAL);
    m2_->SetRowStart(row_start);
    m2_->SetNrCols(nr_cols);
    m2_->SetNrNonzeros(0);
    
    for (size_t i = 0; i < nr_rows; i++) {
        for (size_t j = matrix->GetRow(i); j < matrix->GetRow(i+1); j++) {
            //elem = elems + j;
            elem = matrix->operator[](j);
            if (elem.col < row_start + 1) {
                if (rows1 < i) {
                    spmbld1->NewRow(i - rows1);
                    rows1 = i;
                }
                m1_->SetNrNonzeros(m1_->GetNrNonzeros() + 1);
                MakeRowElem(elem, spmbld1->AllocElem());
            } else {
                if (rows2 < i) {
                    spmbld2->NewRow(i - rows2);
                    rows2 = i;
                }
                m2_->SetNrNonzeros(m2_->GetNrNonzeros() + 1);
                MakeRowElem(elem, spmbld2->AllocElem());
            }
        }
    }
    
    spmbld1->Finalize();
    spmbld2->Finalize();
    
    m1_->SetNrRows(m1_->GetNrRows());
    m2_->SetNrRows(m2_->GetNrRows());
    
    delete spmbld1;
    delete spmbld2;
}

template<typename IndexType, typename ValueType>
void SparsePartitionSym<IndexType, ValueType>::MergeMatrix()
{
    SparsePartition<IndexType, ValueType> *m1 = GetFirstMatrix();
    SparsePartition<IndexType, ValueType> *m2 = GetSecondMatrix();
    SparsePartition<IndexType, ValueType> *temp = new
        SparsePartition<IndexType, ValueType>();
    
    SparsePartition<IndexType, ValueType> *matrix = lower_matrix_;
    //uint64_t *rowptr1 = m1->GetRowPtr();
    //uint64_t *rowptr2 = m2->GetRowPtr();
    //Elem<IndexType, ValueType> *elems1 = m1->GetElems();
    //Elem<IndexType, ValueType> *elems2 = m2->GetElems();
    IndexType row_start = matrix->GetRowStart();
    IndexType nr_rows = matrix->GetNrRows();
    IndexType nr_cols = matrix->GetNrCols();
    IndexType nr_nzeros = matrix->GetNrNonzeros();
    
    typename SparsePartition<IndexType, ValueType>::Builder *spmbld = new
        typename SparsePartition<IndexType, ValueType>::Builder(temp, nr_rows + 1,
                                                                nr_nzeros);

    //Elem<IndexType, ValueType> *elem;
    Elem<IndexType, ValueType> elem;
    
    temp->SetType(HORIZONTAL);
    temp->SetRowStart(row_start);
    temp->SetNrCols(nr_cols);
    temp->SetNrNonzeros(matrix->GetNrNonzeros());
    
    for (size_t i = 0; i < nr_rows; i++) {
        if (m1_->GetNrRows() > i) {
            for (size_t j = m1_->GetRow(i); j < m1_->GetRow(i+1); j++) {
                //elem = elems1 + j;
                elem = m1->operator[](j);
                MakeRowElem(elem, spmbld->AllocElem());
            }
        }
        if (m2_->GetNrRows() > i) {
            for (size_t j = m2_->GetRow(i); j < m2_->GetRow(i+1); j++) {
                //elem = elems2 + j;
                elem = m2->operator[](j);
                MakeRowElem(elem, spmbld->AllocElem());
            }
        }
        spmbld->NewRow();
    }
    
    spmbld->Finalize();
    
    temp->SetNrRows(temp->GetNrRows());
    
    delete spmbld;
    
    delete lower_matrix_;
    lower_matrix_ = temp;
    
    delete m1_;
    delete m2_;
}

template<typename IndexType, typename ValueType>
void SparsePartitionSym<IndexType, ValueType>::PrintDiagElems(std::ostream &out)
{
    IndexType row_start = lower_matrix_->GetRowStart();
    
    for (size_t i = 0; i < diagonal_size_; i++)
        std::cout << row_start + i + 1 << " " << row_start + i + 1 << " "
                  << diagonal_[i] << " cnt:" << i << std::endl;
}

template<typename IndexType, typename ValueType>
SparsePartitionSym<IndexType, ValueType>::Builder::
Builder(SparsePartitionSym *spm_sym, IndexType nr_elems, IndexType nr_rows)
  : spm_sym_(spm_sym)
{
    SparsePartition<IndexType, ValueType> *spm = spm_sym_->GetLowerMatrix();
    
    spm_bld_ = new typename SparsePartition<IndexType, ValueType>::BuilderSym
        (spm, nr_rows, nr_elems);
    da_dvalues_ = dynarray_create(sizeof(ValueType), 512, nr_rows);
}

template<typename IndexType, typename ValueType>
SparsePartitionSym<IndexType, ValueType>::Builder::~Builder()
{
    delete spm_bld_;
    assert(da_dvalues_ == NULL && "da_dvalues_ not destroyed");
}

template<typename IndexType, typename ValueType>
ValueType *SparsePartitionSym<IndexType, ValueType>::Builder::AllocDiagElem()
{
    // assert(dynarray_size(da_dvalues_) <
    //       spm_sym_->GetLowerMatrix()->GetNrRows() && "out of bounds");

    return (ValueType *) dynarray_alloc(da_dvalues_);
}

template<typename IndexType, typename ValueType>
IndexType SparsePartitionSym<IndexType, ValueType>::Builder::GetDiagElemsCnt()
{
    return dynarray_size(da_dvalues_);
}

template<typename IndexType, typename ValueType>
IndexType SparsePartitionSym<IndexType, ValueType>::Builder::GetElemsCnt()
{
    return GetDiagElemsCnt() + spm_bld_->GetElemsCnt();
}

template<typename IndexType, typename ValueType>
void SparsePartitionSym<IndexType, ValueType>::Builder::Finalize()
{
    spm_bld_->Finalize();
    spm_sym_->SetDiagonalSize(GetDiagElemsCnt());
    spm_sym_->SetDiagonal((ValueType *) dynarray_destroy(da_dvalues_));
    da_dvalues_ = NULL;
}

template<typename IndexType, typename ValueType>
template<typename IterT>
IndexType SparsePartitionSym<IndexType, ValueType>::
SetElems(IterT &pi, const IterT &pnts_end, IndexType first_row,
         unsigned long limit, IndexType nr_elems, IndexType nr_rows,
         SparsePartitionSym::Builder *spm_sym_bld)
{
    ValueType *value;
    Elem<IndexType, ValueType> *elem;
    IndexType row_prev, row, col;

    row_prev = first_row;
    for (; pi != pnts_end; ++pi) {
        row = (*pi).row;
        col = (*pi).col;
    
        if (row > col) {
            if (row != row_prev) {;
                assert(row > row_prev);
                if (limit && spm_sym_bld->GetElemsCnt() >= limit && 
                    row_prev == row - 1)
                    break;
                spm_sym_bld->GetLowerBuilder()->NewRow(row - row_prev);
                row_prev = row;
            }
            elem = spm_sym_bld->GetLowerBuilder()->AllocElem();
            MakeRowElem(*pi, elem);
        } else if (row == col) {
            value = spm_sym_bld->AllocDiagElem();
            *value = (*pi).val;
        }
    }
    return lower_matrix_->GetElemsSize() + GetDiagonalSize();
}

template<typename IndexType, typename ValueType>
template<typename IterT>
IndexType SparsePartitionSym<IndexType, ValueType>::
SetElems(IterT &pi, const IterT &pnts_end, IndexType first_row,
         unsigned long limit, IndexType nr_elems, IndexType nr_rows)
{
    SparsePartitionSym::Builder *spm_sym_bld = new SparsePartitionSym::Builder
        (this, nr_elems, nr_rows);

    SetElems(pi, pnts_end, first_row, limit, nr_elems, nr_rows, spm_sym_bld);
    spm_sym_bld->Finalize();
    delete spm_sym_bld;
    return lower_matrix_->GetElemsSize() + GetDiagonalSize();
}

template<typename IndexType, typename ValueType>
SparsePartitionSym<IndexType, ValueType> *
SparsePartitionSym<IndexType, ValueType>::
LoadMMF_mt(MMF<IndexType, ValueType>& mmf, const long nr)
{
    SparsePartitionSym<IndexType, ValueType> *ret, *spm_sym;
    long limit, cnt, row_start, nr_nzeros, n, nnz;
    typename MMF<IndexType, ValueType>::iterator iter = mmf.begin();
    typename MMF<IndexType, ValueType>::iterator iter_end = mmf.end();

    assert(mmf.GetNrRows() == mmf.GetNrCols());
    nr_nzeros = (mmf.GetNrNonzeros() + mmf.GetNrCols()) / 2;
    n = mmf.GetNrCols();
    ret = new SparsePartitionSym<IndexType, ValueType>[nr];
    row_start = limit = cnt = 0;
    for (long i = 0; i < nr; ++i) {
        spm_sym = ret + i;
        limit = (nr_nzeros - cnt) / (nr - i);
        nnz = spm_sym->SetElems(iter, iter_end, row_start + 1, limit,
                                limit + 2 * mmf.GetNrRows() - 1,
                                mmf.GetNrRows() + 1);
        spm_sym->lower_matrix_->SetNrNonzeros(nnz - spm_sym->GetDiagonalSize());
        spm_sym->lower_matrix_->SetNrRows(spm_sym->lower_matrix_->GetNrRows());
        spm_sym->lower_matrix_->SetNrCols(n);
        spm_sym->lower_matrix_->SetRowStart(row_start);
        row_start += spm_sym->GetDiagonalSize();
        spm_sym->lower_matrix_->SetType(HORIZONTAL);
        cnt += nnz;
    }

    assert((IndexType) cnt == (IndexType) nr_nzeros);
    return ret;
}

#endif  // SYM

#endif  // SPARSEPARTITION_H__

// vim:expandtab:tabstop=8:shiftwidth=4:softtabstop=4