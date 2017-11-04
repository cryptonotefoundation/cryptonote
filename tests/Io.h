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

#include <cstddef>
#include <ios>
#include <iostream>
#include <type_traits>
#include <vector>

inline bool hexdecode(const char *from, std::size_t length, void *to) {
  std::size_t i;
  for (i = 0; i < length; i++) {
    int v = 0;
    if (from[2 * i] >= '0' && from[2 * i] <= '9') {
      v = from[2 * i] - '0';
    } else if (from[2 * i] >= 'a' && from[2 * i] <= 'f') {
      v = from[2 * i] - 'a' + 10;
    } else {
      return false;
    }
    v <<= 4;
    if (from[2 * i + 1] >= '0' && from[2 * i + 1] <= '9') {
      v |= from[2 * i + 1] - '0';
    } else if (from[2 * i + 1] >= 'a' && from[2 * i + 1] <= 'f') {
      v |= from[2 * i + 1] - 'a' + 10;
    } else {
      return false;
    }
    *(reinterpret_cast<unsigned char *>(to) + i) = v;
  }
  return true;
}

inline void get(std::istream &input, bool &res) {
  std::string sres;
  input >> sres;
  if (sres == "false") {
    res = false;
  } else if (sres == "true") {
    res = true;
  } else {
    input.setstate(std::ios_base::failbit);
  }
}

template<typename T>
typename std::enable_if<std::is_integral<T>::value, void>::type
get(std::istream &input, T &res) {
  input >> res;
}

inline void getvar(std::istream &input, std::size_t length, void *res) {
  std::string sres;
  input >> sres;
  if (sres.length() != 2 * length || !hexdecode(sres.data(), length, res)) {
    input.setstate(std::ios_base::failbit);
  }
}

template<typename T>
typename std::enable_if<std::is_standard_layout<T>::value && !std::is_scalar<T>::value, void>::type
get(std::istream &input, T &res) {
  getvar(input, sizeof(T), &res);
}

inline void get(std::istream &input, std::vector<char> &res) {
  std::string sres;
  input >> sres;
  if (sres == "x") {
    res.clear();
  } else if (sres.length() % 2 != 0) {
    input.setstate(std::ios_base::failbit);
  } else {
    std::size_t length = sres.length() / 2;
    res.resize(length);
    if (!hexdecode(sres.data(), length, res.data())) {
      input.setstate(std::ios_base::failbit);
    }
  }
}

#if !defined(_MSC_VER) || _MSC_VER >= 1800

template<typename T, typename... TT>
typename std::enable_if<(sizeof...(TT) > 0), void>::type
get(std::istream &input, T &res, TT &... resres) {
  get(input, res);
  get(input, resres...);
}

#else
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition/enum_binary_params.hpp>
#include <boost/preprocessor/repetition/enum_params.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#define NESTED_GET(z, n, data) get(input, BOOST_PP_CAT(res, n));
#define GET(z, n, data) \
template<BOOST_PP_ENUM_PARAMS(n, typename T)> \
void get(std::istream &input, BOOST_PP_ENUM_BINARY_PARAMS(n, T, &res)) { \
  BOOST_PP_REPEAT(n, NESTED_GET, ~) \
}
BOOST_PP_REPEAT_FROM_TO(2, 5, GET, ~)

#endif
