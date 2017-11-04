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
#include <system_error>

namespace CryptoNote {
namespace error {

enum HttpParserErrorCodes {
  STREAM_NOT_GOOD = 1,
  END_OF_STREAM,
  UNEXPECTED_SYMBOL,
  EMPTY_HEADER
};

// custom category:
class HttpParserErrorCategory : public std::error_category {
public:
  static HttpParserErrorCategory INSTANCE;

  virtual const char* name() const throw() override {
    return "HttpParserErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const override {
    switch (ev) {
      case STREAM_NOT_GOOD: return "The stream is not good";
      case END_OF_STREAM: return "The stream is ended";
      case UNEXPECTED_SYMBOL: return "Unexpected symbol";
      case EMPTY_HEADER: return "The header name is empty";
      default: return "Unknown error";
    }
  }

private:
  HttpParserErrorCategory() {
  }
};

} //namespace error
} //namespace CryptoNote

inline std::error_code make_error_code(CryptoNote::error::HttpParserErrorCodes e) {
  return std::error_code(static_cast<int>(e), CryptoNote::error::HttpParserErrorCategory::INSTANCE);
}
