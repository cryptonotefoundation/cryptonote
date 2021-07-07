// Copyright (c) 2012-2018, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2020, The Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstddef>
#include <cstring>
#include <functional>
#include "crypto-util.h"

#define CRYPTO_MAKE_COMPARABLE(type, cmp)                                                                 \
namespace Crypto { \
	inline bool operator==(const type &_v1, const type &_v2) { return cmp(&_v1, &_v2, sizeof(type)) == 0; } \
	inline bool operator!=(const type &_v1, const type &_v2) { return cmp(&_v1, &_v2, sizeof(type)) != 0; } \
	inline bool operator<(const type &_v1, const type &_v2) { return cmp(&_v1, &_v2, sizeof(type)) < 0; }   \
	inline bool operator<=(const type &_v1, const type &_v2) { return cmp(&_v1, &_v2, sizeof(type)) <= 0; } \
	inline bool operator>(const type &_v1, const type &_v2) { return cmp(&_v1, &_v2, sizeof(type)) > 0; }   \
	inline bool operator>=(const type &_v1, const type &_v2) { return cmp(&_v1, &_v2, sizeof(type)) >= 0; } \
}

#define CRYPTO_MAKE_HASHABLE(type)                                                                        \
namespace Crypto {                                                                                        \
  static_assert(sizeof(size_t) <= sizeof(type), "Size of " #type " must be at least that of size_t");     \
  inline size_t hash_value(const type &_v) {                                                              \
    return reinterpret_cast<const size_t &>(_v);                                                          \
  }                                                                                                       \
}                                                                                                         \
namespace std {                                                                                           \
  template<>                                                                                              \
  struct hash<Crypto::type> {                                                                             \
    size_t operator()(const Crypto::type &_v) const {                                                     \
      return reinterpret_cast<const size_t &>(_v);                                                        \
    }                                                                                                     \
  };                                                                                                      \
}
