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

#include "JsonInputValueSerializer.h"

#include <cassert>
#include <stdexcept>

#include "Common/StringTools.h"

using Common::JsonValue;
using namespace CryptoNote;

JsonInputValueSerializer::JsonInputValueSerializer(const Common::JsonValue& value) {
  if (!value.isObject()) {
    throw std::runtime_error("Serializer doesn't support this type of serialization: Object expected.");
  }

  chain.push_back(&value);
}

JsonInputValueSerializer::JsonInputValueSerializer(Common::JsonValue&& value) : value(std::move(value)) {
  if (!this->value.isObject()) {
    throw std::runtime_error("Serializer doesn't support this type of serialization: Object expected.");
  }

  chain.push_back(&this->value);
}

JsonInputValueSerializer::~JsonInputValueSerializer() {
}

ISerializer::SerializerType JsonInputValueSerializer::type() const {
  return ISerializer::INPUT;
}

bool JsonInputValueSerializer::beginObject(Common::StringView name) {
  const JsonValue* parent = chain.back();

  if (parent->isArray()) {
    const JsonValue& v = (*parent)[idxs.back()++];
    chain.push_back(&v);
    return true;
  }

  if (parent->contains(std::string(name))) {
    const JsonValue& v = (*parent)(std::string(name));
    chain.push_back(&v);
    return true;
  }

  return false;
}

void JsonInputValueSerializer::endObject() {
  assert(!chain.empty());
  chain.pop_back();
}

bool JsonInputValueSerializer::beginArray(size_t& size, Common::StringView name) {
  const JsonValue* parent = chain.back();
  std::string strName(name);

  if (parent->contains(strName)) {
    const JsonValue& arr = (*parent)(strName);
    size = arr.size();
    chain.push_back(&arr);
    idxs.push_back(0);
    return true;
  }
 
  size = 0;
  return false;
}

void JsonInputValueSerializer::endArray() {
  assert(!chain.empty());
  assert(!idxs.empty());

  chain.pop_back();
  idxs.pop_back();
}

bool JsonInputValueSerializer::operator()(uint16_t& value, Common::StringView name) {
  return getNumber(name, value);
}

bool JsonInputValueSerializer::operator()(int16_t& value, Common::StringView name) {
  return getNumber(name, value);
}

bool JsonInputValueSerializer::operator()(uint32_t& value, Common::StringView name) {
  return getNumber(name, value);
}

bool JsonInputValueSerializer::operator()(int32_t& value, Common::StringView name) {
  return getNumber(name, value);
}

bool JsonInputValueSerializer::operator()(int64_t& value, Common::StringView name) {
  return getNumber(name, value);
}

bool JsonInputValueSerializer::operator()(uint64_t& value, Common::StringView name) {
  return getNumber(name, value);
}

bool JsonInputValueSerializer::operator()(double& value, Common::StringView name) {
  return getNumber(name, value);
}

bool JsonInputValueSerializer::operator()(uint8_t& value, Common::StringView name) {
  return getNumber(name, value);
}

bool JsonInputValueSerializer::operator()(std::string& value, Common::StringView name) {
  auto ptr = getValue(name);
  if (ptr == nullptr) {
    return false;
  }
  value = ptr->getString();
  return true;
}

bool JsonInputValueSerializer::operator()(bool& value, Common::StringView name) {
  auto ptr = getValue(name);
  if (ptr == nullptr) {
    return false;
  }
  value = ptr->getBool();
  return true;
}

bool JsonInputValueSerializer::binary(void* value, size_t size, Common::StringView name) {
  auto ptr = getValue(name);
  if (ptr == nullptr) {
    return false;
  }

  Common::fromHex(ptr->getString(), value, size);
  return true;
}

bool JsonInputValueSerializer::binary(std::string& value, Common::StringView name) {
  auto ptr = getValue(name);
  if (ptr == nullptr) {
    return false;
  }

  std::string valueHex = ptr->getString();
  value = Common::asString(Common::fromHex(valueHex));

  return true;
}

const JsonValue* JsonInputValueSerializer::getValue(Common::StringView name) {
  const JsonValue& val = *chain.back();
  if (val.isArray()) {
    return &val[idxs.back()++];
  }

  std::string strName(name);
  return val.contains(strName) ? &val(strName) : nullptr;
}
