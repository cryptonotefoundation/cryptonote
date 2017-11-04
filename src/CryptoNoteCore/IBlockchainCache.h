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

#include <vector>

#include <CryptoNote.h>

#include "CryptoNoteCore/CachedBlock.h"
#include "CryptoNoteCore/CachedTransaction.h"
#include "CryptoNoteCore/Difficulty.h"
#include "CryptoNoteCore/TransactionValidatiorState.h"
#include "Common/ArrayView.h"
#include <CryptoNoteCore/Difficulty.h>

namespace CryptoNote {

class ISerializer;
struct TransactionValidatorState;

enum class ExtractOutputKeysResult {
  SUCCESS,
  INVALID_GLOBAL_INDEX,
  OUTPUT_LOCKED
};

union PackedOutIndex {
  struct {
    uint32_t blockIndex;
    uint16_t transactionIndex;
    uint16_t outputIndex;
  };

  uint64_t packedValue;
};

const uint32_t INVALID_BLOCK_INDEX = std::numeric_limits<uint32_t>::max();

struct PushedBlockInfo {
  RawBlock rawBlock;
  TransactionValidatorState validatorState;
  size_t blockSize;
  uint64_t generatedCoins;
  Difficulty blockDifficulty;
};

class UseGenesis {
public:
  explicit UseGenesis(bool u) : use(u) {}
  // emulate boolean flag
  operator bool() {
    return use;
  }

private:
  bool use = false;
};
  
struct CachedBlockInfo;
struct CachedTransactionInfo;
class ITransactionPool;

class IBlockchainCache {
public:
  using BlockIndex = uint32_t;
  using GlobalOutputIndex = uint32_t;
  using Amount = uint64_t;

  virtual ~IBlockchainCache() {}

  virtual RawBlock getBlockByIndex(uint32_t index) const = 0;
  virtual BinaryArray getRawTransaction(uint32_t blockIndex, uint32_t transactionIndex) const = 0;
  virtual std::unique_ptr<IBlockchainCache> split(uint32_t splitBlockIndex) = 0;
  virtual void pushBlock(
      const CachedBlock& cachedBlock,
      const std::vector<CachedTransaction>& cachedTransactions,
      const TransactionValidatorState& validatorState,
      size_t blockSize,
      uint64_t generatedCoins,
      Difficulty blockDifficulty,
      RawBlock&& rawBlock) = 0;
  virtual PushedBlockInfo getPushedBlockInfo(uint32_t index) const = 0;
  virtual bool checkIfSpent(const Crypto::KeyImage& keyImage, uint32_t blockIndex) const = 0;
  virtual bool checkIfSpent(const Crypto::KeyImage& keyImage) const = 0;

  virtual bool checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex) const = 0;
  virtual bool checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) const = 0;

  virtual bool isTransactionSpendTimeUnlocked(uint64_t unlockTime) const = 0;
  virtual bool isTransactionSpendTimeUnlocked(uint64_t unlockTime, uint32_t blockIndex) const = 0;

  virtual ExtractOutputKeysResult extractKeyOutputKeys(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes, std::vector<Crypto::PublicKey>& publicKeys) const = 0;
  virtual ExtractOutputKeysResult extractKeyOutputKeys(uint64_t amount, uint32_t blockIndex, Common::ArrayView<uint32_t> globalIndexes, std::vector<Crypto::PublicKey>& publicKeys) const = 0;

  virtual ExtractOutputKeysResult extractKeyOtputIndexes(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes, std::vector<PackedOutIndex>& outIndexes) const = 0;
  virtual ExtractOutputKeysResult extractKeyOtputReferences(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes, std::vector<std::pair<Crypto::Hash, size_t>>& outputReferences) const = 0;
  //TODO: get rid of pred in this method. return vector of KeyOutputInfo structures
  virtual ExtractOutputKeysResult extractKeyOutputs(
      uint64_t amount, uint32_t blockIndex, Common::ArrayView<uint32_t> globalIndexes,
      std::function<ExtractOutputKeysResult(const CachedTransactionInfo& info, PackedOutIndex index, uint32_t globalIndex)> pred) const = 0;

  virtual bool getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex, MultisignatureOutput& output, uint64_t& unlockTime) const = 0;
  virtual bool getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex, MultisignatureOutput& output, uint64_t& unlockTime) const = 0;

  virtual std::pair<Crypto::Hash, size_t> getMultisignatureOutputReference(uint64_t amount, uint32_t globalIndex) const = 0;

  virtual uint32_t getTopBlockIndex() const = 0;
  virtual const Crypto::Hash& getTopBlockHash() const = 0;
  virtual uint32_t getBlockCount() const = 0;
  virtual bool hasBlock(const Crypto::Hash& blockHash) const = 0;
  virtual uint32_t getBlockIndex(const Crypto::Hash& blockHash) const = 0;

  virtual bool hasTransaction(const Crypto::Hash& transactionHash) const = 0;

  virtual std::vector<uint64_t> getLastTimestamps(size_t count) const = 0;
  virtual std::vector<uint64_t> getLastTimestamps(size_t count, uint32_t blockIndex, UseGenesis) const = 0;

  virtual std::vector<uint64_t> getLastBlocksSizes(size_t count) const = 0;
  virtual std::vector<uint64_t> getLastBlocksSizes(size_t count, uint32_t blockIndex, UseGenesis) const = 0;

  virtual std::vector<Difficulty> getLastCumulativeDifficulties(size_t count, uint32_t blockIndex, UseGenesis) const = 0;
  virtual std::vector<Difficulty> getLastCumulativeDifficulties(size_t count) const = 0;

  virtual Difficulty getDifficultyForNextBlock() const = 0;
  virtual Difficulty getDifficultyForNextBlock(uint32_t blockIndex) const = 0;

  virtual Difficulty getCurrentCumulativeDifficulty() const = 0;
  virtual Difficulty getCurrentCumulativeDifficulty(uint32_t blockIndex) const = 0;

  virtual uint64_t getAlreadyGeneratedCoins() const = 0;
  virtual uint64_t getAlreadyGeneratedCoins(uint32_t blockIndex) const = 0;

  virtual uint64_t getAlreadyGeneratedTransactions(uint32_t blockIndex) const = 0;

  virtual Crypto::Hash getBlockHash(uint32_t blockIndex) const = 0;
  virtual std::vector<Crypto::Hash> getBlockHashes(uint32_t startIndex, size_t maxCount) const = 0;

  virtual IBlockchainCache* getParent() const = 0;
  virtual void setParent(IBlockchainCache* parent) = 0;
  virtual uint32_t getStartBlockIndex() const = 0;

  virtual size_t getKeyOutputsCountForAmount(uint64_t amount, uint32_t blockIndex) const = 0;
  virtual size_t getMultisignatureCountForAmount(uint64_t amount, uint32_t blockIndex) const = 0;

  virtual uint32_t getTimestampLowerBoundBlockIndex(uint64_t timestamp) const = 0;
  virtual void addSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) = 0;

  //NOTE: shouldn't be recursive otherwise we'll get quadratic complexity
  virtual void getRawTransactions(const std::vector<Crypto::Hash>& transactions,
    std::vector<BinaryArray>& foundTransactions, std::vector<Crypto::Hash>& missedTransactions) const = 0;
  virtual std::vector<BinaryArray> getRawTransactions(const std::vector<Crypto::Hash> &transactions,
                                  std::vector<Crypto::Hash> &missedTransactions) const = 0;
  virtual std::vector<BinaryArray> getRawTransactions(const std::vector<Crypto::Hash> &transactions) const = 0;

  //NOTE: not recursive!
  virtual bool getTransactionGlobalIndexes(const Crypto::Hash& transactionHash, std::vector<uint32_t>& globalIndexes) const = 0;

  virtual size_t getTransactionCount() const = 0;

  virtual uint32_t getBlockIndexContainingTx(const Crypto::Hash& transactionHash) const = 0;

  virtual size_t getChildCount() const = 0;
  virtual void addChild(IBlockchainCache*) = 0;
  virtual bool deleteChild(IBlockchainCache*) = 0;

  virtual void save() = 0;
  virtual void load() = 0;

  virtual std::vector<uint64_t> getLastUnits(size_t count, uint32_t blockIndex, UseGenesis use,
                                             std::function<uint64_t(const CachedBlockInfo&)> pred) const = 0;
  virtual std::vector<Crypto::Hash> getTransactionHashes() const = 0;
  virtual std::vector<uint32_t> getRandomOutsByAmount(uint64_t amount, size_t count, uint32_t blockIndex) const = 0;

  virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) const = 0;
  virtual std::vector<Crypto::Hash> getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const = 0;
};

}
