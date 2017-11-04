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

#include "StringView.h"
#include <limits>
#include <string.h>

namespace Common {

// 'StringBuffer' is a string of fixed maximum size.
template<size_t MAXIMUM_SIZE_VALUE> class StringBuffer {
public:
  typedef char Object;
  typedef size_t Size;

  const static Size MAXIMUM_SIZE = MAXIMUM_SIZE_VALUE;
  const static Size INVALID;

  static_assert(MAXIMUM_SIZE != 0, "StringBuffer's size must not be zero");

  // Default constructor.
  // After construction, 'StringBuffer' is empty, that is 'size' == 0
  StringBuffer() : size(0) {
  }

  // Direct constructor.
  // Copies string from 'stringData' to 'StringBuffer'.
  // The behavior is undefined unless ('stringData' != 'nullptr' || 'stringSize' == 0) && 'stringSize' <= 'MAXIMUM_SIZE'.
  StringBuffer(const Object* stringData, Size stringSize) : size(stringSize) {
    assert(stringData != nullptr || size == 0);
    assert(size <= MAXIMUM_SIZE);
    memcpy(data, stringData, size);
  }

  // Constructor from C array.
  // Copies string from 'stringData' to 'StringBuffer'.
  // The behavior is undefined unless ('stringData' != 'nullptr' || 'stringSize' == 0) && 'stringSize' <= 'MAXIMUM_SIZE'. Input state can be malformed using poiner conversions.
  template<Size stringSize> explicit StringBuffer(const Object(&stringData)[stringSize]) : size(stringSize - 1) {
    assert(stringData != nullptr || size == 0);
    assert(size <= MAXIMUM_SIZE);
    memcpy(data, stringData, size);
  }

  // Constructor from StringView
  // Copies string from 'stringView' to 'StringBuffer'.
  // The behavior is undefined unless 'stringView.size()' <= 'MAXIMUM_SIZE'.
  explicit StringBuffer(StringView stringView) : size(stringView.getSize()) {
    assert(size <= MAXIMUM_SIZE);
    memcpy(data, stringView.getData(), size);
  }

  // Copy constructor.
  // Copies string from 'other' to 'StringBuffer'.
  StringBuffer(const StringBuffer& other) : size(other.size) {
    memcpy(data, other.data, size);
  }

  // Destructor.
  // No special action is performed.
  ~StringBuffer() {
  }

  // Copy assignment operator.
  StringBuffer& operator=(const StringBuffer& other) {
    size = other.size;
    memcpy(data, other.data, size);
    return *this;
  }

  // StringView assignment operator.
  // Copies string from 'stringView' to 'StringBuffer'.
  // The behavior is undefined unless 'stringView.size()' <= 'MAXIMUM_SIZE'.
  StringBuffer& operator=(StringView stringView) {
    assert(stringView.getSize() <= MAXIMUM_SIZE);
    memcpy(data, stringView.getData(), stringView.getSize());
    size = stringView.getSize();
    return *this;
  }

  operator StringView() const {
    return StringView(data, size);
  }

  explicit operator std::string() const {
    return std::string(data, size);
  }

  Object* getData() {
    return data;
  }

  const Object* getData() const {
    return data;
  }

  Size getSize() const {
    return size;
  }

  // Return false if 'StringView' is not EMPTY.
  bool isEmpty() const {
    return size == 0;
  }

  // Get 'StringBuffer' element by index.
  // The behavior is undefined unless 'index' < 'size'.
  Object& operator[](Size index) {
    assert(index < size);
    return *(data + index);
  }

  // Get 'StringBuffer' element by index.
  // The behavior is undefined unless 'index' < 'size'.
  const Object& operator[](Size index) const {
    assert(index < size);
    return *(data + index);
  }

  // Get first element.
  // The behavior is undefined unless 'size' > 0
  Object& first() {
    assert(size > 0);
    return *data;
  }

  // Get first element.
  // The behavior is undefined unless 'size' > 0
  const Object& first() const {
    assert(size > 0);
    return *data;
  }

  // Get last element.
  // The behavior is undefined unless 'size' > 0
  Object& last() {
    assert(size > 0);
    return *(data + (size - 1));
  }

  // Get last element.
  // The behavior is undefined unless 'size' > 0
  const Object& last() const {
    assert(size > 0);
    return *(data + (size - 1));
  }

  // Return a pointer to the first element.
  Object* begin() {
    return data;
  }

  // Return a pointer to the first element.
  const Object* begin() const {
    return data;
  }

  // Return a pointer after the last element.
  Object* end() {
    return data + size;
  }

  // Return a pointer after the last element.
  const Object* end() const {
    return data + size;
  }

  // Compare elements of two strings, return false if there is a difference.
  bool operator==(StringView other) const {
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

  // Compare elements two strings, return false if there is no difference.
  bool operator!=(StringView other) const {
    return !(*this == other);
  }

  // Compare two strings character-wise.
  bool operator<(StringView other) const {
    Size count = other.getSize() < size ? other.getSize() : size;
    for (Size i = 0; i < count; ++i) {
      Object char1 = *(data + i);
      Object char2 = *(other.getData() + i);
      if (char1 < char2) {
        return true;
      }

      if (char2 < char1) {
        return false;
      }
    }

    return size < other.getSize();
  }

  // Compare two strings character-wise.
  bool operator<=(StringView other) const {
    return !(other < *this);
  }

  // Compare two strings character-wise.
  bool operator>(StringView other) const {
    return other < *this;
  }

  // Compare two strings character-wise.
  bool operator>=(StringView other) const {
    return !(*this < other);
  }

  // Return false if 'StringView' does not contain 'object' at the beginning.
  bool beginsWith(const Object& object) const {
    if (size == 0) {
      return false;
    }

    return *data == object;
  }

  // Return false if 'StringView' does not contain 'other' at the beginning.
  bool beginsWith(StringView other) const {
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

  // Return false if 'StringView' does not contain 'object'.
  bool contains(const Object& object) const {
    for (Size i = 0; i < size; ++i) {
      if (*(data + i) == object) {
        return true;
      }
    }

    return false;
  }

  // Return false if 'StringView' does not contain 'other'.
  bool contains(StringView other) const {
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

  // Return false if 'StringView' does not contain 'object' at the end.
  bool endsWith(const Object& object) const {
    if (size == 0) {
      return false;
    }

    return *(data + (size - 1)) == object;
  }

  // Return false if 'StringView' does not contain 'other' at the end.
  bool endsWith(StringView other) const {
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

  // Looks for the first occurence of 'object' in 'StringView',
  // returns index or INVALID if there are no occurences.
  Size find(const Object& object) const {
    for (Size i = 0; i < size; ++i) {
      if (*(data + i) == object) {
        return i;
      }
    }

    return INVALID;
  }

  // Looks for the first occurence of 'other' in 'StringView',
  // returns index or INVALID if there are no occurences.
  Size find(StringView other) const {
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

  // Looks for the last occurence of 'object' in 'StringView',
  // returns index or INVALID if there are no occurences.
  Size findLast(const Object& object) const {
    for (Size i = 0; i < size; ++i) {
      if (*(data + (size - 1 - i)) == object) {
        return size - 1 - i;
      }
    }

    return INVALID;
  }

  // Looks for the first occurence of 'other' in 'StringView',
  // returns index or INVALID if there are no occurences.
  Size findLast(StringView other) const {
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

  // Returns substring of 'headSize' first elements.
  // The behavior is undefined unless 'headSize' <= 'size'.
  StringView head(Size headSize) const {
    assert(headSize <= size);
    return StringView(data, headSize);
  }

  // Returns substring of 'tailSize' last elements.
  // The behavior is undefined unless 'tailSize' <= 'size'.
  StringView tail(Size tailSize) const {
    assert(tailSize <= size);
    return StringView(data + (size - tailSize), tailSize);
  }

  // Returns 'StringView' without 'headSize' first elements.
  // The behavior is undefined unless 'headSize' <= 'size'.
  StringView unhead(Size headSize) const {
    assert(headSize <= size);
    return StringView(data + headSize, size - headSize);
  }

  // Returns 'StringView' without 'tailSize' last elements.
  // The behavior is undefined unless 'tailSize' <= 'size'.
  StringView untail(Size tailSize) const {
    assert(tailSize <= size);
    return StringView(data, size - tailSize);
  }

  // Returns substring starting at 'startIndex' and contaning 'endIndex' - 'startIndex' elements.
  // The behavior is undefined unless 'startIndex' <= 'endIndex' and 'endIndex' <= 'size'.
  StringView range(Size startIndex, Size endIndex) const {
    assert(startIndex <= endIndex && endIndex <= size);
    return StringView(data + startIndex, endIndex - startIndex);
  }

  // Returns substring starting at 'startIndex' and contaning 'sliceSize' elements.
  // The behavior is undefined unless 'startIndex' <= 'size' and 'sliceSize' <= 'size' - 'startIndex'.
  StringView slice(Size startIndex, Size sliceSize) const {
    assert(startIndex <= size && sliceSize <= size - startIndex);
    return StringView(data + startIndex, sliceSize);
  }

  // Appends 'object' to 'StringBuffer'.
  // The behavior is undefined unless 1 <= 'MAXIMUM_SIZE' - 'size'.
  StringBuffer& append(Object object) {
    assert(1 <= MAXIMUM_SIZE - size);
    data[size] = object;
    ++size;
    return *this;
  }

  // Appends 'stringView' to 'StringBuffer'.
  // The behavior is undefined unless 'stringView.size()' <= 'MAXIMUM_SIZE' - 'size'.
  StringBuffer& append(StringView stringView) {
    assert(stringView.getSize() <= MAXIMUM_SIZE - size);
    if (stringView.getSize() != 0) {
      memcpy(data + size, stringView.getData(), stringView.getSize());
      size += stringView.getSize();
    }

    return *this;
  }

  // Sets 'StringBuffer' to empty state, that is 'size' == 0
  StringBuffer& clear() {
    size = 0;
    return *this;
  }

  // Removes substring starting at 'startIndex' and contaning 'cutSize' elements.
  // The behavior is undefined unless 'startIndex' <= 'size' and 'cutSize' <= 'size' - 'startIndex'.
  StringBuffer& cut(Size startIndex, Size cutSize) {
    assert(startIndex <= size && cutSize <= size - startIndex);
    if (cutSize != 0) {
      memcpy(data + startIndex, data + startIndex + cutSize, size - startIndex - cutSize);
      size -= cutSize;
    }

    return *this;
  }

  // Copy 'object' to each element of 'StringBuffer'.
  StringBuffer& fill(Object object) {
    if (size > 0) {
      memset(data, object, size);
    }

    return *this;
  }

  // Inserts 'object' into 'StringBuffer' at 'index'.
  // The behavior is undefined unless 'index' <= 'size' and 1 <= 'MAXIMUM_SIZE' - 'size'.
  StringBuffer& insert(Size index, Object object) {
    assert(index <= size);
    assert(1 <= MAXIMUM_SIZE - size);
    memmove(data + index + 1, data + index, size - index);
    data[index] = object;
    ++size;
    return *this;
  }

  // Inserts 'stringView' into 'StringBuffer' at 'index'.
  // The behavior is undefined unless 'index' <= 'size' and 'stringView.size()' <= 'MAXIMUM_SIZE' - 'size'.
  StringBuffer& insert(Size index, StringView stringView) {
    assert(index <= size);
    assert(stringView.getSize() <= MAXIMUM_SIZE - size);
    if (stringView.getSize() != 0) {
      memmove(data + index + stringView.getSize(), data + index, size - index);
      memcpy(data + index, stringView.getData(), stringView.getSize());
      size += stringView.getSize();
    }

    return *this;
  }

  // Overwrites 'StringBuffer' starting at 'index' with 'stringView', possibly expanding 'StringBuffer'.
  // The behavior is undefined unless 'index' <= 'size' and 'stringView.size()' <= 'MAXIMUM_SIZE' - 'index'.
  StringBuffer& overwrite(Size index, StringView stringView) {
    assert(index <= size);
    assert(stringView.getSize() <= MAXIMUM_SIZE - index);
    memcpy(data + index, stringView.getData(), stringView.getSize());
    if (size < index + stringView.getSize()) {
      size = index + stringView.getSize();
    }

    return *this;
  }

  // Sets 'size' to 'bufferSize', assigning value '\0' to newly inserted elements.
  // The behavior is undefined unless 'bufferSize' <= 'MAXIMUM_SIZE'.
  StringBuffer& resize(Size bufferSize) {
    assert(bufferSize <= MAXIMUM_SIZE);
    if (bufferSize > size) {
      memset(data + size, 0, bufferSize - size);
    }

    size = bufferSize;
    return *this;
  }

  // Reverse 'StringBuffer' elements.
  StringBuffer& reverse() {
    for (Size i = 0; i < size / 2; ++i) {
      Object object = *(data + i);
      *(data + i) = *(data + (size - 1 - i));
      *(data + (size - 1 - i)) = object;
    }

    return *this;
  }

  // Sets 'size' to 'bufferSize'.
  // The behavior is undefined unless 'bufferSize' <= 'size'.
  StringBuffer& shrink(Size bufferSize) {
    assert(bufferSize <= size);
    size = bufferSize;
    return *this;
  }

protected:
  Object data[MAXIMUM_SIZE];
  Size size;
};

template<size_t MAXIMUM_SIZE> const typename StringBuffer<MAXIMUM_SIZE>::Size StringBuffer<MAXIMUM_SIZE>::INVALID = std::numeric_limits<typename StringBuffer<MAXIMUM_SIZE>::Size>::max();

}
