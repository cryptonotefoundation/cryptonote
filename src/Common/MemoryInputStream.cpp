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

#include "MemoryInputStream.h"
#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

namespace Common {

MemoryInputStream::MemoryInputStream(const void* buffer, size_t bufferSize) : 
buffer(static_cast<const char*>(buffer)), bufferSize(bufferSize), position(0) {}

size_t MemoryInputStream::getPosition() const {
  return position;
}

bool MemoryInputStream::endOfStream() const {
  return position == bufferSize;
}

size_t MemoryInputStream::readSome(void* data, size_t size) {
  assert(position <= bufferSize);
  size_t readSize = std::min(size, bufferSize - position);

  if (readSize > 0) {
    memcpy(data, buffer + position, readSize);
    position += readSize;
  }
  
  return readSize;
}

}
