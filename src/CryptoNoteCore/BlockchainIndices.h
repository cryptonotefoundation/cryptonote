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

#pragma once

#include <boost/functional/hash.hpp>
#include <map>
#include <string>
#include <unordered_map>
#include <parallel_hashmap/phmap.h>

#include "crypto/hash.h"
#include "CryptoNoteBasic.h"

using phmap::flat_hash_map;

namespace CryptoNote {

class ISerializer;

inline size_t paymentIdHash(const Crypto::Hash& paymentId) {
  return boost::hash_range(std::begin(paymentId.data), std::end(paymentId.data));
}

class PaymentIdIndex {
public:
  PaymentIdIndex(bool enabled);

  bool add(const Transaction& transaction);
  bool remove(const Transaction& transaction);
  bool find(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes);
  std::vector<Crypto::Hash> find(const Crypto::Hash& paymentId);
  void clear();

  void serialize(ISerializer& s);

  template<class Archive> 
  void serialize(Archive& archive, unsigned int version) {
    archive & index;
  }
private:
  std::unordered_multimap<Crypto::Hash, Crypto::Hash, std::function<decltype(paymentIdHash)>> index;
  bool enabled = false;
};

class TimestampBlocksIndex {
public:
  TimestampBlocksIndex(bool enabled);

  bool add(uint64_t timestamp, const Crypto::Hash& hash);
  bool remove(uint64_t timestamp, const Crypto::Hash& hash);
  bool find(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t hashesNumberLimit, std::vector<Crypto::Hash>& hashes, uint32_t& hashesNumberWithinTimestamps);
  void clear();

  void serialize(ISerializer& s);

  template<class Archive> 
  void serialize(Archive& archive, unsigned int version) {
    archive & index;
  }
private:
  std::multimap<uint64_t, Crypto::Hash> index;
  bool enabled = false;
};

class TimestampTransactionsIndex {
public:
  TimestampTransactionsIndex(bool enabled);

  bool add(uint64_t timestamp, const Crypto::Hash& hash);
  bool remove(uint64_t timestamp, const Crypto::Hash& hash);
  bool find(uint64_t timestampBegin, uint64_t timestampEnd, uint64_t hashesNumberLimit, std::vector<Crypto::Hash>& hashes, uint64_t& hashesNumberWithinTimestamps);
  void clear();

  void serialize(ISerializer& s);

  template<class Archive>
  void serialize(Archive& archive, unsigned int version) {
    archive & index;
  }
private:
  std::multimap<uint64_t, Crypto::Hash> index;
  bool enabled = false;
};

class GeneratedTransactionsIndex {
public:
  GeneratedTransactionsIndex(bool enabled);

  bool add(const Block& block);
  bool remove(const Block& block);
  bool find(uint32_t height, uint64_t& generatedTransactions);
  void clear();

  void serialize(ISerializer& s);

  template<class Archive> 
  void serialize(Archive& archive, unsigned int version) {
    archive & index;
    archive & lastGeneratedTxNumber;
  }
private:
  flat_hash_map<uint32_t, uint64_t> index;

  uint64_t lastGeneratedTxNumber;
  bool enabled = false;
};

class OrphanBlocksIndex {
public:
  OrphanBlocksIndex(bool enabled);

  bool add(const Block& block);
  bool remove(const Block& block);
  bool find(uint32_t height, std::vector<Crypto::Hash>& blockHashes);
  void clear();
private:
  std::unordered_multimap<uint32_t, Crypto::Hash> index;
  bool enabled = false;
};

}
