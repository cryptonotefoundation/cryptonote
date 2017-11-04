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

enum class WalletServiceErrorCode {
  WRONG_KEY_FORMAT = 1,
  WRONG_PAYMENT_ID_FORMAT,
  WRONG_HASH_FORMAT,
  OBJECT_NOT_FOUND,
  DUPLICATE_KEY
};

// custom category:
class WalletServiceErrorCategory : public std::error_category {
public:
  static WalletServiceErrorCategory INSTANCE;

  virtual const char* name() const throw() override {
    return "WalletServiceErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const override {
    WalletServiceErrorCode code = static_cast<WalletServiceErrorCode>(ev);

    switch (code) {
      case WalletServiceErrorCode::WRONG_KEY_FORMAT: return "Wrong key format";
      case WalletServiceErrorCode::WRONG_PAYMENT_ID_FORMAT: return "Wrong payment id format";
      case WalletServiceErrorCode::WRONG_HASH_FORMAT: return "Wrong block id format";
      case WalletServiceErrorCode::OBJECT_NOT_FOUND: return "Requested object not found";
      case WalletServiceErrorCode::DUPLICATE_KEY: return "Duplicate key";
      default: return "Unknown error";
    }
  }

private:
  WalletServiceErrorCategory() {
  }
};

} //namespace error
} //namespace CryptoNote

inline std::error_code make_error_code(CryptoNote::error::WalletServiceErrorCode e) {
  return std::error_code(static_cast<int>(e), CryptoNote::error::WalletServiceErrorCategory::INSTANCE);
}

namespace std {

template <>
struct is_error_code_enum<CryptoNote::error::WalletServiceErrorCode>: public true_type {};

}
