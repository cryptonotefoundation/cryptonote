// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2014-2017 XDN developers
// Copyright (c) 2016-2017 BXC developers
// Copyright (c) 2017 UltraNote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
  static JsonValue fromStringWithWhiteSpaces(const std::string& source);
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
