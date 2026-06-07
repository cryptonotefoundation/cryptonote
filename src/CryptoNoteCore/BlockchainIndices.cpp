// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
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

#include "BlockchainIndices.h"

#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "BlockchainExplorer/BlockchainExplorerDataBuilder.h"

namespace CryptoNote {

namespace {
  const size_t DEFAULT_BUCKET_COUNT = 5;
}

PaymentIdIndex::PaymentIdIndex(bool _enabled) : enabled(_enabled), index(DEFAULT_BUCKET_COUNT, paymentIdHash) {
}

bool PaymentIdIndex::add(const Transaction& transaction) {
  if (!enabled) {
    return false;
  }

  Crypto::Hash paymentId;
  Crypto::Hash transactionHash = getObjectHash(transaction);
  if (!BlockchainExplorerDataBuilder::getPaymentId(transaction, paymentId)) {
    return false;
  }

  //index.emplace(paymentId, transactionHash);
  index.insert(std::make_pair(paymentId, transactionHash));

  return true;
}

bool PaymentIdIndex::remove(const Transaction& transaction) {
  if (!enabled) {
    return false;
  }

  Crypto::Hash paymentId;
  Crypto::Hash transactionHash = getObjectHash(transaction);
  if (!BlockchainExplorerDataBuilder::getPaymentId(transaction, paymentId)) {
    return false;
  }

  auto range = index.equal_range(paymentId);
  for (auto iter = range.first; iter != range.second; ++iter){
    if (iter->second == transactionHash) {
      index.erase(iter);
      return true;
    }
  }

  return false;
}

bool PaymentIdIndex::find(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes) {
  if (!enabled) {
    throw std::runtime_error("Payment id index disabled.");
  }

  bool found = false;
  auto range = index.equal_range(paymentId);
  for (auto iter = range.first; iter != range.second; ++iter){
    found = true;
    transactionHashes.emplace_back(iter->second);
  }
  return found;
}

std::vector<Crypto::Hash> PaymentIdIndex::find(const Crypto::Hash& paymentId) {
  if (!enabled) {
    throw std::runtime_error("Payment id index disabled.");
  }
  std::vector<Crypto::Hash> transactionHashes;
  auto range = index.equal_range(paymentId);
  for (auto iter = range.first; iter != range.second; ++iter) {
    transactionHashes.emplace_back(iter->second);
  }
  return transactionHashes;
}

void PaymentIdIndex::clear() {
  if (enabled) {
    index.clear();
  }
}


void PaymentIdIndex::serialize(ISerializer& s) {
  if (!enabled) {
    throw std::runtime_error("Payment id index disabled.");
  }

  s(index, "index");
}


TimestampTransactionsIndex::TimestampTransactionsIndex(bool _enabled) : enabled(_enabled) {
}

bool TimestampTransactionsIndex::add(uint64_t timestamp, const Crypto::Hash& hash) {
  if (!enabled) {
    return false;
  }

  index.emplace(timestamp, hash);
  return true;
}

bool TimestampTransactionsIndex::remove(uint64_t timestamp, const Crypto::Hash& hash) {
  if (!enabled) {
    return false;
  }

  auto range = index.equal_range(timestamp);
  for (auto iter = range.first; iter != range.second; ++iter) {
    if (iter->second == hash) {
      index.erase(iter);
      return true;
    }
  }

  return false;
}

bool TimestampTransactionsIndex::find(uint64_t timestampBegin, uint64_t timestampEnd, uint64_t hashesNumberLimit, std::vector<Crypto::Hash>& hashes, uint64_t& hashesNumberWithinTimestamps) {
  if (!enabled) {
    throw std::runtime_error("Timestamp transactions index disabled.");
  }
  
  uint32_t hashesNumber = 0;
  if (timestampBegin > timestampEnd) {
    //std::swap(timestampBegin, timestampEnd);
    return false;
  }
  auto begin = index.lower_bound(timestampBegin);
  auto end = index.upper_bound(timestampEnd);

  hashesNumberWithinTimestamps = static_cast<uint32_t>(std::distance(begin, end));

  for (auto iter = begin; iter != end && hashesNumber < hashesNumberLimit; ++iter) {
    ++hashesNumber;
    hashes.emplace_back(iter->second);
  }
  return hashesNumber > 0;
}

void TimestampTransactionsIndex::clear() {
  if (enabled) {
    index.clear();
  }
}

void TimestampTransactionsIndex::serialize(ISerializer& s) {
  if (!enabled) {
    throw std::runtime_error("Timestamp transactions index disabled.");
  }

  s(index, "index");
}


}
