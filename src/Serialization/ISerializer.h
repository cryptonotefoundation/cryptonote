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

#include <string>
#include <cstdint>

#include <Common/StringView.h>

namespace CryptoNote {

class ISerializer {
public:

  enum SerializerType {
    INPUT,
    OUTPUT
  };

  virtual ~ISerializer() {}

  virtual SerializerType type() const = 0;

  virtual bool beginObject(Common::StringView name) = 0;
  virtual void endObject() = 0;
  virtual bool beginArray(size_t& size, Common::StringView name) = 0;
  virtual void endArray() = 0;

  virtual bool operator()(uint8_t& value, Common::StringView name) = 0;
  virtual bool operator()(int16_t& value, Common::StringView name) = 0;
  virtual bool operator()(uint16_t& value, Common::StringView name) = 0;
  virtual bool operator()(int32_t& value, Common::StringView name) = 0;
  virtual bool operator()(uint32_t& value, Common::StringView name) = 0;
  virtual bool operator()(int64_t& value, Common::StringView name) = 0;
  virtual bool operator()(uint64_t& value, Common::StringView name) = 0;
  virtual bool operator()(double& value, Common::StringView name) = 0;
  virtual bool operator()(bool& value, Common::StringView name) = 0;
  virtual bool operator()(std::string& value, Common::StringView name) = 0;
  
  // read/write binary block
  virtual bool binary(void* value, size_t size, Common::StringView name) = 0;
  virtual bool binary(std::string& value, Common::StringView name) = 0;

  template<typename T>
  bool operator()(T& value, Common::StringView name);
};

template<typename T>
bool ISerializer::operator()(T& value, Common::StringView name) {
  return serialize(value, name, *this);
}

template<typename T>
bool serialize(T& value, Common::StringView name, ISerializer& serializer) {
  if (!serializer.beginObject(name)) {
    return false;
  }

  serialize(value, serializer);
  serializer.endObject();
  return true;
}

template<typename T>
void serialize(T& value, ISerializer& serializer) {
  value.serialize(serializer);
}

#ifdef __clang__
template<> inline
bool ISerializer::operator()(size_t& value, Common::StringView name) {
  return operator()(*reinterpret_cast<uint64_t*>(&value), name);
}
#endif

#define KV_MEMBER(member) s(member, #member);

}
