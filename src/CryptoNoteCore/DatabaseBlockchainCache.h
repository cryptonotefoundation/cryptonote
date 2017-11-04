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

#include "Common/StringView.h"
#include "Currency.h"
#include "Difficulty.h"
#include "IBlockchainCache.h"
#include <IDataBase.h>
#include <CryptoNoteCore/BlockchainReadBatch.h>
#include <CryptoNoteCore/BlockchainWriteBatch.h>
#include <CryptoNoteCore/DatabaseCacheData.h>
#include <CryptoNoteCore/IBlockchainCacheFactory.h>

namespace CryptoNote {

/*
 * Implementation of IBlockchainCache that uses database to store internal indexes.
 * Current implementation is designed to always be the root of blockchain, ie
 * start index is always zero, parent is always nullptr, no methods
 * do recursive calls to parent.
 */
class DatabaseBlockchainCache : public IBlockchainCache {
public:
  using BlockIndex = uint32_t;
  using GlobalOutputIndex = uint32_t;
  using Amount = uint64_t;

  /*
   * Constructs new DatabaseBlockchainCache object. Currnetly, only factories that produce 
   * BlockchainCache objects as children are supported.
   */
  DatabaseBlockchainCache(const Currency& currency, IDataBase& dataBase,
                          IBlockchainCacheFactory& blockchainCacheFactory, Logging::ILogger& logger);

  /*
   * This methods splits cache, upper part (ie blocks with indexes larger than splitBlockIndex)
   * is copied to new BlockchainCache. Unfortunately, implementation requires return value to be of
   * BlockchainCache type.
   */
  std::unique_ptr<IBlockchainCache> split(uint32_t splitBlockIndex) override;
  void pushBlock(const CachedBlock& cachedBlock, const std::vector<CachedTransaction>& cachedTransactions,
                 const TransactionValidatorState& validatorState, size_t blockSize, uint64_t generatedCoins,
                 Difficulty blockDifficulty, RawBlock&& rawBlock) override;
  virtual PushedBlockInfo getPushedBlockInfo(uint32_t index) const override;
  bool checkIfSpent(const Crypto::KeyImage& keyImage, uint32_t blockIndex) const override;
  bool checkIfSpent(const Crypto::KeyImage& keyImage) const override;

  bool checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex) const override;
  bool checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) const override;

  bool isTransactionSpendTimeUnlocked(uint64_t unlockTime) const override;
  bool isTransactionSpendTimeUnlocked(uint64_t unlockTime, uint32_t blockIndex) const override;

  ExtractOutputKeysResult extractKeyOutputKeys(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes,
                                               std::vector<Crypto::PublicKey>& publicKeys) const override;
  ExtractOutputKeysResult extractKeyOutputKeys(uint64_t amount, uint32_t blockIndex,
                                               Common::ArrayView<uint32_t> globalIndexes,
                                               std::vector<Crypto::PublicKey>& publicKeys) const override;

  ExtractOutputKeysResult extractKeyOtputIndexes(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes,
                                                 std::vector<PackedOutIndex>& outIndexes) const override;
  ExtractOutputKeysResult
  extractKeyOtputReferences(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes,
                            std::vector<std::pair<Crypto::Hash, size_t>>& outputReferences) const override;

  bool getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex, MultisignatureOutput& output,
                                       uint64_t& unlockTime) const override;
  bool getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex,
                                       MultisignatureOutput& output, uint64_t& unlockTime) const override;
  std::pair<Crypto::Hash, size_t> getMultisignatureOutputReference(uint64_t amount,
                                                                   uint32_t globalIndex) const override;

  uint32_t getTopBlockIndex() const override;
  const Crypto::Hash& getTopBlockHash() const override;
  uint32_t getBlockCount() const override;
  bool hasBlock(const Crypto::Hash& blockHash) const override;
  uint32_t getBlockIndex(const Crypto::Hash& blockHash) const override;

  bool hasTransaction(const Crypto::Hash& transactionHash) const override;

  std::vector<uint64_t> getLastTimestamps(size_t count) const override;
  std::vector<uint64_t> getLastTimestamps(size_t count, uint32_t blockIndex, UseGenesis) const override;

  std::vector<uint64_t> getLastBlocksSizes(size_t count) const override;
  std::vector<uint64_t> getLastBlocksSizes(size_t count, uint32_t blockIndex, UseGenesis) const override;

  std::vector<Difficulty> getLastCumulativeDifficulties(size_t count, uint32_t blockIndex, UseGenesis) const override;
  std::vector<Difficulty> getLastCumulativeDifficulties(size_t count) const override;

  Difficulty getDifficultyForNextBlock() const override;
  Difficulty getDifficultyForNextBlock(uint32_t blockIndex) const override;

  virtual Difficulty getCurrentCumulativeDifficulty() const override;
  virtual Difficulty getCurrentCumulativeDifficulty(uint32_t blockIndex) const override;

  uint64_t getAlreadyGeneratedCoins() const override;
  uint64_t getAlreadyGeneratedCoins(uint32_t blockIndex) const override;
  uint64_t getAlreadyGeneratedTransactions(uint32_t blockIndex) const override;
  std::vector<uint64_t> getLastUnits(size_t count, uint32_t blockIndex, UseGenesis use,
                                     std::function<uint64_t(const CachedBlockInfo&)> pred) const override;

  Crypto::Hash getBlockHash(uint32_t blockIndex) const override;
  virtual std::vector<Crypto::Hash> getBlockHashes(uint32_t startIndex, size_t maxCount) const override;

  /*
   * This method always returns zero
   */
  virtual uint32_t getStartBlockIndex() const override;

  virtual size_t getKeyOutputsCountForAmount(uint64_t amount, uint32_t blockIndex) const override;
  virtual size_t getMultisignatureCountForAmount(uint64_t amount, uint32_t blockIndex) const override;

  virtual uint32_t getTimestampLowerBoundBlockIndex(uint64_t timestamp) const override;
  virtual bool getTransactionGlobalIndexes(const Crypto::Hash& transactionHash,
                                           std::vector<uint32_t>& globalIndexes) const override;
  virtual size_t getTransactionCount() const override;
  virtual void addSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) override;
  virtual uint32_t getBlockIndexContainingTx(const Crypto::Hash& transactionHash) const override;

  virtual size_t getChildCount() const override;

  /*
   * This method always returns nullptr
   */
  virtual IBlockchainCache* getParent() const override;
  /*
   * This method does nothing, is here only to support full interface
   */
  virtual void setParent(IBlockchainCache* ptr) override;
  virtual void addChild(IBlockchainCache* ptr) override;
  virtual bool deleteChild(IBlockchainCache* ptr) override;

  virtual void save() override;
  virtual void load() override;

  virtual std::vector<BinaryArray> getRawTransactions(const std::vector<Crypto::Hash>& transactions,
                                                      std::vector<Crypto::Hash>& missedTransactions) const override;
  virtual std::vector<BinaryArray> getRawTransactions(const std::vector<Crypto::Hash>& transactions) const override;
  void getRawTransactions(const std::vector<Crypto::Hash>& transactions, std::vector<BinaryArray>& foundTransactions,
                          std::vector<Crypto::Hash>& missedTransactions) const override;
  virtual RawBlock getBlockByIndex(uint32_t index) const override;
  virtual BinaryArray getRawTransaction(uint32_t blockIndex, uint32_t transactionIndex) const override;
  virtual std::vector<Crypto::Hash> getTransactionHashes() const override;
  virtual std::vector<uint32_t> getRandomOutsByAmount(uint64_t amount, size_t count,
                                                      uint32_t blockIndex) const override;
  virtual ExtractOutputKeysResult
  extractKeyOutputs(uint64_t amount, uint32_t blockIndex, Common::ArrayView<uint32_t> globalIndexes,
                    std::function<ExtractOutputKeysResult(const CachedTransactionInfo& info, PackedOutIndex index,
                                                          uint32_t globalIndex)> pred) const override;

  virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) const override;
  virtual std::vector<Crypto::Hash> getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const override;

private:
  const Currency& currency;
  IDataBase& database;
  IBlockchainCacheFactory& blockchainCacheFactory;
  mutable boost::optional<uint32_t> topBlockIndex;
  mutable boost::optional<Crypto::Hash> topBlockHash;
  mutable boost::optional<uint64_t> transactionsCount;
  mutable boost::optional<uint32_t> keyOutputAmountsCount;
  mutable boost::optional<uint32_t> multiOutputAmountsCount;
  mutable std::unordered_map<Amount, int32_t> keyOutputCountsForAmounts;
  mutable std::unordered_map<Amount, int32_t> multiOutputCountsForAmounts;
  std::vector<IBlockchainCache*> children;
  Logging::LoggerRef logger;
  std::deque<CachedBlockInfo> unitsCache;
  const size_t unitsCacheSize = 1000;

  struct ExtendedPushedBlockInfo;
  ExtendedPushedBlockInfo getExtendedPushedBlockInfo(uint32_t blockIndex) const;

  void deleteClosestTimestampBlockIndex(BlockchainWriteBatch& writeBatch, uint32_t splitBlockIndex);
  CachedBlockInfo getCachedBlockInfo(uint32_t index) const;
  BlockchainReadResult readDatabase(BlockchainReadBatch& batch) const;

  void addSpentKeyImage(const Crypto::KeyImage& keyImage, uint32_t blockIndex);
  void pushTransaction(const CachedTransaction& cachedTransaction,
                       uint32_t blockIndex,
                       uint16_t transactionBlockIndex,
                       BlockchainWriteBatch& batch);

  uint32_t insertKeyOutputToGlobalIndex(uint64_t amount, PackedOutIndex output); //TODO not implemented. Should it be removed?
  uint32_t insertMultisignatureToGlobalIndex(uint64_t amount, PackedOutIndex output);
  uint32_t updateKeyOutputCount(Amount amount, int32_t diff) const;
  uint32_t updateMultiOutputCount(Amount amount, int32_t diff) const;
  void insertPaymentId(BlockchainWriteBatch& batch, const Crypto::Hash& transactionHash, const Crypto::Hash& paymentId);
  void insertBlockTimestamp(BlockchainWriteBatch& batch, uint64_t timestamp, const Crypto::Hash& blockHash);

  void addGenesisBlock(CachedBlock&& genesisBlock);

  enum class OutputSearchResult : uint8_t { FOUND, NOT_FOUND, INVALID_ARGUMENT };

  OutputSearchResult findPackedOutForMultisignatureInCurrentSegment(uint64_t amount, uint32_t globalIndex,
                                                                    PackedOutIndex& packedOut) const;
  TransactionValidatorState fillOutputsSpentByBlock(uint32_t blockIndex) const;

  bool doGetMultisignatureOutputIfExists(
    uint64_t amount,
    uint32_t globalIndex,
    uint32_t blockIndex,
    std::function<void (const CachedTransactionInfo& transaction, PackedOutIndex packedOutput)> extractor) const;

  Crypto::Hash pushBlockToAnotherCache(IBlockchainCache& segment, PushedBlockInfo&& pushedBlockInfo);
  void requestDeleteSpentOutputs(BlockchainWriteBatch& writeBatch, uint32_t splitBlockIndex, const TransactionValidatorState& spentOutputs);
  std::vector<Crypto::Hash> requestTransactionHashesFromBlockIndex(uint32_t splitBlockIndex);
  void requestDeleteTransactions(BlockchainWriteBatch& writeBatch, const std::vector<Crypto::Hash>& transactionHashes);
  void requestDeletePaymentIds(BlockchainWriteBatch& writeBatch, const std::vector<Crypto::Hash>& transactionHashes);
  void requestDeletePaymentId(BlockchainWriteBatch& writeBatch, const Crypto::Hash& paymentId, size_t toDelete);
  void requestDeleteKeyOutputs(BlockchainWriteBatch& writeBatch, const std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>& boundaries);
  void requestDeleteKeyOutputsAmount(BlockchainWriteBatch& writeBatch, IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex boundary, uint32_t outputsCount);
  void requestDeleteMultisignatureOutputs(BlockchainWriteBatch& writeBatch, const std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>& boundaries);
  void requestDeleteMultisignatureOutputsAmount(BlockchainWriteBatch& writeBatch, IBlockchainCache::Amount amount,
                                                IBlockchainCache::GlobalOutputIndex boundary, uint32_t outputsCount);
  void requestRemoveTimestamp(BlockchainWriteBatch& batch, uint64_t timestamp, const Crypto::Hash& blockHash);

  uint64_t getCachedTransactionsCount() const;

  std::vector<CachedBlockInfo> getLastCachedUnits(uint32_t blockIndex, size_t count, UseGenesis useGenesis) const;
  std::vector<CachedBlockInfo> getLastDbUnits(uint32_t blockIndex, size_t count, UseGenesis useGenesis) const;
};
}
