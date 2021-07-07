// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2014-2017 XDN developers
// Copyright (c) 2016-2017 BXC developers
// Copyright (c) 2017 UltraNote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BinaryInputStreamSerializer.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <Common/StreamTools.h>
#include "SerializationOverloads.h"

using namespace Common;

namespace CryptoNote {

namespace {

template<typename StorageType, typename T>
void readVarintAs(IInputStream& s, T &i) {
  i = static_cast<T>(readVarint<StorageType>(s));
}

}

ISerializer::SerializerType BinaryInputStreamSerializer::type() const {
  return ISerializer::INPUT;
}

bool BinaryInputStreamSerializer::beginObject(Common::StringView name) {
  return true;
}

void BinaryInputStreamSerializer::endObject() {
}

bool BinaryInputStreamSerializer::beginArray(size_t& size, Common::StringView name) {
  readVarintAs<uint64_t>(stream, size);

  return true;
}

void BinaryInputStreamSerializer::endArray() {
}

bool BinaryInputStreamSerializer::operator()(uint8_t& value, Common::StringView name) {
  readVarint(stream, value);
  return true;
}

bool BinaryInputStreamSerializer::operator()(uint16_t& value, Common::StringView name) {
  readVarint(stream, value);
  return true;
}

bool BinaryInputStreamSerializer::operator()(int16_t& value, Common::StringView name) {
  readVarintAs<uint16_t>(stream, value);
  return true;
}

bool BinaryInputStreamSerializer::operator()(uint32_t& value, Common::StringView name) {
  readVarint(stream, value);
  return true;
}

bool BinaryInputStreamSerializer::operator()(int32_t& value, Common::StringView name) {
  readVarintAs<uint32_t>(stream, value);
  return true;
}

bool BinaryInputStreamSerializer::operator()(int64_t& value, Common::StringView name) {
  readVarintAs<uint64_t>(stream, value);
  return true;
}

bool BinaryInputStreamSerializer::operator()(uint64_t& value, Common::StringView name) {
  readVarint(stream, value);
  return true;
}

bool BinaryInputStreamSerializer::operator()(bool& value, Common::StringView name) {
  value = read<uint8_t>(stream) != 0;
  return true;
}

bool BinaryInputStreamSerializer::operator()(std::string& value, Common::StringView name) {
  uint64_t size;
  readVarint(stream, size);

  if (size > 0) {
    std::vector<char> temp;
    temp.resize(size);
    checkedRead(&temp[0], size);
    value.reserve(size);
    value.assign(&temp[0], size);
  } else {
    value.clear();
  }

  return true;
}

bool BinaryInputStreamSerializer::binary(void* value, size_t size, Common::StringView name) {
  checkedRead(static_cast<char*>(value), size);
  return true;
}

bool BinaryInputStreamSerializer::binary(std::string& value, Common::StringView name) {
  return (*this)(value, name);
}

bool BinaryInputStreamSerializer::operator()(double& value, Common::StringView name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("double serialization is not supported in BinaryInputStreamSerializer");
  return false;
}

void BinaryInputStreamSerializer::checkedRead(char* buf, size_t size) {
  read(stream, buf, size);
}

}
