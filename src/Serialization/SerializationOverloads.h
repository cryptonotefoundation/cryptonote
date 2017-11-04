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

#include "ISerializer.h"

#include <array>
#include <cstring>
#include <list>
#include <map>
#include <string>
#include <set>
#include <type_traits>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace CryptoNote {

template<typename T>
typename std::enable_if<std::is_pod<T>::value>::type
serializeAsBinary(std::vector<T>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  std::string blob;
  if (serializer.type() == ISerializer::INPUT) {
    serializer.binary(blob, name);
    value.resize(blob.size() / sizeof(T));
    if (blob.size()) {
      memcpy(&value[0], blob.data(), blob.size());
    }
  } else {
    if (!value.empty()) {
      blob.assign(reinterpret_cast<const char*>(&value[0]), value.size() * sizeof(T));
    }
    serializer.binary(blob, name);
  }
}

template<typename T>
typename std::enable_if<std::is_pod<T>::value>::type
serializeAsBinary(std::list<T>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  std::string blob;
  if (serializer.type() == ISerializer::INPUT) {
    serializer.binary(blob, name);

    size_t count = blob.size() / sizeof(T);
    const T* ptr = reinterpret_cast<const T*>(blob.data());

    while (count--) {
      value.push_back(*ptr++);
    }
  } else {
    if (!value.empty()) {
      blob.resize(value.size() * sizeof(T));
      T* ptr = reinterpret_cast<T*>(&blob[0]);

      for (const auto& item : value) {
        *ptr++ = item;
      }
    }
    serializer.binary(blob, name);
  }
}

template <typename Cont>
bool serializeContainer(Cont& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  size_t size = value.size();
  if (!serializer.beginArray(size, name)) {
    if (serializer.type() == ISerializer::INPUT) {
      value.clear();
    }

    return false;
  }

  value.resize(size);

  for (auto& item : value) {
    serializer(const_cast<typename Cont::value_type&>(item), "");
  }

  serializer.endArray();
  return true;
}

template <typename E>
bool serializeEnumClass(E& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  static_assert(std::is_enum<E>::value, "E must be an enum class");

  typedef typename std::underlying_type<E>::type EType;

  if (serializer.type() == CryptoNote::ISerializer::INPUT) {
    EType numericValue;
    serializer(numericValue, name);
    value = static_cast<E>(numericValue);
  } else {
    auto numericValue = static_cast<EType>(value);
    serializer(numericValue, name);
  }

  return true;
}

template<typename T>
bool serialize(std::vector<T>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeContainer(value, name, serializer);
}

template<typename T>
bool serialize(std::list<T>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeContainer(value, name, serializer);
}

template<typename MapT, typename ReserveOp>
bool serializeMap(MapT& value, Common::StringView name, CryptoNote::ISerializer& serializer, ReserveOp reserve) {
  size_t size = value.size();

  if (!serializer.beginArray(size, name)) {
    if (serializer.type() == ISerializer::INPUT) {
      value.clear();
    }

    return false;
  }

  if (serializer.type() == CryptoNote::ISerializer::INPUT) {
    reserve(size);

    for (size_t i = 0; i < size; ++i) {
      typename MapT::key_type key;
      typename MapT::mapped_type v;

      serializer.beginObject("");
      serializer(key, "key");
      serializer(v, "value");
      serializer.endObject();

      value.insert(std::make_pair(std::move(key), std::move(v)));
    }
  } else {
    for (auto& kv : value) {
      serializer.beginObject("");
      serializer(const_cast<typename MapT::key_type&>(kv.first), "key");
      serializer(kv.second, "value");
      serializer.endObject();
    }
  }

  serializer.endArray();
  return true;
}

template<typename SetT>
bool serializeSet(SetT& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  size_t size = value.size();

  if (!serializer.beginArray(size, name)) {
    if (serializer.type() == ISerializer::INPUT) {
      value.clear();
    }

    return false;
  }

  if (serializer.type() == CryptoNote::ISerializer::INPUT) {
    for (size_t i = 0; i < size; ++i) {
      typename SetT::value_type key;
      serializer(key, "");
      value.insert(std::move(key));
    }
  } else {
    for (auto& key : value) {
      serializer(const_cast<typename SetT::value_type&>(key), "");
    }
  }

  serializer.endArray();
  return true;
}

template<typename K, typename Hash>
bool serialize(std::unordered_set<K, Hash>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeSet(value, name, serializer);
}

template<typename K, typename Cmp>
bool serialize(std::set<K, Cmp>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeSet(value, name, serializer);
}

template<typename K, typename V, typename Hash>
bool serialize(std::unordered_map<K, V, Hash>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeMap(value, name, serializer, [&value](size_t size) { value.reserve(size); });
}

template<typename K, typename V, typename Hash>
bool serialize(std::unordered_multimap<K, V, Hash>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeMap(value, name, serializer, [&value](size_t size) { value.reserve(size); });
}

template<typename K, typename V, typename Hash>
bool serialize(std::map<K, V, Hash>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeMap(value, name, serializer, [](size_t size) {});
}

template<typename K, typename V, typename Hash>
bool serialize(std::multimap<K, V, Hash>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeMap(value, name, serializer, [](size_t size) {});
}

template<size_t size>
bool serialize(std::array<uint8_t, size>& value, Common::StringView name, CryptoNote::ISerializer& s) {
  return s.binary(value.data(), value.size(), name);
}

template <typename T1, typename T2> 
void serialize(std::pair<T1, T2>& value, ISerializer& s) {
  s(value.first, "first");
  s(value.second, "second");
}

template <typename Element, typename Iterator>
void writeSequence(Iterator begin, Iterator end, Common::StringView name, ISerializer& s) {
  size_t size = std::distance(begin, end);
  s.beginArray(size, name);
  for (Iterator i = begin; i != end; ++i) {
    s(const_cast<Element&>(*i), "");
  }
  s.endArray();
}

template <typename Element, typename Iterator>
void readSequence(Iterator outputIterator, Common::StringView name, ISerializer& s) {
  size_t size = 0;
  // array of zero size is not written in KVBinaryOutputStreamSerializer
  if (!s.beginArray(size, name)) {
    return;
  }

  while (size--) {
    Element e;
    s(e, "");
    *outputIterator++ = std::move(e);
  }

  s.endArray();
}

//convinience function since we change block height type
void serializeBlockHeight(ISerializer& s, uint32_t& blockHeight, Common::StringView name);

//convinience function since we change global output index type
void serializeGlobalOutputIndex(ISerializer& s, uint32_t& globalOutputIndex, Common::StringView name);

}
