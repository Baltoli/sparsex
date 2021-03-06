/*
 * Copyright (C) 2013, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2013, Vasileios Karakasis
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */

/**
 * \file DynamicArray.hpp
 * \brief Dynamic Array implementation supporting efficient resizing
 *
 * \author Computing Systems Laboratory (CSLab), NTUA
 * \date 2011&ndash;2014
 * \copyright This file is distributed under the BSD License. See LICENSE.txt
 * for details.
 */

#ifndef SPARSEX_INTERNALS_DYNAMIC_ARRAY_HPP
#define SPARSEX_INTERNALS_DYNAMIC_ARRAY_HPP

#include <sparsex/internals/Allocators.hpp>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>

using namespace std;

namespace sparsex {
  namespace utilities {

    template<typename T, typename Allocator = reallocator<T> >
    class DynamicArray
    {
    public:
      DynamicArray(size_t capacity = 1024)
        : size_(0),
          capacity_(capacity),
          own_elems_(true)
      {
        elems_ = alloc_.allocate(capacity_);
      }

      DynamicArray(T *storage, size_t capacity)
        : elems_(storage),
          size_(0),
          capacity_(capacity),
          own_elems_(false)
      { }

      ~DynamicArray()
      {
        if (own_elems_) {
	  // Destroy array elements and deallocate
	  alloc_.destroy(elems_, size_);
	  alloc_.deallocate(elems_, capacity_);
        }
      }

      size_t GetSize() const
      {
        return size_;
      }

      size_t GetCapacity() const
      {
        return capacity_;
      }

      void Resize(size_t nr_elems)
      {
        if (nr_elems < size_) {
	  // Destroy excess elements, then shrink
	  for (size_t i = size_-1; i >= nr_elems; --i)
	    alloc_.destroy(&elems_[i]);
	  size_ = nr_elems;
        }

        // Use no-throw version 
        elems_ = alloc_.reallocate(capacity_, nr_elems, elems_, nothrow);
        capacity_ = nr_elems;
      }

      const T *GetElems() const
      {
        return elems_;
      }

      T *TakeElems()
      {
        // Be safe and shrink-to-fit first, since the user is (conceptually)
        // aware only of the dynamic array size, so avoid future memory leaks
        ShrinkToFit();
        own_elems_ = false;
        return elems_;
      }

      void Append(const T &val)
      {
        if (size_ == capacity_) {
	  // Expand array
	  if (!capacity_)
	    // we were previously resized to zero, so re-initialize
	    Resize(1024);
	  else
	    Resize(2*capacity_);
        }

        alloc_.construct(&elems_[size_], val);
        ++size_;
      }

      void Append(T &&val)
      {
        if (size_ == capacity_) {
	  // Expand array
	  if (!capacity_)
	    // we were previously resized to zero, so re-initialize
	    Resize(1024);
	  else
	    Resize(2*capacity_);
        }

        // val is an rvalue so forward it as an rvalue in order to use
        // the construct(T&&) overload and avoid the redundant copy
        alloc_.construct(&elems_[size_], forward<T>(val));
        ++size_;
      }

      const T &GetLast() const
      {
        if (!size_)
	  throw out_of_range(__FUNCTION__);

        return elems_[size_-1];
      }

      void ShrinkToFit()
      {
        Resize(size_);
        assert((size_ == capacity_) && "[BUG] shrink failed");
      }

      Allocator &GetAllocator()
      {
        return alloc_;
      }

      T &operator[](size_t pos)
      {
        if (pos >= size_)
	  throw out_of_range(__FUNCTION__);

        return elems_[pos];
      }

      const T &operator[](size_t pos) const
      {
        if (pos >= size_)
	  throw out_of_range(__FUNCTION__);

        return elems_[pos];
      }

    private:
      T *elems_;
      Allocator alloc_;
      size_t size_;
      size_t capacity_;
      bool own_elems_;
    };

    template<typename T>
    ostream &operator<<(ostream& os, const DynamicArray<T> &da)
    {
      for (size_t i = 0; i < da.GetSize(); ++i)
        os << static_cast<int>(da[i]) << "|";

      os << "\n";
      return os;
    }

  } // end of namespace utilities
} // end of namespace sparsex

#endif  // SPARSEX_INTERNALS_DYNAMIC_ARRAY_HPP
