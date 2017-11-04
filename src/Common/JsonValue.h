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

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Common {

class JsonValue {
public:
  typedef std::string Key;

  typedef std::vector<JsonValue> Array;
  typedef bool Bool;
  typedef int64_t Integer;
  typedef std::nullptr_t Nil;
  typedef std::map<Key, JsonValue> Object;
  typedef double Real;
  typedef std::string String;

  enum Type {
    ARRAY,
    BOOL,
    INTEGER,
    NIL,
    OBJECT,
    REAL,
    STRING
  };

  JsonValue();
  JsonValue(const JsonValue& other);
  JsonValue(JsonValue&& other);
  JsonValue(Type valueType);
  JsonValue(const Array& value);
  JsonValue(Array&& value);
  explicit JsonValue(Bool value);
  JsonValue(Integer value);
  JsonValue(Nil value);
  JsonValue(const Object& value);
  JsonValue(Object&& value);
  JsonValue(Real value);
  JsonValue(const String& value);
  JsonValue(String&& value);
  template<size_t size> JsonValue(const char(&value)[size]) {
    new(valueString)String(value, size - 1);
    type = STRING;
  }

  ~JsonValue();

  JsonValue& operator=(const JsonValue& other);
  JsonValue& operator=(JsonValue&& other);
  JsonValue& operator=(const Array& value);
  JsonValue& operator=(Array&& value);
  //JsonValue& operator=(Bool value);
  JsonValue& operator=(Integer value);
  JsonValue& operator=(Nil value);
  JsonValue& operator=(const Object& value);
  JsonValue& operator=(Object&& value);
  JsonValue& operator=(Real value);
  JsonValue& operator=(const String& value);
  JsonValue& operator=(String&& value);
  template<size_t size> JsonValue& operator=(const char(&value)[size]) {
    if (type != STRING) {
      destructValue();
      type = NIL;
      new(valueString)String(value, size - 1);
      type = STRING;
    } else {
      reinterpret_cast<String*>(valueString)->assign(value, size - 1);
    }

    return *this;
  }

  bool isArray() const;
  bool isBool() const;
  bool isInteger() const;
  bool isNil() const;
  bool isObject() const;
  bool isReal() const;
  bool isString() const;

  Type getType() const;
  Array& getArray();
  const Array& getArray() const;
  Bool getBool() const;
  Integer getInteger() const;
  Object& getObject();
  const Object& getObject() const;
  Real getReal() const;
  String& getString();
  const String& getString() const;

  size_t size() const;

  JsonValue& operator[](size_t index);
  const JsonValue& operator[](size_t index) const;
  JsonValue& pushBack(const JsonValue& value);
  JsonValue& pushBack(JsonValue&& value);

  JsonValue& operator()(const Key& key);
  const JsonValue& operator()(const Key& key) const;
  bool contains(const Key& key) const;
  JsonValue& insert(const Key& key, const JsonValue& value);
  JsonValue& insert(const Key& key, JsonValue&& value);

  // sets or creates value, returns reference to self
  JsonValue& set(const Key& key, const JsonValue& value);
  JsonValue& set(const Key& key, JsonValue&& value);

  size_t erase(const Key& key);

  static JsonValue fromString(const std::string& source);
  std::string toString() const;

  friend std::ostream& operator<<(std::ostream& out, const JsonValue& jsonValue);
  friend std::istream& operator>>(std::istream& in, JsonValue& jsonValue);

private:
  Type type;
  union {
    uint8_t valueArray[sizeof(Array)];
    Bool valueBool;
    Integer valueInteger;
    uint8_t valueObject[sizeof(Object)];
    Real valueReal;
    uint8_t valueString[sizeof(std::string)];
  };

  void destructValue();

  void readArray(std::istream& in);
  void readTrue(std::istream& in);
  void readFalse(std::istream& in);
  void readNull(std::istream& in);
  void readNumber(std::istream& in, char c);
  void readObject(std::istream& in);
  void readString(std::istream& in);
};

}
