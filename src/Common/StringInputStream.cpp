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

#include "StringInputStream.h"
#include <string.h>

namespace Common {

StringInputStream::StringInputStream(const std::string& in) : in(in), offset(0) {
}

size_t StringInputStream::readSome(void* data, size_t size) {
  if (size > in.size() - offset) {
    size = in.size() - offset;
  }

  memcpy(data, in.data() + offset, size);
  offset += size;
  return size;
}

}
