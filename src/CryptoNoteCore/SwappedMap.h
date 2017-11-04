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

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <unordered_map>
#include <string>
#include <vector>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

template<class Key, class T> class SwappedMap {
private:
  struct Descriptor {
    uint64_t offset;
    uint64_t index;
  };

public:
  typedef typename std::pair<Key, T> value_type;

  class const_iterator {
  public:
    //typedef ptrdiff_t difference_type;
    //typedef std::bidirectional_iterator_tag iterator_category;
    //typedef std::pair<const Key, T>* pointer;
    //typedef std::pair<const Key, T>& reference;
    //typedef std::pair<const Key, T> value_type;

    const_iterator(SwappedMap* swappedMap, typename std::unordered_map<Key, Descriptor>::const_iterator descriptorsIterator) : m_swappedMap(swappedMap), m_descriptorsIterator(descriptorsIterator) {
    }

    const_iterator& operator++() {
      ++m_descriptorsIterator;
      return *this;
    }

    bool operator !=(const_iterator other) const {
      return m_descriptorsIterator != other.m_descriptorsIterator;
    }

    bool operator ==(const_iterator other) const {
      return m_descriptorsIterator == other.m_descriptorsIterator;
    }

    const std::pair<const Key, T>& operator*() const {
      return *m_swappedMap->load(m_descriptorsIterator->first, m_descriptorsIterator->second.offset);
    }

    const std::pair<const Key, T>* operator->() const {
      return m_swappedMap->load(m_descriptorsIterator->first, m_descriptorsIterator->second.offset);
    }

    typename std::unordered_map<Key, Descriptor>::const_iterator innerIterator() const {
      return m_descriptorsIterator;
    }

  private:
    SwappedMap* m_swappedMap;
    typename std::unordered_map<Key, Descriptor>::const_iterator m_descriptorsIterator;
  };

  typedef const_iterator iterator;

  SwappedMap();
  //SwappedMap(const SwappedMap&) = delete;
  ~SwappedMap();
  //SwappedMap& operator=(const SwappedMap&) = delete;

  bool open(const std::string& itemFileName, const std::string& indexFileName, size_t poolSize);
  void close();

  uint64_t size() const;
  const_iterator begin();
  const_iterator end();
  size_t count(const Key& key) const;
  const_iterator find(const Key& key);

  void clear();
  void erase(const_iterator iterator);
  std::pair<const_iterator, bool> insert(const std::pair<const Key, T>& value);

private:
  std::fstream m_itemsFile;
  std::fstream m_indexesFile;
  size_t m_poolSize;
  std::unordered_map<Key, Descriptor> m_descriptors;
  uint64_t m_itemsFileSize;
  std::unordered_map<Key, T> m_items;
  std::list<Key> m_cache;
  std::unordered_map<Key, typename std::list<Key>::iterator> m_cacheIterators;
  uint64_t m_cacheHits;
  uint64_t m_cacheMisses;
  uint64_t descriptorsCounter;

  std::pair<const Key, T>* prepare(const Key& key);
  const std::pair<const Key, T>* load(const Key& key, uint64_t offset);
};

template<class Key, class T> SwappedMap<Key, T>::SwappedMap() {
}

template<class Key, class T> SwappedMap<Key, T>::~SwappedMap() {
  close();
}

template<class Key, class T> bool SwappedMap<Key, T>::open(const std::string& itemFileName, const std::string& indexFileName, size_t poolSize) {
  if (poolSize == 0) {
    return false;
  }
  descriptorsCounter = 0;

  m_itemsFile.open(itemFileName, std::ios::in | std::ios::out | std::ios::binary);
  m_indexesFile.open(indexFileName, std::ios::in | std::ios::out | std::ios::binary);
  if (m_itemsFile && m_indexesFile) {
    uint64_t count;
    m_indexesFile.read(reinterpret_cast<char*>(&count), sizeof count);
    if (!m_indexesFile) {
      return false;
    }

    std::unordered_map<Key, Descriptor> descriptors;
    uint64_t itemsFileSize = 0;
    for (uint64_t i = 0; i < count; ++i) {
      bool valid;
      m_indexesFile.read(reinterpret_cast<char*>(&valid), sizeof valid);
      if (!m_indexesFile) {
        return false;
      }

      Key key;
      m_indexesFile.read(reinterpret_cast<char*>(&key), sizeof key);
      if (!m_indexesFile) {
        return false;
      }

      uint32_t itemSize;
      m_indexesFile.read(reinterpret_cast<char*>(&itemSize), sizeof itemSize);
      if (!m_indexesFile) {
        return false;
      }

      if (valid) {
        Descriptor descriptor = { itemsFileSize, i };
        descriptors.insert(std::make_pair(key, descriptor));
      }
      descriptorsCounter++;
      itemsFileSize += itemSize;
    }

    m_descriptors.swap(descriptors);
    m_itemsFileSize = itemsFileSize;
  } else {
    m_itemsFile.open(itemFileName, std::ios::out | std::ios::binary);
    m_itemsFile.close();
    m_itemsFile.open(itemFileName, std::ios::in | std::ios::out | std::ios::binary);
    m_indexesFile.open(indexFileName, std::ios::out | std::ios::binary);
    uint64_t count = 0;
    m_indexesFile.write(reinterpret_cast<char*>(&count), sizeof count);
    if (!m_indexesFile) {
      return false;
    }

    m_indexesFile.close();
    m_indexesFile.open(indexFileName, std::ios::in | std::ios::out | std::ios::binary);
    m_descriptors.clear();
    m_itemsFileSize = 0;
  }

  m_poolSize = poolSize;
  m_items.clear();
  m_cache.clear();
  m_cacheIterators.clear();
  m_cacheHits = 0;
  m_cacheMisses = 0;
  return true;
}

template<class Key, class T> void SwappedMap<Key, T>::close() {
  std::cout << "SwappedMap cache hits: " << m_cacheHits << ", misses: " << m_cacheMisses << " (" << std::fixed << std::setprecision(2) << static_cast<double>(m_cacheMisses) / (m_cacheHits + m_cacheMisses) * 100 << "%)" << std::endl;
}

template<class Key, class T> uint64_t SwappedMap<Key, T>::size() const {
  return m_descriptors.size();
}

template<class Key, class T> typename SwappedMap<Key, T>::const_iterator SwappedMap<Key, T>::begin() {
  return const_iterator(this, m_descriptors.cbegin());
}

template<class Key, class T> typename SwappedMap<Key, T>::const_iterator SwappedMap<Key, T>::end() {
  return const_iterator(this, m_descriptors.cend());
}

template<class Key, class T> size_t SwappedMap<Key, T>::count(const Key& key) const {
  return m_descriptors.count(key);
}

template<class Key, class T> typename SwappedMap<Key, T>::const_iterator SwappedMap<Key, T>::find(const Key& key) {
  return const_iterator(this, m_descriptors.find(key));
}

template<class Key, class T> void SwappedMap<Key, T>::clear() {
  if (!m_indexesFile) {
    throw std::runtime_error("SwappedMap::clear");
  }

  m_indexesFile.seekp(0);
  uint64_t count = 0;
  m_indexesFile.write(reinterpret_cast<char*>(&count), sizeof count);
  if (!m_indexesFile) {
    throw std::runtime_error("SwappedMap::clear");
  }

  m_descriptors.clear();
  m_itemsFileSize = 0;
  m_items.clear();
  m_cache.clear();
  m_cacheIterators.clear();
  descriptorsCounter = 0;
}

template<class Key, class T> void SwappedMap<Key, T>::erase(const_iterator iterator) {
  if (!m_indexesFile) {
    throw std::runtime_error("SwappedMap::erase");
  }

  typename std::unordered_map<Key, Descriptor>::const_iterator descriptorsIterator = iterator.innerIterator();
  m_indexesFile.seekp(sizeof(uint64_t) + (sizeof(bool) + sizeof(Key) + sizeof(uint32_t)) * descriptorsIterator->second.index);
  bool valid = false;
  m_indexesFile.write(reinterpret_cast<char*>(&valid), sizeof valid);
  if (!m_indexesFile) {
    throw std::runtime_error("SwappedMap::erase");
  }

  m_descriptors.erase(descriptorsIterator);
  auto cacheIteratorsIterator = m_cacheIterators.find(descriptorsIterator->first);
  if (cacheIteratorsIterator != m_cacheIterators.end()) {
    m_items.erase(descriptorsIterator->first);
    m_cache.erase(cacheIteratorsIterator->second);
    m_cacheIterators.erase(cacheIteratorsIterator);
  }
}

template<class Key, class T> std::pair<typename SwappedMap<Key, T>::const_iterator, bool> SwappedMap<Key, T>::insert(const std::pair<const Key, T>& value) {
  uint64_t itemsFileSize;

  {
    if (!m_itemsFile) {
      throw std::runtime_error("SwappedMap::insert");
    }

    m_itemsFile.seekp(m_itemsFileSize);
    try {
      boost::archive::binary_oarchive archive(m_itemsFile);
      archive & value.second;
    } catch (std::exception&) {
      throw std::runtime_error("SwappedMap::insert");
    }

    itemsFileSize = m_itemsFile.tellp();
  }

  {
    if (!m_indexesFile) {
      throw std::runtime_error("SwappedMap::insert");
    }

    m_indexesFile.seekp(sizeof(uint64_t) + (sizeof(bool) + sizeof(Key) + sizeof(uint32_t)) * descriptorsCounter);
    bool valid = true;
    m_indexesFile.write(reinterpret_cast<char*>(&valid), sizeof valid);
    if (!m_indexesFile) {
      throw std::runtime_error("SwappedMap::insert");
    }

    m_indexesFile.write(reinterpret_cast<const char*>(&value.first), sizeof value.first);
    if (!m_indexesFile) {
      throw std::runtime_error("SwappedMap::insert");
    }

    uint32_t itemSize = static_cast<uint32_t>(itemsFileSize - m_itemsFileSize);
    m_indexesFile.write(reinterpret_cast<char*>(&itemSize), sizeof itemSize);
    if (!m_indexesFile) {
      throw std::runtime_error("SwappedMap::insert");
    }

    m_indexesFile.seekp(0);
    uint64_t count = descriptorsCounter + 1;
    m_indexesFile.write(reinterpret_cast<char*>(&count), sizeof count);
    if (!m_indexesFile) {
      throw std::runtime_error("SwappedMap::insert");
    }

  }

  Descriptor descriptor = { m_itemsFileSize, descriptorsCounter };
  auto descriptorsInsert = m_descriptors.insert(std::make_pair(value.first, descriptor));
  m_itemsFileSize = itemsFileSize;

  descriptorsCounter++;

  T* newItem = &prepare(value.first)->second;
  *newItem = value.second;
  return std::make_pair(const_iterator(this, descriptorsInsert.first), true);
}

template<class Key, class T> std::pair<const Key, T>* SwappedMap<Key, T>::prepare(const Key& key) {
  if (m_items.size() == m_poolSize) {
    typename std::list<Key>::iterator cacheIter = m_cache.begin();
    m_items.erase(*cacheIter);
    m_cacheIterators.erase(*cacheIter);
    m_cache.erase(cacheIter);
  }

  std::pair<typename std::unordered_map<Key, T>::iterator, bool> itemInsert = m_items.insert(std::make_pair(key, T()));
  typename std::list<Key>::iterator cacheIter = m_cache.insert(m_cache.end(), key);
  m_cacheIterators.insert(std::make_pair(key, cacheIter));
  return &*itemInsert.first;
}

template<class Key, class T> const std::pair<const Key, T>* SwappedMap<Key, T>::load(const Key& key, uint64_t offset) {
  auto itemIterator = m_items.find(key);
  if (itemIterator != m_items.end()) {
    auto cacheIteratorsIterator = m_cacheIterators.find(key);
    if (cacheIteratorsIterator->second != --m_cache.end()) {
      m_cache.splice(m_cache.end(), m_cache, cacheIteratorsIterator->second);
    }

    ++m_cacheHits;
    return &*itemIterator;
  }

  typename std::unordered_map<Key, Descriptor>::iterator descriptorsIterator = m_descriptors.find(key);
  if (descriptorsIterator == m_descriptors.end()) {
    throw std::runtime_error("SwappedMap::load");
  }

  if (!m_itemsFile) {
    throw std::runtime_error("SwappedMap::load");
  }

  m_itemsFile.seekg(descriptorsIterator->second.offset);
  T tempItem;
  try {
    boost::archive::binary_iarchive archive(m_itemsFile);
    archive & tempItem;
  } catch (std::exception&) {
    throw std::runtime_error("SwappedMap::load");
  }

  std::pair<const Key, T>* item = prepare(key);
  std::swap(tempItem, item->second);
  ++m_cacheMisses;
  return item;
}
