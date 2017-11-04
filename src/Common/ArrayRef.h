// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "ArrayView.h"
#include <limits>

namespace Common {

// 'ArrayRef' is a pair of pointer to object of parametrized type and size.
// It is recommended to pass 'ArrayRef' to procedures by value.
// 'ArrayRef' supports 'EMPTY' and 'NIL' representations as follows:
//   'data' == 'nullptr' && 'size' == 0 - EMPTY NIL
//   'data' != 'nullptr' && 'size' == 0 - EMPTY NOTNIL
//   'data' == 'nullptr' && 'size' > 0 - Undefined
//   'data' != 'nullptr' && 'size' > 0 - NOTEMPTY NOTNIL
// For signed integer 'Size', 'ArrayRef' with 'size' < 0 is undefined.
template<class ObjectType = uint8_t, class SizeType = size_t> class ArrayRef {
public:
  typedef ObjectType Object;
  typedef SizeType Size;

  const static Size INVALID;
  const static ArrayRef EMPTY;
  const static ArrayRef NIL;

  // Default constructor.
  // Leaves object uninitialized. Any usage before initializing it is undefined.
  ArrayRef()
#ifndef NDEBUG
    : data(nullptr), size(INVALID) // In debug mode, fill in object with invalid state (undefined).
#endif
  {
  }

  // Direct constructor.
  // The behavior is undefined unless 'arrayData' != 'nullptr' || 'arraySize' == 0
  ArrayRef(Object* arrayData, Size arraySize) : data(arrayData), size(arraySize) {
    assert(data != nullptr || size == 0);
  }

  // Constructor from C array.
  // The behavior is undefined unless 'arrayData' != 'nullptr' || 'arraySize' == 0. Input state can be malformed using poiner conversions.
  template<Size arraySize> ArrayRef(Object(&arrayData)[arraySize]) : data(arrayData), size(arraySize) {
    assert(data != nullptr || size == 0);
  }

  // Copy constructor.
  // Performs default action - bitwise copying of source object.
  // The behavior is undefined unless 'other' 'ArrayRef' is in defined state, that is 'data' != 'nullptr' || 'size' == 0
  ArrayRef(const ArrayRef& other) : data(other.data), size(other.size) {
    assert(data != nullptr || size == 0);
  }

  // Destructor.
  // No special action is performed.
  ~ArrayRef() {
  }

  // Copy assignment operator.
  // The behavior is undefined unless 'other' 'ArrayRef' is in defined state, that is 'data' != 'nullptr' || 'size' == 0
  ArrayRef& operator=(const ArrayRef& other) {
    assert(other.data != nullptr || other.size == 0);
    data = other.data;
    size = other.size;
    return *this;
  }

  operator ArrayView<Object, Size>() const {
    return ArrayView<Object, Size>(data, size);
  }

  Object* getData() const {
    assert(data != nullptr || size == 0);
    return data;
  }

  Size getSize() const {
    assert(data != nullptr || size == 0);
    return size;
  }

  // Return false if 'ArrayRef' is not EMPTY.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  bool isEmpty() const {
    assert(data != nullptr || size == 0);
    return size == 0;
  }

  // Return false if 'ArrayRef' is not NIL.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  bool isNil() const {
    assert(data != nullptr || size == 0);
    return data == nullptr;
  }

  // Get 'ArrayRef' element by index.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'index' < 'size'.
  Object& operator[](Size index) const {
    assert(data != nullptr || size == 0);
    assert(index < size);
    return *(data + index);
  }

  // Get first element.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'size' > 0
  Object& first() const {
    assert(data != nullptr || size == 0);
    assert(size > 0);
    return *data;
  }

  // Get last element.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'size' > 0
  Object& last() const {
    assert(data != nullptr || size == 0);
    assert(size > 0);
    return *(data + (size - 1));
  }

  // Return a pointer to the first element.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  Object* begin() const {
    assert(data != nullptr || size == 0);
    return data;
  }

  // Return a pointer after the last element.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  Object* end() const {
    assert(data != nullptr || size == 0);
    return data + size;
  }

  // Compare elements of two arrays, return false if there is a difference.
  // EMPTY and NIL arrays are considered equal.
  // The behavior is undefined unless both arrays were initialized.
  bool operator==(ArrayView<Object, Size> other) const {
    assert(data != nullptr || size == 0);
    if (size == other.getSize()) {
      for (Size i = 0;; ++i) {
        if (i == size) {
          return true;
        }

        if (!(*(data + i) == *(other.getData() + i))) {
          break;
        }
      }
    }

    return false;
  }

  // Compare elements two arrays, return false if there is no difference.
  // EMPTY and NIL arrays are considered equal.
  // The behavior is undefined unless both arrays were initialized.
  bool operator!=(ArrayView<Object, Size> other) const {
    assert(data != nullptr || size == 0);
    if (size == other.getSize()) {
      for (Size i = 0;; ++i) {
        if (i == size) {
          return false;
        }

        if (*(data + i) != *(other.getData() + i)) {
          break;
        }
      }
    }

    return true;
  }

  // Return false if 'ArrayRef' does not contain 'object' at the beginning.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  bool beginsWith(const Object& object) const {
    assert(data != nullptr || size == 0);
    if (size == 0) {
      return false;
    }

    return *data == object;
  }

  // Return false if 'ArrayRef' does not contain 'other' at the beginning.
  // The behavior is undefined unless both arrays were initialized.
  bool beginsWith(ArrayView<Object, Size> other) const {
    assert(data != nullptr || size == 0);
    if (size >= other.getSize()) {
      for (Size i = 0;; ++i) {
        if (i == other.getSize()) {
          return true;
        }

        if (!(*(data + i) == *(other.getData() + i))) {
          break;
        }
      }
    }

    return false;
  }

  // Return false if 'ArrayRef' does not contain 'object'.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  bool contains(const Object& object) const {
    assert(data != nullptr || size == 0);
    for (Size i = 0; i < size; ++i) {
      if (*(data + i) == object) {
        return true;
      }
    }

    return false;
  }

  // Return false if 'ArrayRef' does not contain 'other'.
  // The behavior is undefined unless both arrays were initialized.
  bool contains(ArrayView<Object, Size> other) const {
    assert(data != nullptr || size == 0);
    if (size >= other.getSize()) {
      Size i = size - other.getSize();
      for (Size j = 0; !(i < j); ++j) {
        for (Size k = 0;; ++k) {
          if (k == other.getSize()) {
            return true;
          }

          if (!(*(data + j + k) == *(other.getData() + k))) {
            break;
          }
        }
      }
    }

    return false;
  }

  // Return false if 'ArrayRef' does not contain 'object' at the end.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  bool endsWith(const Object& object) const {
    assert(data != nullptr || size == 0);
    if (size == 0) {
      return false;
    }

    return *(data + (size - 1)) == object;
  }

  // Return false if 'ArrayRef' does not contain 'other' at the end.
  // The behavior is undefined unless both arrays were initialized.
  bool endsWith(ArrayView<Object, Size> other) const {
    assert(data != nullptr || size == 0);
    if (size >= other.getSize()) {
      Size i = size - other.getSize();
      for (Size j = 0;; ++j) {
        if (j == other.getSize()) {
          return true;
        }

        if (!(*(data + i + j) == *(other.getData() + j))) {
          break;
        }
      }
    }

    return false;
  }

  // Looks for the first occurence of 'object' in 'ArrayRef',
  // returns index or INVALID if there are no occurences.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  Size find(const Object& object) const {
    assert(data != nullptr || size == 0);
    for (Size i = 0; i < size; ++i) {
      if (*(data + i) == object) {
        return i;
      }
    }

    return INVALID;
  }

  // Looks for the first occurence of 'other' in 'ArrayRef',
  // returns index or INVALID if there are no occurences.
  // The behavior is undefined unless both arrays were initialized.
  Size find(ArrayView<Object, Size> other) const {
    assert(data != nullptr || size == 0);
    if (size >= other.getSize()) {
      Size i = size - other.getSize();
      for (Size j = 0; !(i < j); ++j) {
        for (Size k = 0;; ++k) {
          if (k == other.getSize()) {
            return j;
          }

          if (!(*(data + j + k) == *(other.getData() + k))) {
            break;
          }
        }
      }
    }

    return INVALID;
  }

  // Looks for the last occurence of 'object' in 'ArrayRef',
  // returns index or INVALID if there are no occurences.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  Size findLast(const Object& object) const {
    assert(data != nullptr || size == 0);
    for (Size i = 0; i < size; ++i) {
      if (*(data + (size - 1 - i)) == object) {
        return size - 1 - i;
      }
    }

    return INVALID;
  }

  // Looks for the first occurence of 'other' in 'ArrayRef',
  // returns index or INVALID if there are no occurences.
  // The behavior is undefined unless both arrays were initialized.
  Size findLast(ArrayView<Object, Size> other) const {
    assert(data != nullptr || size == 0);
    if (size >= other.getSize()) {
      Size i = size - other.getSize();
      for (Size j = 0; !(i < j); ++j) {
        for (Size k = 0;; ++k) {
          if (k == other.getSize()) {
            return i - j;
          }

          if (!(*(data + (i - j + k)) == *(other.getData() + k))) {
            break;
          }
        }
      }
    }

    return INVALID;
  }

  // Returns subarray of 'headSize' first elements.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'headSize' <= 'size'.
  ArrayRef head(Size headSize) const {
    assert(data != nullptr || size == 0);
    assert(headSize <= size);
    return ArrayRef(data, headSize);
  }

  // Returns subarray of 'tailSize' last elements.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'tailSize' <= 'size'.
  ArrayRef tail(Size tailSize) const {
    assert(data != nullptr || size == 0);
    assert(tailSize <= size);
    return ArrayRef(data + (size - tailSize), tailSize);
  }

  // Returns 'ArrayRef' without 'headSize' first elements.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'headSize' <= 'size'.
  ArrayRef unhead(Size headSize) const {
    assert(data != nullptr || size == 0);
    assert(headSize <= size);
    return ArrayRef(data + headSize, size - headSize);
  }

  // Returns 'ArrayRef' without 'tailSize' last elements.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'tailSize' <= 'size'.
  ArrayRef untail(Size tailSize) const {
    assert(data != nullptr || size == 0);
    assert(tailSize <= size);
    return ArrayRef(data, size - tailSize);
  }

  // Returns subarray starting at 'startIndex' and contaning 'endIndex' - 'startIndex' elements.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'startIndex' <= 'endIndex' and 'endIndex' <= 'size'.
  ArrayRef range(Size startIndex, Size endIndex) const {
    assert(data != nullptr || size == 0);
    assert(startIndex <= endIndex && endIndex <= size);
    return ArrayRef(data + startIndex, endIndex - startIndex);
  }

  // Returns subarray starting at 'startIndex' and contaning 'sliceSize' elements.
  // The behavior is undefined unless 'ArrayRef' was initialized and 'startIndex' <= 'size' and 'startIndex' + 'sliceSize' <= 'size'.
  ArrayRef slice(Size startIndex, Size sliceSize) const {
    assert(data != nullptr || size == 0);
    assert(startIndex <= size && startIndex + sliceSize <= size);
    return ArrayRef(data + startIndex, sliceSize);
  }

  // Copy 'object' to each element of 'ArrayRef'.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  const ArrayRef& fill(const Object& object) const {
    assert(data != nullptr || size == 0);
    for (Size i = 0; i < size; ++i) {
      *(data + i) = object;
    }

    return *this;
  }

  // Reverse 'ArrayRef' elements.
  // The behavior is undefined unless 'ArrayRef' was initialized.
  const ArrayRef& reverse() const {
    assert(data != nullptr || size == 0);
    for (Size i = 0; i < size / 2; ++i) {
      Object object = *(data + i);
      *(data + i) = *(data + (size - 1 - i));
      *(data + (size - 1 - i)) = object;
    }

    return *this;
  }

protected:
  Object* data;
  Size size;
};

template<class Object, class Size> const Size ArrayRef<Object, Size>::INVALID = std::numeric_limits<Size>::max();
template<class Object, class Size> const ArrayRef<Object, Size> ArrayRef<Object, Size>::EMPTY(reinterpret_cast<Object*>(1), 0);
template<class Object, class Size> const ArrayRef<Object, Size> ArrayRef<Object, Size>::NIL(nullptr, 0);

}
