// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include "CryptoNoteTools.h"
#include "CryptoNoteFormatUtils.h"

namespace CryptoNote {
template<>
bool toBinaryArray(const BinaryArray& object, BinaryArray& binaryArray) {
  try {
    Common::VectorOutputStream stream(binaryArray);
    BinaryOutputStreamSerializer serializer(stream);
    std::string oldBlob = Common::asString(object);
    serializer(oldBlob, "");
  } catch (std::exception&) {
    return false;
  }

  return true;
}

void getBinaryArrayHash(const BinaryArray& binaryArray, Crypto::Hash& hash) {
  cn_fast_hash(binaryArray.data(), binaryArray.size(), hash);
}

Crypto::Hash getBinaryArrayHash(const BinaryArray& binaryArray) {
  Crypto::Hash hash;
  getBinaryArrayHash(binaryArray, hash);
  return hash;
}

uint64_t getInputAmount(const Transaction& transaction) {
  uint64_t amount = 0;
  for (auto& input : transaction.inputs) {
    if (input.type() == typeid(KeyInput)) {
      amount += boost::get<KeyInput>(input).amount;
    } else if (input.type() == typeid(MultisignatureInput)) {
      amount += boost::get<MultisignatureInput>(input).amount;
    }
  }

  return amount;
}

std::vector<uint64_t> getInputsAmounts(const Transaction& transaction) {
  std::vector<uint64_t> inputsAmounts;
  inputsAmounts.reserve(transaction.inputs.size());

  for (auto& input: transaction.inputs) {
    if (input.type() == typeid(KeyInput)) {
      inputsAmounts.push_back(boost::get<KeyInput>(input).amount);
    } else if (input.type() == typeid(MultisignatureInput)) {
      inputsAmounts.push_back(boost::get<MultisignatureInput>(input).amount);
    }
  }

  return inputsAmounts;
}

uint64_t getOutputAmount(const Transaction& transaction) {
  uint64_t amount = 0;
  for (auto& output : transaction.outputs) {
    amount += output.amount;
  }

  return amount;
}

void decomposeAmount(uint64_t amount, uint64_t dustThreshold, std::vector<uint64_t>& decomposedAmounts) {
  decompose_amount_into_digits(amount, dustThreshold,
    [&](uint64_t amount) {
    decomposedAmounts.push_back(amount);
  },
    [&](uint64_t dust) {
    decomposedAmounts.push_back(dust);
  }
  );
}

}
