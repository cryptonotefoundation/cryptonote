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

#include "Ipv4Address.h"
#include <stdexcept>

namespace System {

namespace {

uint8_t readUint8(const std::string& source, size_t& offset) {
  if (offset == source.size() || source[offset] < '0' || source[offset] > '9') {
    throw std::runtime_error("Unable to read value from string");
  }

  uint8_t value = source[offset] - '0';
  if (offset + 1 == source.size() || source[offset + 1] < '0' || source[offset + 1] > '9') {
    offset = offset + 1;
    return value;
  }

  if (value == 0) {
    throw std::runtime_error("Unable to read value from string");
  }

  value = value * 10 + (source[offset + 1] - '0');
  if (offset + 2 == source.size() || source[offset + 2] < '0' || source[offset + 2] > '9') {
    offset = offset + 2;
    return value;
  }

  if ((value == 25 && source[offset + 2] > '5') || value > 25) {
    throw std::runtime_error("Unable to read value from string");
  }

  value = value * 10 + (source[offset + 2] - '0');
  offset = offset + 3;
  return value;
}

}

Ipv4Address::Ipv4Address(uint32_t value) : value(value) {
}

Ipv4Address::Ipv4Address(const std::string& dottedDecimal) {
  size_t offset = 0;
  value = readUint8(dottedDecimal, offset);
  if (offset == dottedDecimal.size() || dottedDecimal[offset] != '.') {
    throw std::runtime_error("Invalid Ipv4 address string");
  }

  ++offset;
  value = value << 8 | readUint8(dottedDecimal, offset);
  if (offset == dottedDecimal.size() || dottedDecimal[offset] != '.') {
    throw std::runtime_error("Invalid Ipv4 address string");
  }

  ++offset;
  value = value << 8 | readUint8(dottedDecimal, offset);
  if (offset == dottedDecimal.size() || dottedDecimal[offset] != '.') {
    throw std::runtime_error("Invalid Ipv4 address string");
  }

  ++offset;
  value = value << 8 | readUint8(dottedDecimal, offset);
  if (offset < dottedDecimal.size()) {
    throw std::runtime_error("Invalid Ipv4 address string");
  }
}

bool Ipv4Address::operator!=(const Ipv4Address& other) const {
  return value != other.value;
}

bool Ipv4Address::operator==(const Ipv4Address& other) const {
  return value == other.value;
}

uint32_t Ipv4Address::getValue() const {
  return value;
}

std::string Ipv4Address::toDottedDecimal() const {
  std::string result;
  result += std::to_string(value >> 24);
  result += '.';
  result += std::to_string(value >> 16 & 255);
  result += '.';
  result += std::to_string(value >> 8 & 255);
  result += '.';
  result += std::to_string(value & 255);
  return result;
}

bool Ipv4Address::isLoopback() const {
  // 127.0.0.0/8
  return (value & 0xff000000) == (127 << 24);
}

bool Ipv4Address::isPrivate() const {
  return
    // 10.0.0.0/8
    (value & 0xff000000) == (10 << 24) ||
    // 172.16.0.0/12
    (value & 0xfff00000) == ((172 << 24) | (16 << 16)) ||
    // 192.168.0.0/16
    (value & 0xffff0000) == ((192 << 24) | (168 << 16));
}

}
