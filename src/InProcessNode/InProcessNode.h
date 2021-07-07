// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2020, The Karbo developers
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

#include "INode.h"
#include "ITransaction.h"
#include "CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"
#include "CryptoNoteProtocol/ICryptoNoteProtocolObserver.h"
#include "CryptoNoteCore/ICore.h"
#include "CryptoNoteCore/ICoreObserver.h"
#include "Common/ObserverManager.h"
#include "BlockchainExplorer/BlockchainExplorerDataBuilder.h"

#include <thread>
#include <boost/asio.hpp>

namespace CryptoNote {

class Core;

class InProcessNode : public INode, public CryptoNote::ICryptoNoteProtocolObserver, public CryptoNote::ICoreObserver {
public:
  InProcessNode(CryptoNote::ICore& core, CryptoNote::ICryptoNoteProtocolQuery& protocol);

  InProcessNode(const InProcessNode&) = delete;
  InProcessNode(InProcessNode&&) = delete;

  InProcessNode& operator=(const InProcessNode&) = delete;
  InProcessNode& operator=(InProcessNode&&) = delete;

  virtual ~InProcessNode();

  virtual void init(const Callback& callback) override;
  virtual bool shutdown() override;

  virtual bool addObserver(INodeObserver* observer) override;
  virtual bool removeObserver(INodeObserver* observer) override;

  virtual size_t getPeerCount() const override;
  virtual uint32_t getLastLocalBlockHeight() const override;
  virtual uint32_t getLastKnownBlockHeight() const override;
  virtual uint32_t getLocalBlockCount() const override;
  virtual uint32_t getKnownBlockCount() const override;
  virtual uint32_t getNodeHeight() const override;
  virtual uint64_t getLastLocalBlockTimestamp() const override;
  virtual uint64_t getMinimalFee() const override;
  virtual uint64_t getNextDifficulty() const override;
  virtual uint64_t getNextReward() const override;
  virtual uint64_t getAlreadyGeneratedCoins() const override;
  virtual BlockHeaderInfo getLastLocalBlockHeaderInfo() const override;
  virtual uint64_t getTransactionsCount() const override;
  virtual uint64_t getTransactionsPoolSize() const override;
  virtual uint64_t getAltBlocksCount() const override;
  virtual uint64_t getOutConnectionsCount() const override;
  virtual uint64_t getIncConnectionsCount() const override;
  virtual uint64_t getRpcConnectionsCount() const override;
  virtual uint64_t getWhitePeerlistSize() const override;
  virtual uint64_t getGreyPeerlistSize() const override;
  virtual std::string getNodeVersion() const override;
  virtual std::string feeAddress() const override { return std::string(); }
  virtual uint64_t feeAmount() const override { return 0; }

  virtual void getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback) override;
  virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
      std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override;
  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) override;
  virtual void queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks,
    uint32_t& startHeight, const Callback& callback) override;
  virtual void getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
          std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) override;
  virtual void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback) override;

  virtual void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) override;
  virtual void getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) override;
  virtual void getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) override;
  virtual void getBlock(const uint32_t blockHeight, BlockDetails &block, const Callback& callback) override;
  virtual void getTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction, const Callback& callback) override;
  virtual void getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) override;
  virtual void getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions, const Callback& callback) override;
  virtual void getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback) override;
  virtual void getBlockTimestamp(uint32_t height, uint64_t& timestamp, const Callback& callback) override;
  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override;
  virtual void getConnections(std::vector<p2pConnection>& connections, const Callback& callback) override;

  virtual void setRootCert(const std::string &path) override;
  virtual void disableVerify() override;

private:
  virtual void peerCountUpdated(size_t count) override;
  virtual void lastKnownBlockHeightUpdated(uint32_t height) override;
  virtual void blockchainSynchronized(uint32_t topHeight) override;
  virtual void blockchainUpdated() override;
  virtual void poolUpdated() override;

  void updateLastLocalBlockHeaderInfo();
  void resetLastLocalBlockHeaderInfo();
  void getNewBlocksAsync(std::vector<Crypto::Hash>& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback);
  std::error_code doGetNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight);

  void getTransactionOutsGlobalIndicesAsync(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback);
  std::error_code doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices);

  void getRandomOutsByAmountsAsync(std::vector<uint64_t>& amounts, uint64_t outsCount,
      std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  std::error_code doGetRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
      std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result);

  void relayTransactionAsync(const CryptoNote::Transaction& transaction, const Callback& callback);
  std::error_code doRelayTransaction(const CryptoNote::Transaction& transaction);

  void queryBlocksLiteAsync(std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight,
          const Callback& callback);
  std::error_code doQueryBlocksLite(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight);

  void getPoolSymmetricDifferenceAsync(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
          std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback);

  void getOutByMSigGIndexAsync(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback);

  void getBlocksAsync(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback);
  std::error_code doGetBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks);

  void getBlocksAsync(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback);
  std::error_code doGetBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks);

  void getBlocksAsync(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback);
  std::error_code doGetBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps);

  void getTransactionAsync(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction, const Callback& callback);
  std::error_code doGetTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction);

  void getTransactionsAsync(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback);
  std::error_code doGetTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions);

  void getPoolTransactionsAsync(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback);
  std::error_code doGetPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps);

  void getTransactionsByPaymentIdAsync(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions, const Callback& callback);
  std::error_code doGetTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions);

  void getBlockTimestampAsync(uint32_t height, uint64_t& timestamp, const Callback& callback);
  std::error_code doGetBlockTimestampAsync(uint32_t height, uint64_t& timestamp);

  void isSynchronizedAsync(bool& syncStatus, const Callback& callback);

  void getConnectionsAsync(std::vector<p2pConnection>& connections, const Callback& callback);
  std::error_code doGetConnections(std::vector<p2pConnection>& connections);

  void workerFunc();
  bool doShutdown();

  enum State {
    NOT_INITIALIZED,
    INITIALIZED
  };

  State state;
  CryptoNote::ICore& core;
  CryptoNote::ICryptoNoteProtocolQuery& protocol;
  Tools::ObserverManager<INodeObserver> observerManager;
  BlockHeaderInfo lastLocalBlockHeaderInfo;

  boost::asio::io_service ioService;
  std::unique_ptr<std::thread> workerThread;
  std::unique_ptr<boost::asio::io_service::work> work;

  BlockchainExplorerDataBuilder blockchainExplorerDataBuilder;

  mutable std::mutex mutex;
};

} //namespace CryptoNote
