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

#include "IWriteBatch.h"

#include "BlockchainCache.h"
#include "CryptoNote.h"
#include "DatabaseCacheData.h"

namespace CryptoNote {

class BlockchainWriteBatch : public IWriteBatch {
public:
  BlockchainWriteBatch();
  ~BlockchainWriteBatch();

  BlockchainWriteBatch& insertSpentKeyImages(uint32_t blockIndex, const std::unordered_set<Crypto::KeyImage>& spentKeyImages);
  BlockchainWriteBatch& insertCachedTransaction(const ExtendedTransactionInfo& transaction, uint64_t totalTxsCount);
  BlockchainWriteBatch& insertPaymentId(const Crypto::Hash& transactionHash, const Crypto::Hash paymentId, uint32_t totalTxsCountForPaymentId);
  BlockchainWriteBatch& insertCachedBlock(const CachedBlockInfo& block, uint32_t blockIndex, const std::vector<Crypto::Hash>& blockTxs);
  BlockchainWriteBatch& insertKeyOutputGlobalIndexes(IBlockchainCache::Amount amount, const std::vector<PackedOutIndex>& outputs, uint32_t totalOutputsCountForAmount);
  BlockchainWriteBatch& insertMultisignatureOutputGlobalIndexes(IBlockchainCache::Amount amount, const std::vector<PackedOutIndex>& outputs, uint32_t totalOutputsCountForAmount);
  BlockchainWriteBatch& insertSpentMultisignatureOutputGlobalIndexes(uint32_t spendingBlockIndex, const std::set<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>>& outputs);
  BlockchainWriteBatch& insertRawBlock(uint32_t blockIndex, const RawBlock& block);
  BlockchainWriteBatch& insertClosestTimestampBlockIndex(uint64_t timestamp, uint32_t blockIndex);
  BlockchainWriteBatch& insertKeyOutputAmounts(const std::set<IBlockchainCache::Amount>& amounts, uint32_t totalKeyOutputAmountsCount);
  BlockchainWriteBatch& insertMultisignatureOutputAmounts(const std::set<IBlockchainCache::Amount>& amounts, uint32_t totalMultisignatureOutputAmountsCount);
  BlockchainWriteBatch& insertTimestamp(uint64_t timestamp, const std::vector<Crypto::Hash>& blockHashes);
  BlockchainWriteBatch& insertKeyOutputInfo(IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex globalIndex, const KeyOutputInfo& outputInfo);

  BlockchainWriteBatch& removeSpentKeyImages(uint32_t blockIndex, const std::vector<Crypto::KeyImage>& spentKeyImages);
  BlockchainWriteBatch& removeCachedTransaction(const Crypto::Hash& transactionHash, uint64_t totalTxsCount);
  BlockchainWriteBatch& removePaymentId(const Crypto::Hash paymentId, uint32_t totalTxsCountForPaytmentId);
  BlockchainWriteBatch& removeCachedBlock(const Crypto::Hash& blockHash, uint32_t blockIndex);
  BlockchainWriteBatch& removeKeyOutputGlobalIndexes(IBlockchainCache::Amount amount, uint32_t outputsToRemoveCount, uint32_t totalOutputsCountForAmount);
  BlockchainWriteBatch& removeMultisignatureOutputGlobalIndexes(IBlockchainCache::Amount amount, uint32_t outputsToRemoveCount, uint32_t totalOutputsCountForAmount);
  BlockchainWriteBatch& removeSpentMultisignatureOutputGlobalIndexes(uint32_t spendingBlockIndex, const std::vector<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>>& outputs);
  BlockchainWriteBatch& removeRawBlock(uint32_t blockIndex);
  BlockchainWriteBatch& removeClosestTimestampBlockIndex(uint64_t timestamp);
  BlockchainWriteBatch& removeTimestamp(uint64_t timestamp);
  BlockchainWriteBatch& removeKeyOutputAmounts(uint32_t keyOutputAmountsToRemoveCount, uint32_t totalKeyOutputAmountsCount);
  BlockchainWriteBatch& removeMultisignatureOutputAmounts(uint32_t multisignatureOutputAmountsToRemoveCount, uint32_t totalMultisignatureOutputAmountsCount);
  BlockchainWriteBatch& removeKeyOutputInfo(IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex globalIndex);

  std::vector<std::pair<std::string, std::string>> extractRawDataToInsert() override;
  std::vector<std::string> extractRawKeysToRemove() override;
private:
  std::vector<std::pair<std::string, std::string>> rawDataToInsert;
  std::vector<std::string> rawKeysToRemove;
};

}
