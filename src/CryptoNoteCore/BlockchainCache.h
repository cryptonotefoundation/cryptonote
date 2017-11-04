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
#include <map>
#include <unordered_map>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>

#include "BlockchainStorage.h"
#include "Common/StringView.h"
#include "Currency.h"
#include "Difficulty.h"
#include "IBlockchainCache.h"

namespace CryptoNote {

class ISerializer;

struct SpentKeyImage {
  uint32_t blockIndex;
  Crypto::KeyImage keyImage;

  void serialize(ISerializer& s);
};

struct CachedTransactionInfo {
  uint32_t blockIndex;
  uint32_t transactionIndex;
  Crypto::Hash transactionHash;
  uint64_t unlockTime;
  std::vector<TransactionOutputTarget> outputs;
  //needed for getTransactionGlobalIndexes query
  std::vector<uint32_t> globalIndexes;

  void serialize(ISerializer& s);
};

struct CachedBlockInfo {
  Crypto::Hash blockHash;
  uint64_t timestamp;
  Difficulty cumulativeDifficulty;
  uint64_t alreadyGeneratedCoins;
  uint64_t alreadyGeneratedTransactions;
  uint32_t blockSize;

  void serialize(ISerializer& s);
};

struct OutputGlobalIndexesForAmount {
  uint32_t startIndex = 0;

  // 1. This container must be sorted by PackedOutIndex::blockIndex and PackedOutIndex::transactionIndex
  // 2. GlobalOutputIndex for particular output is calculated as following: startIndex + index in vector
  std::vector<PackedOutIndex> outputs;

  void serialize(ISerializer& s);
};

struct MultisignatureOutputState {
  PackedOutIndex output;
  void serialize(ISerializer& s);
};

struct MultisignatureIndexes {
  uint32_t startIndex = 0;

  // 1. This container must be sorted by PackedOutIndex::blockIndex and PackedOutIndex::transactionIndex
  // 2. GlobalOutputIndex for particular output is calculated as following: startIndex + index in vector
  std::vector<MultisignatureOutputState> outputs;

  void serialize(ISerializer& s);
};

struct PaymentIdTransactionHashPair {
  Crypto::Hash paymentId;
  Crypto::Hash transactionHash;

  void serialize(ISerializer& s);
};

bool serialize(PackedOutIndex& value, Common::StringView name, CryptoNote::ISerializer& serializer);

class DatabaseBlockchainCache;

class BlockchainCache : public IBlockchainCache {
public:
  BlockchainCache(const std::string& filename, const Currency& currency, Logging::ILogger& logger, IBlockchainCache* parent, uint32_t startIndex = 0);

  //Returns upper part of segment. [this] remains lower part.
  //All of indexes on blockIndex == splitBlockIndex belong to upper part
  std::unique_ptr<IBlockchainCache> split(uint32_t splitBlockIndex) override;
  virtual void pushBlock(const CachedBlock& cachedBlock,
    const std::vector<CachedTransaction>& cachedTransactions,
    const TransactionValidatorState& validatorState,
    size_t blockSize,
    uint64_t generatedCoins,
    Difficulty blockDifficulty,
    RawBlock&& rawBlock) override;

  virtual PushedBlockInfo getPushedBlockInfo(uint32_t index) const override;
  bool checkIfSpent(const Crypto::KeyImage& keyImage, uint32_t blockIndex) const override;
  bool checkIfSpent(const Crypto::KeyImage& keyImage) const override;

  bool checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex) const override;
  bool checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) const override;
  
  bool isTransactionSpendTimeUnlocked(uint64_t unlockTime) const override;
  bool isTransactionSpendTimeUnlocked(uint64_t unlockTime, uint32_t blockIndex) const override;

  ExtractOutputKeysResult extractKeyOutputKeys(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes, std::vector<Crypto::PublicKey>& publicKeys) const override;
  ExtractOutputKeysResult extractKeyOutputKeys(uint64_t amount, uint32_t blockIndex, Common::ArrayView<uint32_t> globalIndexes, std::vector<Crypto::PublicKey>& publicKeys) const override;

  ExtractOutputKeysResult extractKeyOtputIndexes(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes, std::vector<PackedOutIndex>& outIndexes) const override;
  ExtractOutputKeysResult extractKeyOtputReferences(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes, std::vector<std::pair<Crypto::Hash, size_t>>& outputReferences) const override;
  
  bool getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex, MultisignatureOutput& output, uint64_t& unlockTime) const override;
  bool getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex, MultisignatureOutput& output, uint64_t& unlockTime) const override;
  std::pair<Crypto::Hash, size_t> getMultisignatureOutputReference(uint64_t amount, uint32_t globalIndex) const override;

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

  virtual IBlockchainCache* getParent() const override;
  virtual void setParent(IBlockchainCache* p) override;
  virtual uint32_t getStartBlockIndex() const override;

  virtual size_t getKeyOutputsCountForAmount(uint64_t amount, uint32_t blockIndex) const override;
  virtual size_t getMultisignatureCountForAmount(uint64_t amount, uint32_t blockIndex) const override;

  virtual uint32_t getTimestampLowerBoundBlockIndex(uint64_t timestamp) const override;
  virtual bool getTransactionGlobalIndexes(const Crypto::Hash& transactionHash, std::vector<uint32_t>& globalIndexes) const override;
  virtual size_t getTransactionCount() const override;
  virtual void addSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) override;
  virtual uint32_t getBlockIndexContainingTx(const Crypto::Hash& transactionHash) const override;

  virtual size_t getChildCount() const override;
  virtual void addChild(IBlockchainCache* child) override;
  virtual bool deleteChild(IBlockchainCache*) override;

  virtual void save() override;
  virtual void load() override;

  virtual std::vector<BinaryArray> getRawTransactions(const std::vector<Crypto::Hash> &transactions,
    std::vector<Crypto::Hash> &missedTransactions) const override;
  virtual std::vector<BinaryArray> getRawTransactions(const std::vector<Crypto::Hash> &transactions) const override;
  void getRawTransactions(const std::vector<Crypto::Hash> &transactions,
    std::vector<BinaryArray> &foundTransactions,
    std::vector<Crypto::Hash> &missedTransactions) const override;
  virtual RawBlock getBlockByIndex(uint32_t index) const override;
  virtual BinaryArray getRawTransaction(uint32_t blockIndex, uint32_t transactionIndex) const override;
  virtual std::vector<Crypto::Hash> getTransactionHashes() const override;
  virtual std::vector<uint32_t> getRandomOutsByAmount(uint64_t amount, size_t count, uint32_t blockIndex) const override;
  virtual ExtractOutputKeysResult extractKeyOutputs(uint64_t amount, uint32_t blockIndex, Common::ArrayView<uint32_t> globalIndexes,
    std::function<ExtractOutputKeysResult(const CachedTransactionInfo& info, PackedOutIndex index,
    uint32_t globalIndex)> pred) const override;

  virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) const override;
  virtual std::vector<Crypto::Hash> getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const override;

private:

  struct BlockIndexTag {};
  struct BlockHashTag {};
  struct TransactionHashTag {};
  struct KeyImageTag {};
  struct TransactionInBlockTag {};
  struct PackedOutputTag {};
  struct TimestampTag {};
  struct PaymentIdTag {};

  typedef boost::multi_index_container<
    SpentKeyImage,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<BlockIndexTag>,
        BOOST_MULTI_INDEX_MEMBER(SpentKeyImage, uint32_t, blockIndex)
      >,
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<KeyImageTag>,
        BOOST_MULTI_INDEX_MEMBER(SpentKeyImage, Crypto::KeyImage, keyImage)
      >
    >
  > SpentKeyImagesContainer;

  typedef boost::multi_index_container<
    CachedTransactionInfo,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<TransactionInBlockTag>,
        boost::multi_index::composite_key<
          CachedTransactionInfo,
          BOOST_MULTI_INDEX_MEMBER(CachedTransactionInfo, uint32_t, blockIndex),
          BOOST_MULTI_INDEX_MEMBER(CachedTransactionInfo, uint32_t, transactionIndex)
        >
      >,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<BlockIndexTag>,
        BOOST_MULTI_INDEX_MEMBER(CachedTransactionInfo, uint32_t, blockIndex)
      >,
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<TransactionHashTag>,
        BOOST_MULTI_INDEX_MEMBER(CachedTransactionInfo, Crypto::Hash, transactionHash)
      >
    >
  > TransactionsCacheContainer;

  typedef boost::multi_index_container<
    CachedBlockInfo,
    boost::multi_index::indexed_by<
      //The index here is blockIndex - startIndex
      boost::multi_index::random_access<
        boost::multi_index::tag<BlockIndexTag>
      >,
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<BlockHashTag>,
        BOOST_MULTI_INDEX_MEMBER(CachedBlockInfo, Crypto::Hash, blockHash)
      >,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<TimestampTag>,
        BOOST_MULTI_INDEX_MEMBER(CachedBlockInfo, uint64_t, timestamp)
      >
    >
  > BlockInfoContainer;

  typedef boost::multi_index_container<
    PaymentIdTransactionHashPair,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<PaymentIdTag>,
        BOOST_MULTI_INDEX_MEMBER(PaymentIdTransactionHashPair, Crypto::Hash, paymentId)
      >,
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<TransactionHashTag>,
        BOOST_MULTI_INDEX_MEMBER(PaymentIdTransactionHashPair, Crypto::Hash, transactionHash)
      >
    >
  > PaymentIdContainer;

  typedef std::map<uint64_t, OutputGlobalIndexesForAmount> OutputsGlobalIndexesContainer;
  typedef std::map<uint64_t, MultisignatureIndexes> MultisignaturesContainer;
  typedef std::map<BlockIndex, std::vector<std::pair<Amount, GlobalOutputIndex>>> OutputSpentInBlock;
  typedef std::set<std::pair<Amount, GlobalOutputIndex>> SpentOutputsOnAmount;

  const uint32_t CURRENT_SERIALIZATION_VERSION = 1;
  std::string filename;
  const Currency& currency;
  Logging::LoggerRef logger;
  IBlockchainCache* parent;
  // index of first block stored in this cache
  uint32_t startIndex;

  TransactionsCacheContainer transactions;
  SpentKeyImagesContainer spentKeyImages;
  BlockInfoContainer blockInfos;
  OutputsGlobalIndexesContainer keyOutputsGlobalIndexes;
  MultisignaturesContainer multisignatureStorage;
  PaymentIdContainer paymentIds;
  OutputSpentInBlock spentMultisigOutputsByBlock;
  SpentOutputsOnAmount spentMultisigOutputs;
  std::unique_ptr<BlockchainStorage> storage;

  std::vector<IBlockchainCache*> children;
 
  void serialize(ISerializer& s);

  void addSpentKeyImage(const Crypto::KeyImage& keyImage, uint32_t blockIndex);
  void pushTransaction(const CachedTransaction& tx, uint32_t blockIndex, uint16_t transactionBlockIndex);

  void splitSpentKeyImages(BlockchainCache& newCache, uint32_t splitBlockIndex);
  void splitTransactions(BlockchainCache& newCache, uint32_t splitBlockIndex);
  void splitBlocks(BlockchainCache& newCache, uint32_t splitBlockIndex);
  void splitKeyOutputsGlobalIndexes(BlockchainCache& newCache, uint32_t splitBlockIndex);
  void splitMultiSignatureOutputsGlobalIndexes(BlockchainCache& newCache, uint32_t splitBlockIndex);
  void removePaymentId(const Crypto::Hash& transactionHash, BlockchainCache& newCache);

  uint32_t insertKeyOutputToGlobalIndex(uint64_t amount, PackedOutIndex output, uint32_t blockIndex);
  uint32_t insertMultisignatureToGlobalIndex(uint64_t amount, PackedOutIndex output, uint32_t blockIndex);

  enum class OutputSearchResult : uint8_t {
    FOUND,
    NOT_FOUND,
    INVALID_ARGUMENT
  };

  OutputSearchResult findPackedOutForMultisignatureInCurrentSegment(uint64_t amount, uint32_t globalIndex, PackedOutIndex& packedOut) const;
  TransactionValidatorState fillOutputsSpentByBlock(uint32_t blockIndex) const;

  void fixChildrenParent(IBlockchainCache* p);

  void doPushBlock(const CachedBlock& cachedBlock,
    const std::vector<CachedTransaction>& cachedTransactions,
    const TransactionValidatorState& validatorState,
    size_t blockSize,
    uint64_t generatedCoins,
    Difficulty blockDifficulty,
    RawBlock&& rawBlock);
};

}
