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

enum class TransactionValidationError {
  VALIDATION_SUCCESS = 0,
  EMPTY_INPUTS,
  INPUT_UNKNOWN_TYPE,
  INPUT_EMPTY_OUTPUT_USAGE,
  INPUT_INVALID_DOMAIN_KEYIMAGES,
  INPUT_IDENTICAL_KEYIMAGES,
  INPUT_IDENTICAL_OUTPUT_INDEXES,
  INPUT_KEYIMAGE_ALREADY_SPENT,
  INPUT_MULTISIGNATURE_ALREADY_SPENT,
  INPUT_INVALID_GLOBAL_INDEX,
  INPUT_SPEND_LOCKED_OUT,
  INPUT_INVALID_SIGNATURES,
  INPUT_WRONG_SIGNATURES_COUNT,
  INPUTS_AMOUNT_OVERFLOW,
  INPUT_WRONG_COUNT,
  INPUT_UNEXPECTED_TYPE,
  BASE_INPUT_WRONG_BLOCK_INDEX,
  OUTPUT_ZERO_AMOUNT,
  OUTPUT_INVALID_KEY,
  OUTPUT_INVALID_REQUIRED_SIGNATURES_COUNT,
  OUTPUT_INVALID_MULTISIGNATURE_KEY,
  OUTPUT_UNKNOWN_TYPE,
  OUTPUTS_AMOUNT_OVERFLOW,
  WRONG_AMOUNT,
  WRONG_TRANSACTION_UNLOCK_TIME
};

// custom category:
class TransactionValidationErrorCategory : public std::error_category {
public:
  static TransactionValidationErrorCategory INSTANCE;

  virtual const char* name() const throw() {
    return "TransactionValidationErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const {
    TransactionValidationError code = static_cast<TransactionValidationError>(ev);

    switch (code) {
      case TransactionValidationError::VALIDATION_SUCCESS: return "Transaction successfully validated";
      case TransactionValidationError::EMPTY_INPUTS: return "Transaction has no inputs";
      case TransactionValidationError::INPUT_UNKNOWN_TYPE: return "Transaction has input with unknown type";
      case TransactionValidationError::INPUT_EMPTY_OUTPUT_USAGE: return "Transaction's input uses empty output";
      case TransactionValidationError::INPUT_INVALID_DOMAIN_KEYIMAGES: return "Transaction uses key image not in the valid domain";
      case TransactionValidationError::INPUT_IDENTICAL_KEYIMAGES: return "Transaction has identical key images";
      case TransactionValidationError::INPUT_IDENTICAL_OUTPUT_INDEXES: return "Transaction has identical output indexes";
      case TransactionValidationError::INPUT_KEYIMAGE_ALREADY_SPENT: return "Transaction uses spent key image";
      case TransactionValidationError::INPUT_MULTISIGNATURE_ALREADY_SPENT: return "Transaction uses spent multisignature";
      case TransactionValidationError::INPUT_INVALID_GLOBAL_INDEX: return "Transaction has input with invalid global index";
      case TransactionValidationError::INPUT_SPEND_LOCKED_OUT: return "Transaction uses locked input";
      case TransactionValidationError::INPUT_INVALID_SIGNATURES: return "Transaction has input with invalid signature";
      case TransactionValidationError::INPUT_WRONG_SIGNATURES_COUNT: return "Transaction has input with wrong signatures count";
      case TransactionValidationError::INPUTS_AMOUNT_OVERFLOW: return "Transaction's inputs sum overflow";
      case TransactionValidationError::INPUT_WRONG_COUNT: return "Wrong input count";
      case TransactionValidationError::INPUT_UNEXPECTED_TYPE: return "Wrong input type";
      case TransactionValidationError::BASE_INPUT_WRONG_BLOCK_INDEX: return "Base input has wrong block index";
      case TransactionValidationError::OUTPUT_ZERO_AMOUNT: return "Transaction has zero output amount";
      case TransactionValidationError::OUTPUT_INVALID_KEY: return "Transaction has output with invalid key";
      case TransactionValidationError::OUTPUT_INVALID_REQUIRED_SIGNATURES_COUNT: return "Transaction has output with invalid signatures count";
      case TransactionValidationError::OUTPUT_INVALID_MULTISIGNATURE_KEY: return "Transaction has output with invalid multisignature key";
      case TransactionValidationError::OUTPUT_UNKNOWN_TYPE: return "Transaction has unknown output type";
      case TransactionValidationError::OUTPUTS_AMOUNT_OVERFLOW: return "Transaction has outputs amount overflow";
      case TransactionValidationError::WRONG_AMOUNT: return "Transaction wrong amount";
      case TransactionValidationError::WRONG_TRANSACTION_UNLOCK_TIME: return "Transaction has wrong unlock time";
      default: return "Unknown error";
    }
  }

private:
  TransactionValidationErrorCategory() {
  }
};

inline std::error_code make_error_code(CryptoNote::error::TransactionValidationError e) {
  return std::error_code(static_cast<int>(e), CryptoNote::error::TransactionValidationErrorCategory::INSTANCE);
}

}
}

namespace std {

template <>
struct is_error_code_enum<CryptoNote::error::TransactionValidationError>: public true_type {};

}
