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

enum class AddBlockErrorCode {
  ADDED_TO_MAIN = 1,
  ADDED_TO_ALTERNATIVE,
  ADDED_TO_ALTERNATIVE_AND_SWITCHED,
  ALREADY_EXISTS,
  REJECTED_AS_ORPHANED,
  DESERIALIZATION_FAILED
};

// custom category:
class AddBlockErrorCategory : public std::error_category {
public:
  static AddBlockErrorCategory INSTANCE;

  virtual const char* name() const throw() {
    return "AddBlockErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const {
    AddBlockErrorCode code = static_cast<AddBlockErrorCode>(ev);

    switch (code) {
      case AddBlockErrorCode::ADDED_TO_MAIN: return "Block added to main chain";
      case AddBlockErrorCode::ADDED_TO_ALTERNATIVE: return "Block added to alternative chain";
      case AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED: return "Chain switched";
      case AddBlockErrorCode::ALREADY_EXISTS: return "Block already exists";
      case AddBlockErrorCode::REJECTED_AS_ORPHANED: return "Block rejected as orphaned";
      case AddBlockErrorCode::DESERIALIZATION_FAILED: return "Deserialization error";
      default: return "Unknown error";
    }
  }

private:
  AddBlockErrorCategory() {
  }
};

inline std::error_code make_error_code(CryptoNote::error::AddBlockErrorCode e) {
  return std::error_code(static_cast<int>(e), CryptoNote::error::AddBlockErrorCategory::INSTANCE);
}

}
}

namespace std {

template <>
struct is_error_code_enum<CryptoNote::error::AddBlockErrorCode>: public true_type {};

}
