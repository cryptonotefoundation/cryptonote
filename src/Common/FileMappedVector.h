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

#include <cassert>
#include <cstdint>
#include <string>

#include <boost/filesystem.hpp>

#include "System/MemoryMappedFile.h"

#include "Common/ScopeExit.h"

namespace Common {

template<class T>
struct EnableIfPod {
  typedef typename std::enable_if<std::is_pod<T>::value, EnableIfPod>::type type;
};

enum class FileMappedVectorOpenMode {
  OPEN,
  CREATE,
  OPEN_OR_CREATE
};

template<class T>
class FileMappedVector : public EnableIfPod<T>::type {
public:
  typedef T value_type;

  const static uint64_t metadataSize = static_cast<uint64_t>(2 * sizeof(uint64_t));
  const static uint64_t valueSize = static_cast<uint64_t>(sizeof(T));

  class const_iterator {
  public:
    typedef std::random_access_iterator_tag iterator_category;
    typedef T value_type;
    typedef ptrdiff_t difference_type;
    typedef const T* pointer;
    typedef const T& reference;

    const_iterator() : m_fileMappedVector(nullptr) {
    }

    const_iterator(const FileMappedVector* fileMappedVector, size_t index) :
      m_fileMappedVector(fileMappedVector),
      m_index(index) {
    }

    const T& operator*() const {
      return (*m_fileMappedVector)[m_index];
    }

    const T* operator->() const {
      return &(*m_fileMappedVector)[m_index];
    }

    const_iterator& operator++() {
      ++m_index;
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++m_index;
      return tmp;
    }

    const_iterator& operator--() {
      --m_index;
      return *this;
    }

    const_iterator operator--(int) {
      const_iterator tmp = *this;
      --m_index;
      return tmp;
    }

    const_iterator& operator+=(difference_type n) {
      m_index += n;
      return *this;
    }

    const_iterator operator+(difference_type n) const {
      return const_iterator(m_fileMappedVector, m_index + n);
    }

    friend const_iterator operator+(difference_type n, const const_iterator& i) {
      return const_iterator(i.m_fileMappedVector, n + i.m_index);
    }

    const_iterator& operator-=(difference_type n) {
      m_index -= n;
      return *this;
    }

    const_iterator operator-(difference_type n) const {
      return const_iterator(m_fileMappedVector, m_index - n);
    }

    difference_type operator-(const const_iterator& other) const {
      return m_index - other.m_index;
    }

    const T& operator[](difference_type offset) const {
      return (*m_fileMappedVector)[m_index + offset];
    }

    bool operator==(const const_iterator& other) const {
      return m_index == other.m_index;
    }

    bool operator!=(const const_iterator& other) const {
      return m_index != other.m_index;
    }

    bool operator<(const const_iterator& other) const {
      return m_index < other.m_index;
    }

    bool operator>(const const_iterator& other) const {
      return m_index > other.m_index;
    }

    bool operator<=(const const_iterator& other) const {
      return m_index <= other.m_index;
    }

    bool operator>=(const const_iterator& other) const {
      return m_index >= other.m_index;
    }

    size_t index() const {
      return m_index;
    }

  protected:
    const FileMappedVector* m_fileMappedVector;
    size_t m_index;
  };

  class iterator : public const_iterator {
  public:
    typedef std::random_access_iterator_tag iterator_category;
    typedef T value_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef T& reference;

    iterator() : const_iterator() {
    }

    iterator(const FileMappedVector* fileMappedVector, size_t index) : const_iterator(fileMappedVector, index) {
    }

    T& operator*() const {
      return const_cast<T&>((*const_iterator::m_fileMappedVector)[const_iterator::m_index]);
    }

    T* operator->() const {
      return const_cast<T*>(&(*const_iterator::m_fileMappedVector)[const_iterator::m_index]);
    }

    iterator& operator++() {
      ++const_iterator::m_index;
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++const_iterator::m_index;
      return tmp;
    }

    iterator& operator--() {
      --const_iterator::m_index;
      return *this;
    }

    iterator operator--(int) {
      iterator tmp = *this;
      --const_iterator::m_index;
      return tmp;
    }

    iterator& operator+=(difference_type n) {
      const_iterator::m_index += n;
      return *this;
    }

    iterator operator+(difference_type n) const {
      return iterator(const_iterator::m_fileMappedVector, const_iterator::m_index + n);
    }

    friend iterator operator+(difference_type n, const iterator& i) {
      return iterator(i.m_fileMappedVector, n + i.m_index);
    }

    iterator& operator-=(difference_type n) {
      const_iterator::m_index -= n;
      return *this;
    }

    iterator operator-(difference_type n) const {
      return iterator(const_iterator::m_fileMappedVector, const_iterator::m_index - n);
    }

    difference_type operator-(const iterator& other) const {
      return const_iterator::m_index - other.m_index;
    }

    T& operator[](difference_type offset) const {
      return (*const_iterator::m_fileMappedVector)[const_iterator::m_index + offset];
    }
  };

  FileMappedVector();
  FileMappedVector(const std::string& path, FileMappedVectorOpenMode mode = FileMappedVectorOpenMode::OPEN_OR_CREATE, uint64_t prefixSize = 0);
  FileMappedVector(const FileMappedVector&) = delete;
  FileMappedVector& operator=(const FileMappedVector&) = delete;

  void open(const std::string& path, FileMappedVectorOpenMode mode = FileMappedVectorOpenMode::OPEN_OR_CREATE, uint64_t prefixSize = 0);
  void close();
  void close(std::error_code& ec);
  bool isOpened() const;

  bool empty() const;
  uint64_t capacity() const;
  uint64_t size() const;
  void reserve(uint64_t n);
  void shrink_to_fit();

  const_iterator begin() const;
  iterator begin();
  const_iterator cbegin() const;
  const_iterator end() const;
  iterator end();
  const_iterator cend() const;

  const T& operator[](uint64_t index) const;
  T& operator[](uint64_t index);
  const T& at(uint64_t index) const;
  T& at(uint64_t index);
  const T& front() const;
  T& front();
  const T& back() const;
  T& back();
  const T* data() const;
  T* data();

  void clear();
  iterator erase(const_iterator position);
  iterator erase(const_iterator first, const_iterator last);
  iterator insert(const_iterator position, const T& val);
  template<class InputIterator>
  iterator insert(const_iterator position, InputIterator first, InputIterator last);
  void pop_back();
  void push_back(const T& val);
  void swap(FileMappedVector& other);

  bool getAutoFlush() const;
  void setAutoFlush(bool autoFlush);

  void flush();

  const uint8_t* prefix() const;
  uint8_t* prefix();
  uint64_t prefixSize() const;
  void resizePrefix(uint64_t newPrefixSize);

  const uint8_t* suffix() const;
  uint8_t* suffix();
  uint64_t suffixSize() const;
  void resizeSuffix(uint64_t newSuffixSize);

  void rename(const std::string& newPath, std::error_code& ec);
  void rename(const std::string& newPath);

  template<class F>
  void atomicUpdate(F&& func);

private:
  std::string m_path;
  System::MemoryMappedFile m_file;
  uint64_t m_prefixSize;
  uint64_t m_suffixSize;
  bool m_autoFlush;

private:
  template<class F>
  void atomicUpdate(uint64_t newSize, uint64_t newCapacity, uint64_t newPrefixSize, uint64_t newSuffixSize, F&& func);
  template<class F>
  void atomicUpdate0(uint64_t newCapacity, uint64_t newPrefixSize, uint64_t newSuffixSize, F&& func);

  void open(const std::string& path, uint64_t prefixSize);
  void create(const std::string& path, uint64_t initialCapacity, uint64_t prefixSize, uint64_t suffixSize);

  uint8_t* prefixPtr();
  const uint8_t* prefixPtr() const;
  uint64_t* capacityPtr();
  const uint64_t* capacityPtr() const;
  const uint64_t* sizePtr() const;
  uint64_t* sizePtr();
  T* vectorDataPtr();
  const T* vectorDataPtr() const;
  uint8_t* suffixPtr();
  const uint8_t* suffixPtr() const;

  uint64_t vectorDataSize();

  uint64_t nextCapacity();

  void flushElement(uint64_t index);
  void flushSize();
};

template<class T>
FileMappedVector<T>::FileMappedVector() :
  m_autoFlush(true)
{
}

template<class T>
FileMappedVector<T>::FileMappedVector(const std::string& path, FileMappedVectorOpenMode mode, uint64_t prefixSize) :
  m_autoFlush(true)
{
  open(path, mode, prefixSize);
}

template<class T>
void FileMappedVector<T>::open(const std::string& path, FileMappedVectorOpenMode mode, uint64_t prefixSize) {
  assert(!isOpened());

  const uint64_t initialCapacity = 10;

  boost::filesystem::path filePath = path;
  boost::filesystem::path bakPath = path + ".bak";
  bool fileExists;
  if (boost::filesystem::exists(filePath)) {
    if (boost::filesystem::exists(bakPath)) {
      boost::filesystem::remove(bakPath);
    }

    fileExists = true;
  } else if (boost::filesystem::exists(bakPath)) {
    boost::filesystem::rename(bakPath, filePath);
    fileExists = true;
  } else {
    fileExists = false;
  }

  if (mode == FileMappedVectorOpenMode::OPEN) {
    open(path, prefixSize);
  } else if (mode == FileMappedVectorOpenMode::CREATE) {
    create(path, initialCapacity, prefixSize, 0);
  } else if (mode == FileMappedVectorOpenMode::OPEN_OR_CREATE) {
    if (fileExists) {
      open(path, prefixSize);
    } else {
      create(path, initialCapacity, prefixSize, 0);
    }
  } else {
    throw std::runtime_error("FileMappedVector: Unsupported open mode: " + std::to_string(static_cast<int>(mode)));
  }
}

template<class T>
void FileMappedVector<T>::close(std::error_code& ec) {
  m_file.close(ec);
  if (!ec) {
    m_prefixSize = 0;
    m_suffixSize = 0;
    m_path.clear();
  }
}

template<class T>
void FileMappedVector<T>::close() {
  std::error_code ec;
  close(ec);
  if (ec) {
    throw std::system_error(ec, "FileMappedVector::close");
  }
}

template<class T>
bool FileMappedVector<T>::isOpened() const {
  return m_file.isOpened();
}

template<class T>
bool FileMappedVector<T>::empty() const {
  assert(isOpened());

  return size() == 0;
}

template<class T>
uint64_t FileMappedVector<T>::capacity() const {
  assert(isOpened());

  return *capacityPtr();
}

template<class T>
uint64_t FileMappedVector<T>::size() const {
  assert(isOpened());

  return *sizePtr();
}

template<class T>
void FileMappedVector<T>::reserve(uint64_t n) {
  assert(isOpened());

  if (n > capacity()) {
    atomicUpdate(size(), n, prefixSize(), suffixSize(), [this](value_type* target) {
      std::copy(cbegin(), cend(), target);
    });
  }
}

template<class T>
void FileMappedVector<T>::shrink_to_fit() {
  assert(isOpened());

  if (size() < capacity()) {
    atomicUpdate(size(), size(), prefixSize(), suffixSize(), [this](value_type* target) {
      std::copy(cbegin(), cend(), target);
    });
  }
}

template<class T>
typename FileMappedVector<T>::iterator FileMappedVector<T>::begin() {
  assert(isOpened());

  return iterator(this, 0);
}

template<class T>
typename FileMappedVector<T>::const_iterator FileMappedVector<T>::begin() const {
  assert(isOpened());

  return const_iterator(this, 0);
}

template<class T>
typename FileMappedVector<T>::const_iterator FileMappedVector<T>::cbegin() const {
  assert(isOpened());

  return const_iterator(this, 0);
}

template<class T>
typename FileMappedVector<T>::const_iterator FileMappedVector<T>::end() const {
  assert(isOpened());

  return const_iterator(this, size());
}

template<class T>
typename FileMappedVector<T>::iterator FileMappedVector<T>::end() {
  assert(isOpened());

  return iterator(this, size());
}

template<class T>
typename FileMappedVector<T>::const_iterator FileMappedVector<T>::cend() const {
  assert(isOpened());

  return const_iterator(this, size());
}

template<class T>
const T& FileMappedVector<T>::operator[](uint64_t index) const {
  assert(isOpened());

  return vectorDataPtr()[index];
}

template<class T>
T& FileMappedVector<T>::operator[](uint64_t index) {
  assert(isOpened());

  return vectorDataPtr()[index];
}

template<class T>
const T& FileMappedVector<T>::at(uint64_t index) const {
  assert(isOpened());

  if (index >= size()) {
    throw std::out_of_range("FileMappedVector::at " + std::to_string(index));
  }

  return vectorDataPtr()[index];
}

template<class T>
T& FileMappedVector<T>::at(uint64_t index) {
  assert(isOpened());

  if (index >= size()) {
    throw std::out_of_range("FileMappedVector::at " + std::to_string(index));
  }

  return vectorDataPtr()[index];
}

template<class T>
const T& FileMappedVector<T>::front() const {
  assert(isOpened());

  return vectorDataPtr()[0];
}

template<class T>
T& FileMappedVector<T>::front() {
  assert(isOpened());

  return vectorDataPtr()[0];
}

template<class T>
const T& FileMappedVector<T>::back() const {
  assert(isOpened());

  return vectorDataPtr()[size() - 1];
}

template<class T>
T& FileMappedVector<T>::back() {
  assert(isOpened());

  return vectorDataPtr()[size() - 1];
}

template<class T>
const T* FileMappedVector<T>::data() const {
  assert(isOpened());

  return vectorDataPtr();
}

template<class T>
T* FileMappedVector<T>::data() {
  assert(isOpened());

  return vectorDataPtr();
}

template<class T>
void FileMappedVector<T>::clear() {
  assert(isOpened());

  *sizePtr() = 0;
  flushSize();
}

template<class T>
typename FileMappedVector<T>::iterator FileMappedVector<T>::erase(const_iterator position) {
  assert(isOpened());

  return erase(position, std::next(position));
}

template<class T>
typename FileMappedVector<T>::iterator FileMappedVector<T>::erase(const_iterator first, const_iterator last) {
  assert(isOpened());

  uint64_t newSize = size() - std::distance(first, last);

  atomicUpdate(newSize, capacity(), prefixSize(), suffixSize(), [this, first, last](value_type* target) {
    std::copy(cbegin(), first, target);
    std::copy(last, cend(), target + std::distance(cbegin(), first));
  });

  return iterator(this, first.index());
}

template<class T>
typename FileMappedVector<T>::iterator FileMappedVector<T>::insert(const_iterator position, const T& val) {
  assert(isOpened());

  return insert(position, &val, &val + 1);
}

template<class T>
template<class InputIterator>
typename FileMappedVector<T>::iterator FileMappedVector<T>::insert(const_iterator position, InputIterator first, InputIterator last) {
  assert(isOpened());

  uint64_t newSize = size() + static_cast<uint64_t>(std::distance(first, last));
  uint64_t newCapacity;
  if (newSize > capacity()) {
    newCapacity = nextCapacity();
    if (newSize > newCapacity) {
      newCapacity = newSize;
    }
  } else {
    newCapacity = capacity();
  }

  atomicUpdate(newSize, newCapacity, prefixSize(), suffixSize(), [this, position, first, last](value_type* target) {
    std::copy(cbegin(), position, target);
    std::copy(first, last, target + position.index());
    std::copy(position, cend(), target + position.index() + std::distance(first, last));
  });

  return iterator(this, position.index());
}

template<class T>
void FileMappedVector<T>::pop_back() {
  assert(isOpened());

  --(*sizePtr());
  flushSize();
}

template<class T>
void FileMappedVector<T>::push_back(const T& val) {
  assert(isOpened());

  if (capacity() == size()) {
    reserve(nextCapacity());
  }

  vectorDataPtr()[size()] = val;
  flushElement(size());

  ++(*sizePtr());
  flushSize();
}

template<class T>
void FileMappedVector<T>::swap(FileMappedVector& other) {
  m_path.swap(other.m_path);
  m_file.swap(other.m_file);
  std::swap(m_prefixSize, other.m_prefixSize);
  std::swap(m_suffixSize, other.m_suffixSize);
}

template<class T>
bool FileMappedVector<T>::getAutoFlush() const {
  return m_autoFlush;
}

template<class T>
void FileMappedVector<T>::setAutoFlush(bool autoFlush) {
  m_autoFlush = autoFlush;
}

template<class T>
void FileMappedVector<T>::flush() {
  assert(isOpened());

  m_file.flush(m_file.data(), m_file.size());
}

template<class T>
const uint8_t* FileMappedVector<T>::prefix() const {
  assert(isOpened());

  return prefixPtr();
}

template<class T>
uint8_t* FileMappedVector<T>::prefix() {
  assert(isOpened());

  return prefixPtr();
}

template<class T>
uint64_t FileMappedVector<T>::prefixSize() const {
  assert(isOpened());

  return m_prefixSize;
}

template<class T>
void FileMappedVector<T>::resizePrefix(uint64_t newPrefixSize) {
  assert(isOpened());

  if (prefixSize() != newPrefixSize) {
    atomicUpdate(size(), capacity(), newPrefixSize, suffixSize(), [this](value_type* target) {
      std::copy(cbegin(), cend(), target);
    });
  }
}

template<class T>
const uint8_t* FileMappedVector<T>::suffix() const {
  assert(isOpened());

  return suffixPtr();
}

template<class T>
uint8_t* FileMappedVector<T>::suffix() {
  assert(isOpened());

  return suffixPtr();
}

template<class T>
uint64_t FileMappedVector<T>::suffixSize() const {
  assert(isOpened());

  return m_suffixSize;
}

template<class T>
void FileMappedVector<T>::resizeSuffix(uint64_t newSuffixSize) {
  assert(isOpened());

  if (suffixSize() != newSuffixSize) {
    atomicUpdate(size(), capacity(), prefixSize(), newSuffixSize, [this](value_type* target) {
      std::copy(cbegin(), cend(), target);
    });
  }
}

template<class T>
void FileMappedVector<T>::rename(const std::string& newPath, std::error_code& ec) {
  m_file.rename(newPath, ec);
  if (!ec) {
    m_path = newPath;
  }
}

template<class T>
void FileMappedVector<T>::rename(const std::string& newPath) {
  m_file.rename(newPath);
  m_path = newPath;
}

template<class T>
template<class F>
void FileMappedVector<T>::atomicUpdate(F&& func) {
  atomicUpdate0(capacity(), prefixSize(), suffixSize(), std::move(func));
}

template<class T>
template<class F>
void FileMappedVector<T>::atomicUpdate(uint64_t newSize, uint64_t newCapacity, uint64_t newPrefixSize, uint64_t newSuffixSize, F&& func) {
  assert(newSize <= newCapacity);

  atomicUpdate0(newCapacity, newPrefixSize, newSuffixSize, [this, newSize, &func](FileMappedVector<T>& newVector) {
    if (prefixSize() != 0 && newVector.prefixSize() != 0) {
      std::copy(prefixPtr(), prefixPtr() + std::min(prefixSize(), newVector.prefixSize()), newVector.prefix());
    }

    *newVector.sizePtr() = newSize;
    func(newVector.data());

    if (suffixSize() != 0 && newVector.suffixSize() != 0) {
      std::copy(suffixPtr(), suffixPtr() + std::min(suffixSize(), newVector.suffixSize()), newVector.suffix());
    }
  });
}

template<class T>
template<class F>
void FileMappedVector<T>::atomicUpdate0(uint64_t newCapacity, uint64_t newPrefixSize, uint64_t newSuffixSize, F&& func) {
  if (m_file.path() != m_path) {
    throw std::runtime_error("Vector is mapped to a .bak file due to earlier errors");
  }

  boost::filesystem::path bakPath = m_path + ".bak";
  boost::filesystem::path tmpPath = boost::filesystem::unique_path(m_path + ".tmp.%%%%-%%%%");

  if (boost::filesystem::exists(bakPath)) {
    boost::filesystem::remove(bakPath);
  }

  Tools::ScopeExit tmpFileDeleter([&tmpPath] {
    boost::system::error_code ignore;
    boost::filesystem::remove(tmpPath, ignore);
  });

  // Copy file. It is slow but atomic operation
  FileMappedVector<T> tmpVector;
  tmpVector.create(tmpPath.string(), newCapacity, newPrefixSize, newSuffixSize);
  func(tmpVector);
  tmpVector.flush();

  // Swap files
  std::error_code ec;
  std::error_code ignore;
  m_file.rename(bakPath.string());
  tmpVector.rename(m_path, ec);
  if (ec) {
    // Try to restore and ignore errors
    m_file.rename(m_path, ignore);
    throw std::system_error(ec, "Failed to swap temporary and vector files");
  }

  m_path = bakPath.string();
  swap(tmpVector);
  tmpFileDeleter.cancel();

  // Remove .bak file and ignore errors
  tmpVector.close(ignore);
  boost::system::error_code boostError;
  boost::filesystem::remove(bakPath, boostError);
}

template<class T>
void FileMappedVector<T>::open(const std::string& path, uint64_t prefixSize) {
  m_prefixSize = prefixSize;
  m_file.open(path);
  m_path = path;

  if (m_file.size() < prefixSize + metadataSize) {
    throw std::runtime_error("FileMappedVector::open() file is too small");
  }

  if (size() > capacity()) {
    throw std::runtime_error("FileMappedVector::open() vector size is greater than capacity");
  }

  auto minRequiredFileSize = m_prefixSize + metadataSize + vectorDataSize();
  if (m_file.size() < minRequiredFileSize) {
    throw std::runtime_error("FileMappedVector::open() invalid file size");
  }

  m_suffixSize = m_file.size() - minRequiredFileSize;
}

template<class T>
void FileMappedVector<T>::create(const std::string& path, uint64_t initialCapacity, uint64_t prefixSize, uint64_t suffixSize) {
  m_file.create(path, prefixSize + metadataSize + initialCapacity * valueSize + suffixSize, false);
  m_path = path;
  m_prefixSize = prefixSize;
  m_suffixSize = suffixSize;
  *sizePtr() = 0;
  *capacityPtr() = initialCapacity;
  m_file.flush(reinterpret_cast<uint8_t*>(sizePtr()), metadataSize);
}

template<class T>
uint8_t* FileMappedVector<T>::prefixPtr() {
  return m_file.data();
}

template<class T>
const uint8_t* FileMappedVector<T>::prefixPtr() const {
  return m_file.data();
}

template<class T>
uint64_t* FileMappedVector<T>::capacityPtr() {
  return reinterpret_cast<uint64_t*>(prefixPtr() + m_prefixSize);
}

template<class T>
const uint64_t* FileMappedVector<T>::capacityPtr() const {
  return reinterpret_cast<const uint64_t*>(prefixPtr() + m_prefixSize);
}

template<class T>
const uint64_t* FileMappedVector<T>::sizePtr() const {
  return capacityPtr() + 1;
}

template<class T>
uint64_t* FileMappedVector<T>::sizePtr() {
  return capacityPtr() + 1;
}

template<class T>
T* FileMappedVector<T>::vectorDataPtr() {
  return reinterpret_cast<T*>(sizePtr() + 1);
}

template<class T>
const T* FileMappedVector<T>::vectorDataPtr() const {
  return reinterpret_cast<const T*>(sizePtr() + 1);
}

template<class T>
uint8_t* FileMappedVector<T>::suffixPtr() {
  return reinterpret_cast<uint8_t*>(vectorDataPtr() + capacity());
}

template<class T>
const uint8_t* FileMappedVector<T>::suffixPtr() const {
  return reinterpret_cast<const uint8_t*>(vectorDataPtr() + capacity());
}

template<class T>
uint64_t FileMappedVector<T>::vectorDataSize() {
  return capacity() * valueSize;
}

template<class T>
uint64_t FileMappedVector<T>::nextCapacity() {
  return capacity() + capacity() / 2 + 1;
}

template<class T>
void FileMappedVector<T>::flushElement(uint64_t index) {
  if (m_autoFlush) {
    m_file.flush(reinterpret_cast<uint8_t*>(vectorDataPtr() + index), valueSize);
  }
}

template<class T>
void FileMappedVector<T>::flushSize() {
  if (m_autoFlush) {
    m_file.flush(reinterpret_cast<uint8_t*>(sizePtr()), sizeof(uint64_t));
  }
}

}
