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

#include "CachedTransaction.h"
#include <Common/Varint.h>
#include "CryptoNoteConfig.h"
#include "CryptoNoteTools.h"

using namespace Crypto;
using namespace CryptoNote;

CachedTransaction::CachedTransaction(Transaction&& transaction) : transaction(std::move(transaction)) {
}

CachedTransaction::CachedTransaction(const Transaction& transaction) : transaction(transaction) {
}

CachedTransaction::CachedTransaction(const BinaryArray& transactionBinaryArray) : transactionBinaryArray(transactionBinaryArray) {
  if (!fromBinaryArray<Transaction>(transaction, this->transactionBinaryArray.get())) {
    throw std::runtime_error("CachedTransaction::CachedTransaction(BinaryArray&&), deserealization error.");
  }
}

const Transaction& CachedTransaction::getTransaction() const {
  return transaction;
}

const Crypto::Hash& CachedTransaction::getTransactionHash() const {
  if (!transactionHash.is_initialized()) {
    transactionHash = getBinaryArrayHash(getTransactionBinaryArray());
  }

  return transactionHash.get();
}

const Crypto::Hash& CachedTransaction::getTransactionPrefixHash() const {
  if (!transactionPrefixHash.is_initialized()) {
    transactionPrefixHash = getObjectHash(static_cast<const TransactionPrefix&>(transaction));
  }

  return transactionPrefixHash.get();
}

const BinaryArray& CachedTransaction::getTransactionBinaryArray() const {
  if (!transactionBinaryArray.is_initialized()) {
    transactionBinaryArray = toBinaryArray(transaction);
  }

  return transactionBinaryArray.get();
}

uint64_t CachedTransaction::getTransactionFee() const {
  if (!transactionFee.is_initialized()) {
    uint64_t summaryInputAmount = 0;
    uint64_t summaryOutputAmount = 0;
    for (auto& out : transaction.outputs) {
      summaryOutputAmount += out.amount;
    }

    for (auto& in : transaction.inputs) {
      if (in.type() == typeid(KeyInput)) {
        summaryInputAmount += boost::get<KeyInput>(in).amount;
      } else if (in.type() == typeid(MultisignatureInput)) {
        summaryInputAmount += boost::get<MultisignatureInput>(in).amount;
      } else if (in.type() == typeid(BaseInput)) {
        return 0;
      } else {
        assert(false && "Unknown out type");
      }
    }

    transactionFee = summaryInputAmount - summaryOutputAmount;
  }

  return transactionFee.get();
}
