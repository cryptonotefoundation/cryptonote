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

enum class BlockchainExplorerErrorCodes : int {
  NOT_INITIALIZED = 1,
  ALREADY_INITIALIZED,
  INTERNAL_ERROR,
  REQUEST_ERROR
};

class BlockchainExplorerErrorCategory : public std::error_category {
public:
  static BlockchainExplorerErrorCategory INSTANCE;

  virtual const char* name() const throw() override {
    return "BlockchainExplorerErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const override {
    switch (ev) {
      case static_cast<int>(BlockchainExplorerErrorCodes::NOT_INITIALIZED):     return "Object was not initialized";
      case static_cast<int>(BlockchainExplorerErrorCodes::ALREADY_INITIALIZED): return "Object has been already initialized";
      case static_cast<int>(BlockchainExplorerErrorCodes::INTERNAL_ERROR):      return "Internal error";
      case static_cast<int>(BlockchainExplorerErrorCodes::REQUEST_ERROR):       return "Error in request parameters";
      default:                                                                  return "Unknown error";
    }
  }

private:
  BlockchainExplorerErrorCategory() {
  }
};

} //namespace error
} //namespace CryptoNote

inline std::error_code make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes e) {
  return std::error_code(static_cast<int>(e), CryptoNote::error::BlockchainExplorerErrorCategory::INSTANCE);
}

