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

#include "BinaryOutputStreamSerializer.h"

#include <cassert>
#include <stdexcept>
#include "Common/StreamTools.h"

using namespace Common;

namespace CryptoNote {

ISerializer::SerializerType BinaryOutputStreamSerializer::type() const {
  return ISerializer::OUTPUT;
}

bool BinaryOutputStreamSerializer::beginObject(Common::StringView name) {
  return true;
}

void BinaryOutputStreamSerializer::endObject() {
}

bool BinaryOutputStreamSerializer::beginArray(size_t& size, Common::StringView name) {
  writeVarint(stream, size);
  return true;
}

void BinaryOutputStreamSerializer::endArray() {
}

bool BinaryOutputStreamSerializer::operator()(uint8_t& value, Common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint16_t& value, Common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int16_t& value, Common::StringView name) {
  writeVarint(stream, static_cast<uint16_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint32_t& value, Common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int32_t& value, Common::StringView name) {
  writeVarint(stream, static_cast<uint32_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int64_t& value, Common::StringView name) {
  writeVarint(stream, static_cast<uint64_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint64_t& value, Common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(bool& value, Common::StringView name) {
  char boolVal = value;
  checkedWrite(&boolVal, 1);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(std::string& value, Common::StringView name) {
  writeVarint(stream, value.size());
  checkedWrite(value.data(), value.size());
  return true;
}

bool BinaryOutputStreamSerializer::binary(void* value, size_t size, Common::StringView name) {
  checkedWrite(static_cast<const char*>(value), size);
  return true;
}

bool BinaryOutputStreamSerializer::binary(std::string& value, Common::StringView name) {
  // write as string (with size prefix)
  return (*this)(value, name);
}

bool BinaryOutputStreamSerializer::operator()(double& value, Common::StringView name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("double serialization is not supported in BinaryOutputStreamSerializer");
  return false;
}

void BinaryOutputStreamSerializer::checkedWrite(const char* buf, size_t size) {
  write(stream, buf, size);
}

}
