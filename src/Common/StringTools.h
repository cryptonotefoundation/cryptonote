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

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Common {

std::string asString(const void* data, size_t size); // Does not throw
std::string asString(const std::vector<uint8_t>& data); // Does not throw
std::vector<uint8_t> asBinaryArray(const std::string& data);

uint8_t fromHex(char character); // Returns value of hex 'character', throws on error
bool fromHex(char character, uint8_t& value); // Assigns value of hex 'character' to 'value', returns false on error, does not throw
size_t fromHex(const std::string& text, void* data, size_t bufferSize); // Assigns values of hex 'text' to buffer 'data' up to 'bufferSize', returns actual data size, throws on error
bool fromHex(const std::string& text, void* data, size_t bufferSize, size_t& size); // Assigns values of hex 'text' to buffer 'data' up to 'bufferSize', assigns actual data size to 'size', returns false on error, does not throw
std::vector<uint8_t> fromHex(const std::string& text); // Returns values of hex 'text', throws on error
bool fromHex(const std::string& text, std::vector<uint8_t>& data); // Appends values of hex 'text' to 'data', returns false on error, does not throw

template <typename T>
bool podFromHex(const std::string& text, T& val) {
  size_t outSize;
  return fromHex(text, &val, sizeof(val), outSize) && outSize == sizeof(val);
}

std::string toHex(const void* data, size_t size); // Returns hex representation of ('data', 'size'), does not throw
void toHex(const void* data, size_t size, std::string& text); // Appends hex representation of ('data', 'size') to 'text', does not throw
std::string toHex(const std::vector<uint8_t>& data); // Returns hex representation of 'data', does not throw
void toHex(const std::vector<uint8_t>& data, std::string& text); // Appends hex representation of 'data' to 'text', does not throw

template<class T>
std::string podToHex(const T& s) {
  return toHex(&s, sizeof(s));
}

std::string extract(std::string& text, char delimiter); // Does not throw
std::string extract(const std::string& text, char delimiter, size_t& offset); // Does not throw

template<typename T> T fromString(const std::string& text) { // Throws on error
  T value;
  std::istringstream stream(text);
  stream >> value;
  if (stream.fail()) {
    throw std::runtime_error("fromString: unable to parse value");
  }

  return value;
}

template<typename T> bool fromString(const std::string& text, T& value) { // Does not throw
  std::istringstream stream(text);
  stream >> value;
  return !stream.fail();
}

template<typename T> std::vector<T> fromDelimitedString(const std::string& source, char delimiter) { // Throws on error
  std::vector<T> data;
  for (size_t offset = 0; offset != source.size();) {
    data.emplace_back(fromString<T>(extract(source, delimiter, offset)));
  }

  return data;
}

template<typename T> bool fromDelimitedString(const std::string& source, char delimiter, std::vector<T>& data) { // Does not throw
  for (size_t offset = 0; offset != source.size();) {
    T value;
    if (!fromString<T>(extract(source, delimiter, offset), value)) {
      return false;
    }

    data.emplace_back(value);
  }

  return true;
}

template<typename T> std::string toString(const T& value) { // Does not throw
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

template<typename T> void toString(const T& value, std::string& text) { // Does not throw
  std::ostringstream stream;
  stream << value;
  text += stream.str();
}

bool loadFileToString(const std::string& filepath, std::string& buf);
bool saveStringToFile(const std::string& filepath, const std::string& buf);


std::string base64Decode(std::string const& encoded_string);

std::string ipAddressToString(uint32_t ip);
bool parseIpAddressAndPort(uint32_t& ip, uint32_t& port, const std::string& addr);

std::string timeIntervalToString(uint64_t intervalInSeconds);

}
