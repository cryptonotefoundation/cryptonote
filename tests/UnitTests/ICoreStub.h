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

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <boost/bimap.hpp>

#include "Common/ObserverManager.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/ICore.h"
#include "CryptoNoteCore/ICoreObserver.h"
#include "CryptoNoteCore/IntrusiveLinkedList.h"
#include "CryptoNoteCore/MessageQueue.h"
#include "CryptoNoteCore/BlockchainMessages.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

class ICoreStub: public CryptoNote::ICore {
public:
  ICoreStub();
  ICoreStub(const CryptoNote::BlockTemplate& genesisBlock);

  template <class T> using MessageQueue = CryptoNote::MessageQueue<T>;
  using BlockchainMessage = CryptoNote::BlockchainMessage;

  virtual bool addMessageQueue(MessageQueue<BlockchainMessage>&  messageQueue) override;
  virtual bool removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) override;
  virtual uint32_t getTopBlockIndex() const override;
  virtual Crypto::Hash getTopBlockHash() const override;
  virtual uint64_t getBlockTimestampByIndex(uint32_t blockIndex) const override;
  virtual CryptoNote::BlockTemplate getBlockByIndex(uint32_t index) const override;
  
  virtual CryptoNote::Difficulty getDifficultyForNextBlock() const override;
  virtual std::error_code addBlock(const CryptoNote::CachedBlock& cachedBlock, CryptoNote::RawBlock&& rawBlock) override;
  virtual std::error_code addBlock(CryptoNote::RawBlock&& rawBlock) override;
  virtual std::error_code submitBlock(CryptoNote::BinaryArray&& rawBlockTemplate) override;
  
  virtual std::vector<CryptoNote::RawBlock> getBlocks(uint32_t startIndex, uint32_t count) const override;
  virtual void getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<CryptoNote::RawBlock>& blocks, std::vector<Crypto::Hash>& missedHashes) const override;
  virtual bool getRandomOutputs(uint64_t amount, uint16_t count, std::vector<uint32_t>& globalIndexes, std::vector<Crypto::PublicKey>& publicKeys) const override;
  virtual bool addTransactionToPool(const CryptoNote::BinaryArray& transactionBinaryArray) override;
  virtual std::vector<Crypto::Hash> getPoolTransactionHashes() const override;
  virtual bool getBlockTemplate(CryptoNote::BlockTemplate& b, const CryptoNote::AccountPublicAddress& adr, const CryptoNote::BinaryArray& extraNonce, CryptoNote::Difficulty& difficulty, uint32_t& height) const override;

  virtual CryptoNote::CoreStatistics getCoreStatistics() const override;

  virtual void save() override;
  virtual void load() override;

  virtual std::vector<Crypto::Hash> findBlockchainSupplement(const std::vector<Crypto::Hash>& remoteBlockIds, size_t maxCount,
    uint32_t& totalBlockCount, uint32_t& startBlockIndex) const override;
  virtual bool getPoolChanges(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds, std::vector<CryptoNote::BinaryArray>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) const override;
  virtual bool getPoolChangesLite(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
          std::vector<CryptoNote::TransactionPrefixInfo>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) const override;
  virtual bool queryBlocks(const std::vector<Crypto::Hash>& block_ids, uint64_t timestamp,
    uint32_t& start_height, uint32_t& current_height, uint32_t& full_offset, std::vector<CryptoNote::BlockFullInfo>& entries) const override;
  virtual bool queryBlocksLite(const std::vector<Crypto::Hash>& block_ids, uint64_t timestamp,
    uint32_t& start_height, uint32_t& current_height, uint32_t& full_offset, std::vector<CryptoNote::BlockShortInfo>& entries) const override;

  virtual bool hasBlock(const Crypto::Hash& id) const override;
  std::vector<Crypto::Hash> buildSparseChain() const override;
  virtual bool getTransactionGlobalIndexes(const Crypto::Hash& transactionHash, std::vector<uint32_t>& globalIndexes) const override;

  virtual Crypto::Hash getBlockHashByIndex(uint32_t height) const override;
  virtual CryptoNote::BlockTemplate getBlockByHash(const Crypto::Hash &h) const override;
  virtual void getTransactions(const std::vector<Crypto::Hash>& txs_ids, std::vector<CryptoNote::BinaryArray>& txs, std::vector<Crypto::Hash>& missed_txs) const override;
  virtual CryptoNote::Difficulty getBlockDifficulty(uint32_t index) const override;


  bool addObserver(CryptoNote::ICoreObserver* observer);
  bool removeObserver(CryptoNote::ICoreObserver* observer);
  void set_blockchain_top(uint32_t height, const Crypto::Hash& top_id);
  void set_outputs_gindexs(const std::vector<uint32_t>& indexs, bool result);
  void set_random_outs(const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp, bool result);

  void addBlock(const CryptoNote::BlockTemplate& block);
  void addTransaction(const CryptoNote::Transaction& tx);

  void setPoolTxVerificationResult(bool result);
  void setPoolChangesResult(bool result);
  boost::optional<std::pair<CryptoNote::MultisignatureOutput, uint64_t>>
  getMultisignatureOutput(uint64_t amount, uint32_t globalIndex) const override { return {}; }

  virtual bool hasTransaction(const Crypto::Hash& transactionHash) const override;
  virtual CryptoNote::BlockDetails getBlockDetails(const Crypto::Hash& blockHash) const override;
  virtual CryptoNote::TransactionDetails getTransactionDetails(const Crypto::Hash& transactionHash) const override;
  virtual std::vector<Crypto::Hash> getAlternativeBlockHashesByIndex(uint32_t blockIndex) const override;
  virtual std::vector<Crypto::Hash> getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const override { return {};}
  virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) const override { return {}; }

private:
  uint32_t topHeight;
  Crypto::Hash topId;

  std::vector<uint32_t> globalIndices;
  bool globalIndicesResult;

  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response randomOuts;
  bool randomOutsResult;

  std::unordered_map<Crypto::Hash, CryptoNote::BlockTemplate> blocks;
  std::unordered_map<uint32_t, Crypto::Hash> blockHashByHeightIndex; //TODO: replace these two indexes with boost bimap
  std::unordered_map<Crypto::Hash, uint32_t> blockHeightByHashIndex;
  std::unordered_map<Crypto::Hash, Crypto::Hash> blockHashByTxHashIndex;

  std::unordered_map<Crypto::Hash, CryptoNote::BinaryArray> transactions;
  std::unordered_map<Crypto::Hash, CryptoNote::BinaryArray> transactionPool;
  bool poolTxVerificationResult;
  bool poolChangesResult;
  std::unordered_map<Crypto::Hash, Crypto::Hash> transactionBlockHashes;
  Tools::ObserverManager<CryptoNote::ICoreObserver> m_observerManager;

  CryptoNote::IntrusiveLinkedList<MessageQueue<BlockchainMessage>> queueList;
};
