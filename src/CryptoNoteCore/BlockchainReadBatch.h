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

#include <boost/functional/hash.hpp>

#include "IReadBatch.h"
#include "CryptoNote.h"
#include "BlockchainCache.h"
#include "DatabaseCacheData.h"

namespace std {
template <> struct hash<std::pair<CryptoNote::IBlockchainCache::Amount, uint32_t>> {
  using argment_type = std::pair<CryptoNote::IBlockchainCache::Amount, uint32_t>;
  using result_type = size_t;

  result_type operator() (const argment_type& arg) const {
    size_t hashValue = boost::hash_value(arg.first);
    boost::hash_combine(hashValue, arg.second);
    return hashValue;
  }
};

template <> struct hash<std::pair<Crypto::Hash, uint32_t>> {
  using argment_type = std::pair<Crypto::Hash, uint32_t>;
  using result_type = size_t;

  result_type operator() (const argment_type& arg) const {
    size_t hashValue = std::hash<Crypto::Hash>{}(arg.first);
    boost::hash_combine(hashValue, arg.second);
    return hashValue;
  }
};
}

namespace CryptoNote {

using KeyOutputKeyResult = std::unordered_map<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>, KeyOutputInfo>;

struct BlockchainReadState {
  std::unordered_map<uint32_t, std::vector<Crypto::KeyImage>> spentKeyImagesByBlock;
  std::unordered_map<Crypto::KeyImage, uint32_t> blockIndexesBySpentKeyImages;
  std::unordered_map<Crypto::Hash, ExtendedTransactionInfo> cachedTransactions;
  std::unordered_map<uint32_t, std::vector<Crypto::Hash>> transactionHashesByBlocks;
  std::unordered_map<uint32_t, CachedBlockInfo> cachedBlocks;
  std::unordered_map<Crypto::Hash, uint32_t> blockIndexesByBlockHashes;
  std::unordered_map<IBlockchainCache::Amount, uint32_t> keyOutputGlobalIndexesCountForAmounts;
  std::unordered_map<std::pair<IBlockchainCache::Amount, uint32_t>, PackedOutIndex> keyOutputGlobalIndexesForAmounts;
  std::unordered_map<IBlockchainCache::Amount, uint32_t> multisignatureOutputGlobalIndexesCountForAmounts;
  std::unordered_map<std::pair<IBlockchainCache::Amount, uint32_t>, PackedOutIndex> multisignatureOutputGlobalIndexesForAmounts;
  std::unordered_map<uint32_t, std::vector<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>>> spentMultisignatureOutputGlobalIndexesByBlocks;
  std::unordered_map<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>, bool> multisignatureOutputsSpendingStatuses;
  std::unordered_map<uint32_t, RawBlock> rawBlocks;
  std::unordered_map<uint64_t, uint32_t> closestTimestampBlockIndex;
  std::unordered_map<uint32_t, IBlockchainCache::Amount> keyOutputAmounts;
  std::unordered_map<uint32_t, IBlockchainCache::Amount> multisignatureOutputAmounts;
  std::unordered_map<Crypto::Hash, uint32_t> transactionCountsByPaymentIds;
  std::unordered_map<std::pair<Crypto::Hash, uint32_t>, Crypto::Hash> transactionHashesByPaymentIds;
  std::unordered_map<uint64_t, std::vector<Crypto::Hash>> blockHashesByTimestamp;
  KeyOutputKeyResult keyOutputKeys;

  std::pair<uint32_t, bool> lastBlockIndex = { 0, false };
  std::pair<uint32_t, bool> keyOutputAmountsCount = { {}, false };
  std::pair<uint32_t, bool> multisignatureOutputAmountsCount = { {}, false };
  std::pair<uint64_t, bool> transactionsCount = { 0, false };

  BlockchainReadState() = default;
  BlockchainReadState(const BlockchainReadState&) = default;
  BlockchainReadState(BlockchainReadState&& state);

  size_t size() const;
};

class BlockchainReadResult {
public:
  BlockchainReadResult(BlockchainReadState state);
  ~BlockchainReadResult();

  BlockchainReadResult(BlockchainReadResult&& result);

  const std::unordered_map<uint32_t, std::vector<Crypto::KeyImage>>& getSpentKeyImagesByBlock() const;
  const std::unordered_map<Crypto::KeyImage, uint32_t>& getBlockIndexesBySpentKeyImages() const;
  const std::unordered_map<Crypto::Hash, ExtendedTransactionInfo>& getCachedTransactions() const;
  const std::unordered_map<uint32_t, std::vector<Crypto::Hash>>& getTransactionHashesByBlocks() const;
  const std::unordered_map<uint32_t, CachedBlockInfo>& getCachedBlocks() const;
  const std::unordered_map<Crypto::Hash, uint32_t>& getBlockIndexesByBlockHashes() const;
  const std::unordered_map<IBlockchainCache::Amount, uint32_t>& getKeyOutputGlobalIndexesCountForAmounts() const;
  const std::unordered_map<std::pair<IBlockchainCache::Amount, uint32_t>, PackedOutIndex>& getKeyOutputGlobalIndexesForAmounts() const;
  const std::unordered_map<IBlockchainCache::Amount, uint32_t>& getMultisignatureOutputGlobalIndexesCountForAmounts() const;
  const std::unordered_map<std::pair<IBlockchainCache::Amount, uint32_t>, PackedOutIndex>& getMultisignatureOutputGlobalIndexesForAmounts() const;
  const std::unordered_map<uint32_t, std::vector<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>>>& getSpentMultisignatureOutputGlobalIndexesByBlocks() const;
  const std::unordered_map<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>, bool>& getMultisignatureOutputsSpendingStatuses() const;
  const std::unordered_map<uint32_t, RawBlock>& getRawBlocks() const;
  const std::pair<uint32_t, bool>& getLastBlockIndex() const;
  const std::unordered_map<uint64_t, uint32_t>& getClosestTimestampBlockIndex() const;
  uint32_t getKeyOutputAmountsCount() const;
  uint32_t getMultisignatureOutputAmountsCount() const;
  const std::unordered_map<uint32_t, IBlockchainCache::Amount>& getKeyOutputAmounts() const;
  const std::unordered_map<uint32_t, IBlockchainCache::Amount>& getMultisignatureOutputAmounts() const;
  const std::unordered_map<Crypto::Hash, uint32_t>& getTransactionCountByPaymentIds() const;
  const std::unordered_map<std::pair<Crypto::Hash, uint32_t>, Crypto::Hash>& getTransactionHashesByPaymentIds() const;
  const std::unordered_map<uint64_t, std::vector<Crypto::Hash> >& getBlockHashesByTimestamp() const;
  const std::pair<uint64_t, bool>& getTransactionsCount() const;
  const KeyOutputKeyResult& getKeyOutputInfo() const;

private:
  BlockchainReadState state;
};

class BlockchainReadBatch : public IReadBatch {
public:
  BlockchainReadBatch();
  ~BlockchainReadBatch();

  BlockchainReadBatch& requestSpentKeyImagesByBlock(uint32_t blockIndex);
  BlockchainReadBatch& requestBlockIndexBySpentKeyImage(const Crypto::KeyImage& keyImage);
  BlockchainReadBatch& requestCachedTransaction(const Crypto::Hash& txHash);
  BlockchainReadBatch& requestTransactionHashesByBlock(uint32_t blockIndex);
  BlockchainReadBatch& requestCachedBlock(uint32_t blockIndex);
  BlockchainReadBatch& requestBlockIndexByBlockHash(const Crypto::Hash& blockHash);
  BlockchainReadBatch& requestKeyOutputGlobalIndexesCountForAmount(IBlockchainCache::Amount amount);
  BlockchainReadBatch& requestKeyOutputGlobalIndexForAmount(IBlockchainCache::Amount amount, uint32_t outputIndexWithinAmout);
  BlockchainReadBatch& requestMultisignatureOutputGlobalIndexesCountForAmount(IBlockchainCache::Amount amount);
  BlockchainReadBatch& requestMultisignatureOutputGlobalIndexForAmount(IBlockchainCache::Amount amount, uint32_t outputIndexWithinAmout);
  BlockchainReadBatch& requestSpentMultisignatureOutputGlobalIndexesByBlock(uint32_t blockIndex);
  BlockchainReadBatch& requestMultisignatureOutputSpendingStatus(IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex index);
  BlockchainReadBatch& requestRawBlock(uint32_t blockIndex);
  BlockchainReadBatch& requestLastBlockIndex();
  BlockchainReadBatch& requestClosestTimestampBlockIndex(uint64_t timestamp);
  BlockchainReadBatch& requestKeyOutputAmountsCount();
  BlockchainReadBatch& requestMultisignatureOutputAmountsCount();
  BlockchainReadBatch& requestKeyOutputAmount(uint32_t index);
  BlockchainReadBatch& requestMultisignatureOutputAmount(uint32_t index);
  BlockchainReadBatch& requestTransactionCountByPaymentId(const Crypto::Hash& paymentId);
  BlockchainReadBatch& requestTransactionHashByPaymentId(const Crypto::Hash& paymentId, uint32_t transactionIndexWithinPaymentId);
  BlockchainReadBatch& requestBlockHashesByTimestamp(uint64_t timestamp);
  BlockchainReadBatch& requestTransactionsCount();
  BlockchainReadBatch& requestKeyOutputInfo(IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex globalIndex);

  std::vector<std::string> getRawKeys() const override;
  void submitRawResult(const std::vector<std::string>& values, const std::vector<bool>& resultStates) override;

  BlockchainReadResult extractResult();

private:
  bool resultSubmitted = false;
  BlockchainReadState state;
};

}
