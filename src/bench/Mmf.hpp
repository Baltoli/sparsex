/*
 * Copyright (C) 2013-2014, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2013-2014, Athena Elafrou
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */

/**
 * \file Mmf.hpp
 * \brief Matrix Market Format routines
 *
 * \author Athena Elafrou
 * \date 2013&ndash;2014
 * \copyright This file is distributed under the BSD License. See LICENSE.txt
 * for details.
 */

#ifndef MMF_HPP
#define MMF_HPP

#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <algorithm>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/unordered_map.hpp>

using namespace std;

template<typename IndexType, typename ValueType>
struct CooElem {
    IndexType row;        ///< the row index
    IndexType col;        ///< the column index
    union {
        ValueType val;   ///< the value of the element
        ValueType *vals; ///< the value of the elements, if Elem refers to
                         ///  an encoded pattern
    };
};

template<typename IndexType, typename ValueType>
struct CooElemSorter {
    public:
        bool operator() (const CooElem<IndexType, ValueType> &lhs,
                         const CooElem<IndexType, ValueType> &rhs) const
        {
            if (lhs.row < rhs.row) return true;
            if (lhs.row > rhs.row) return false;

            if (lhs.col < rhs.col) return true;
            if (lhs.col > rhs.col) return false;

            return false;
        }
};

bool DoRead(ifstream &in, vector<string> &arguments);
template<typename IndexType, typename ValueType>
void ParseElement(vector<string> &arguments, IndexType &y, IndexType &x, 
                  ValueType &v);

template<typename IndexType, typename ValueType>
class MMF
{
public:
    typedef IndexType idx_t;
    typedef ValueType val_t;

    MMF(const char *filename);
    MMF(IndexType *rowptr, IndexType *colind, ValueType *values,
        IndexType nr_rows, IndexType nr_cols, bool zero_based) {} //Dummy
    ~MMF()
    {
        if (in.is_open())
            in.close();
    }

    IndexType GetNrRows() const 
    {
        return nr_rows_;
    }

    IndexType GetNrCols() const
    {
        return nr_cols_;
    }

    IndexType GetNrNonzeros() const
    {
        if (symmetric_ || col_wise_)
            return matrix_.size();
        else 
            return nr_nzeros_;
    }

    bool IsSymmetric() const
    {
        return symmetric_;
    }

    bool IsColWise() const
    {
        return col_wise_;
    }
    
    bool IsZeroBased() const
    {
        return zero_based_;
    }

    void GetCoordinates(IndexType idx, IndexType &row, IndexType &col)
    {
        row = matrix_[idx].row;
        col = matrix_[idx].col;
    }

    void SetCoordinates(IndexType idx, IndexType row, IndexType col)
    {
        matrix_[idx].row = row;
        matrix_[idx].col = col;
    }

    void Sort()
    {
        sort(matrix_.begin(), matrix_.end(),
             CooElemSorter<IndexType, ValueType>());
    }

    void InitMatrix(size_t size)
    {
        matrix_.reserve(size);
    }

    void InitStream();
    
    void InsertElement(CooElem<IndexType, ValueType> elem)
    {
        matrix_.push_back(elem);
    }

    class iterator;
    iterator begin();
    iterator end();

private:
    IndexType nr_rows_, nr_cols_, nr_nzeros_;
    ifstream in;
    bool symmetric_, col_wise_, zero_based_;
    int file_mode_;     // 0 for MMF files, 1 for regular files
    vector<CooElem<IndexType, ValueType> > matrix_;

    enum MmfInfo {
        Banner,
        Matrix,
        Coordinate,
        Real,
        Double,
        Integer,
        General,
        Symmetric,
        Indexing0,
        Indexing1,
        ColumnWise,
        RowWise
    };

    static boost::unordered_map<MmfInfo, const string> names_;

    void ParseMmfHeaderLine(vector<string> &arguments);
    void ParseMmfSizeLine(vector<string> &arguments); 
    void DoLoadMmfMatrix();
    bool GetNext(IndexType &y, IndexType &x, ValueType &val);
};

template<typename IndexType, typename ValueType>
class MMF<IndexType, ValueType>::iterator 
    : public std::iterator<forward_iterator_tag, CooElem<IndexType, ValueType> >
{
public:
    iterator() {}
    
    iterator(MMF *mmf, size_t cnt)
      :
        mmf_(mmf),
        cnt_(cnt)
    {
        if (mmf_->symmetric_ || mmf_->col_wise_)
            return;

        // this is the initializer
        if (cnt_ == 0) {
            this->DoSet();
        }
    }
    
    bool operator==(const iterator &i)
    {
        //cout << "me: " << mmf_ << " " << cnt_
        //     << " i: " << i.mmf << " " << i.cnt << "\n";
        return (mmf_ == i.mmf_) && (cnt_ == i.cnt_);
    }

    bool operator!=(const iterator &i)
    {
        return !(*this == i);
    }

    void operator++()
    {
        ++cnt_;
        if (mmf_->symmetric_ || mmf_->col_wise_) {
            return;
        }
        this->DoSet();
    }

    CooElem<IndexType, ValueType> &operator*()
    {
        if (mmf_->symmetric_ || mmf_->col_wise_) {
            return mmf_->matrix_[cnt_];
        } else {
            if (!valid_) {
                cout << "Requesting dereference, but mmf ended\n";
                exit(1);
            }
            assert(valid_);
            return elem_;
        }
    }

private:
    void DoSet()
    {
        valid_ = mmf_->GetNext(elem_.row, elem_.col, elem_.val);
    }

    MMF *mmf_;
    size_t cnt_;
    CooElem<IndexType, ValueType> elem_;
    bool valid_;
};

template<typename IndexType, typename ValueType>
typename MMF<IndexType, ValueType>::iterator MMF<IndexType, ValueType>::begin()
{
    return iterator(this, 0);
}

template<typename IndexType, typename ValueType>
typename MMF<IndexType, ValueType>::iterator MMF<IndexType, ValueType>::end()
{
    if (this->symmetric_ || this->col_wise_) {
        return iterator(this, matrix_.size());
    } else {
        return iterator(this, nr_nzeros_);
    }
}

/*
 * Implementation of class MMF
 */
template<typename IndexType, typename ValueType>
boost::unordered_map<typename MMF<IndexType, ValueType>::MmfInfo, const string>
MMF<IndexType, ValueType>::names_ =
                     boost::assign::map_list_of 
                     (Banner, "%%MatrixMarket")
                     (Matrix, "matrix")
                     (Coordinate, "coordinate")
                     (Real, "real")
                     (Double, "double")
                     (Integer, "integer") 
                     (General, "general")
                     (Symmetric, "symmetric")
                     (Indexing0, "0-base")
                     (Indexing1, "1-base")
                     (ColumnWise, "column")
                     (RowWise, "row");

template<typename IndexType, typename ValueType>
MMF<IndexType, ValueType>::MMF(const char* filename)
  : 
    nr_rows_(0),
    nr_cols_(0),
    nr_nzeros_(0),
    symmetric_(false), 
    col_wise_(true),
    zero_based_(false),
    file_mode_(0)
{
    try {
        in.open(filename);
        if (!in.is_open()) {
            throw ios_base::failure("");
        }
    } catch (ios_base::failure &e) {
        cout << "MMF file error.\n";
        exit(1);
    }
    vector<string> arguments;

    DoRead(in, arguments);
    ParseMmfHeaderLine(arguments);
    ParseMmfSizeLine(arguments);

    if (symmetric_ || col_wise_) {
        DoLoadMmfMatrix();
    }
}

template<typename IndexType, typename ValueType>
void MMF<IndexType, ValueType>::ParseMmfHeaderLine(vector<string> &arguments)
{
    // Check if header line exists
    if (arguments[0] != names_[Banner]) {
        if (arguments[0].length() > 2 && arguments[0][0] == '%'&&
            arguments[0][1] == '%') {
            // Header exists but is erroneous so exit
            cout << "invalid header line in MMF file.\n";
            exit(1);
        } else {
            // Parse as size line
            file_mode_ = 1;
            return;
        }        
    }

    size_t length;
    if ((length = arguments.size()) < 5) {
        cout << "less arguments in header line of MMF file.\n";
        exit(1);
    }

    // Convert to lowercase just in case
    BOOST_FOREACH(string &t, arguments) {
        boost::algorithm::to_lower(t);
    }

    if (arguments[1] != names_[Matrix]) {
        cout << "unsupported object in header line of MMF file.\n";
        exit(1);
    }

    if (arguments[2] != names_[Coordinate]) {
        cout << "unsupported matrix format in header line of MMF file.\n";
        exit(1);
    }

    if (arguments[4] == names_[General]) {
        symmetric_ = false;
    } else if (arguments[4] == names_[Symmetric]) {
        symmetric_ = true;
    } else {
        cout << "unsupported symmetry in header line of MMF file.\n";
        exit(1);
    }
    
    if (length > 5) {
        for (size_t i = 5; i < length; i++) {
            if (arguments[i] == names_[Indexing0]) zero_based_ = true;
            else if (arguments[i] == names_[Indexing1]) zero_based_ = false;
            else if (arguments[i] == names_[ColumnWise]) col_wise_ = true;
            else if (arguments[i] == names_[RowWise]) col_wise_ = false;
        }
    }
}

template<typename IndexType, typename ValueType>
void MMF<IndexType, ValueType>::ParseMmfSizeLine(vector<string> &arguments)
{
    bool ignore_comments = false;

    if (file_mode_ && arguments[0][0] == '%') {
        ignore_comments = true;
    }

    if (!file_mode_ || ignore_comments) {
        while (in.peek() == '%') {
            in.ignore(numeric_limits<streamsize>::max(), '\n');
        }
        if (!DoRead(in, arguments)) {
            cout << "size line error in MMF file.\n";
            exit(1);
        }
    }
    ParseElement(arguments, nr_rows_, nr_cols_, nr_nzeros_);
}

template<typename IndexType, typename ValueType>
void MMF<IndexType, ValueType>::DoLoadMmfMatrix()
{
    CooElem<IndexType, ValueType> elem;
    IndexType tmp;

    if (symmetric_) {
        matrix_.reserve(nr_nzeros_ << 1);
        for (IndexType i = 0; i < nr_nzeros_; i++) {
            if (!MMF::GetNext(elem.row, elem.col, elem.val)) {
                cout << "Requesting dereference, but mmf ended.\n";
                exit(1);
            }
            matrix_.push_back(elem);
            if (elem.row != elem.col) {
                tmp = elem.row;
                elem.row = elem.col;
                elem.col = tmp;
                matrix_.push_back(elem);          
            }
        }
    } else {
        matrix_.reserve(nr_nzeros_);
        for (IndexType i = 0; i < nr_nzeros_; i++) {
            if (!MMF::GetNext(elem.row, elem.col, elem.val)) {
                cout << "Requesting dereference, but mmf ended.\n";
                exit(1);
            }
            matrix_.push_back(elem);
        }
    }

    sort(matrix_.begin(), matrix_.end(), CooElemSorter<IndexType, ValueType>());
}

template<typename IndexType, typename ValueType>
bool MMF<IndexType, ValueType>::GetNext(IndexType &y, IndexType &x, ValueType &v)
{
    vector<string> arguments;

    if (!DoRead(in, arguments)) {
        return false;
    }

    ParseElement(arguments, y, x, v);

    if (zero_based_) {
        y++;
        x++;
    }

    return true;
}

template<typename IndexType, typename ValueType>
void MMF<IndexType, ValueType>::InitStream()
{
    in.clear();
    in.seekg(0, ios::beg);
    while (in.peek() == '%') {
        in.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    in.ignore(numeric_limits<streamsize>::max(), '\n');
}

// Returns false at EOF
bool DoRead(ifstream &in, vector<string> &arguments)
{
    string buff;

    try {
        if (getline(in, buff).eof()) {
            return false;
        }
    } catch (ios_base::failure &e) {
        cout << "error reading from MMF file:" + (string) e.what() + "\n";
        exit(1);
    }

    buff = boost::trim_left_copy(buff);
    buff = boost::trim_right_copy(buff);
    boost::split(arguments, buff, boost::algorithm::is_any_of(" "),
        boost::algorithm::token_compress_on);
    
    return true;
}

template<typename IndexType, typename ValueType>
void ParseElement(vector<string> &arguments, IndexType &y, IndexType &x, 
                  ValueType &v)
{
    if (arguments.size() == 3) {
        y = boost::lexical_cast<IndexType,string>(arguments[0]);
        x = boost::lexical_cast<IndexType,string>(arguments[1]);
        v = boost::lexical_cast<ValueType,string>(arguments[2]);
    } else {
        cout << "bad input, less arguments in line of MMF file.\n";
        exit(1);
    }
}

#endif  // MMF_HPP

// vim:expandtab:tabstop=8:shiftwidth=4:softtabstop=4
