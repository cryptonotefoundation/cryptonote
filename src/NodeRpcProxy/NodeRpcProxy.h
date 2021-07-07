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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "../CryptoNoteConfig.h"
#include "Common/ObserverManager.h"
#include "INode.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

namespace System {
  class ContextGroup;
  class Dispatcher;
  class Event;
}

namespace CryptoNote {

class HttpClient;

class INodeRpcProxyObserver {
public:
  virtual ~INodeRpcProxyObserver() {}
  virtual void connectionStatusUpdated(bool connected) {}
};

class NodeRpcProxy : public CryptoNote::INode {
public:
  NodeRpcProxy(const std::string& nodeHost, unsigned short nodePort, const std::string &daemon_path, const bool &daemon_ssl);
  virtual ~NodeRpcProxy();

  virtual bool addObserver(CryptoNote::INodeObserver* observer) override;
  virtual bool removeObserver(CryptoNote::INodeObserver* observer) override;

  virtual bool addObserver(CryptoNote::INodeRpcProxyObserver* observer);
  virtual bool removeObserver(CryptoNote::INodeRpcProxyObserver* observer);

  virtual void init(const Callback& callback) override;
  virtual bool shutdown() override;

  virtual size_t getPeerCount() const override;
  virtual uint32_t getLastLocalBlockHeight() const override;
  virtual uint32_t getLastKnownBlockHeight() const override;
  virtual uint32_t getLocalBlockCount() const override;
  virtual uint32_t getKnownBlockCount() const override;
  virtual uint64_t getLastLocalBlockTimestamp() const override;
  virtual uint64_t getMinimalFee() const override;
  virtual uint64_t getNextDifficulty() const override;
  virtual uint64_t getNextReward() const override;
  virtual uint64_t getAlreadyGeneratedCoins() const override;
  virtual uint32_t getNodeHeight() const override;
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

  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) override;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override;
  virtual void getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback) override;
  virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override;
  virtual void queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const Callback& callback) override;
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

  virtual std::string feeAddress() const override;
  virtual uint64_t feeAmount() const override;

  unsigned int rpcTimeout() const { return m_rpcTimeout; }
  void rpcTimeout(unsigned int val) { m_rpcTimeout = val; }

  const std::string m_daemon_path;
  const std::string m_nodeHost;
  const unsigned short m_nodePort;
  const bool m_daemon_ssl;

  virtual void setRootCert(const std::string &path) override;
  virtual void disableVerify() override;

private:
  void resetInternalState();
  void workerThread(const Callback& initialized_callback);

  std::vector<Crypto::Hash> getKnownTxsVector() const;
  void pullNodeStatusAndScheduleTheNext();
  void updateNodeStatus();
  void updateBlockchainStatus();
  bool updatePoolStatus();
  void updatePeerCount(size_t peerCount);
  void updatePoolState(const std::vector<std::unique_ptr<ITransactionReader>>& addedTxs, const std::vector<Crypto::Hash>& deletedTxsIds);
  void getFeeAddress();

  std::error_code doRelayTransaction(const CryptoNote::Transaction& transaction);
  std::error_code doGetRandomOutsByAmounts(std::vector<uint64_t>& amounts, uint64_t outsCount,
                                           std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result);
  std::error_code doGetNewBlocks(std::vector<Crypto::Hash>& knownBlockIds,
    std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight);
  std::error_code doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                    std::vector<uint32_t>& outsGlobalIndices);
  std::error_code doQueryBlocksLite(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp,
    std::vector<CryptoNote::BlockShortEntry>& newBlocks, uint32_t& startHeight);
  std::error_code doGetPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
          std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds);
  std::error_code doGetBlocksByHeight(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks);
  std::error_code doGetBlocksByHash(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks);
  std::error_code doGetBlock(const uint32_t blockHeight, BlockDetails& block);
  std::error_code doGetTransactionHashesByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes);
  std::error_code doGetTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction);
  std::error_code doGetTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions);
  std::error_code doGetBlockTimestamp(uint32_t height, uint64_t& timestamp);
  std::error_code doGetConnections(std::vector<p2pConnection>& connections);

  void scheduleRequest(std::function<std::error_code()>&& procedure, const Callback& callback);
  template <typename Request, typename Response>
  std::error_code binaryCommand(const std::string& comm, const Request& req, Response& res);
  template <typename Request, typename Response>
  std::error_code jsonCommand(const std::string& comm, const Request& req, Response& res);
  template <typename Request, typename Response>
  std::error_code jsonRpcCommand(const std::string& method, const Request& req, Response& res);

  enum State {
    STATE_NOT_INITIALIZED,
    STATE_INITIALIZING,
    STATE_INITIALIZED
  };

private:
  State m_state = STATE_NOT_INITIALIZED;
  mutable std::mutex m_mutex;
  std::condition_variable m_cv_initialized;
  std::thread m_workerThread;
  System::Dispatcher* m_dispatcher = nullptr;
  System::ContextGroup* m_context_group = nullptr;
  Tools::ObserverManager<CryptoNote::INodeObserver> m_observerManager;
  Tools::ObserverManager<CryptoNote::INodeRpcProxyObserver> m_rpcProxyObserverManager;

  unsigned int m_rpcTimeout;
  HttpClient* m_httpClient = nullptr;
  System::Event* m_httpEvent = nullptr;

  uint64_t m_pullInterval;

  // Internal state
  bool m_stop = false;
  std::atomic<size_t> m_peerCount;
  std::atomic<uint32_t> m_networkHeight;
  std::atomic<uint32_t> m_nodeHeight;
  std::atomic<uint64_t> m_minimalFee;
  std::atomic<uint64_t> m_nextDifficulty;
  std::atomic<uint64_t> m_nextReward;
  std::atomic<uint64_t> m_alreadyGeneratedCoins;
  std::atomic<uint64_t> m_transactionsCount;
  std::atomic<uint64_t> m_transactionsPoolSize;
  std::atomic<uint64_t> m_altBlocksCount;
  std::atomic<uint64_t> m_outConnectionsCount;
  std::atomic<uint64_t> m_incConnectionsCount;
  std::atomic<uint64_t> m_rpcConnectionsCount;
  std::atomic<uint64_t> m_whitePeerlistSize;
  std::atomic<uint64_t> m_greyPeerlistSize;
  std::string m_nodeVersion = "";

  BlockHeaderInfo lastLocalBlockHeaderInfo;
  //protect it with mutex if decided to add worker threads
  std::unordered_set<Crypto::Hash> m_knownTxs;

  bool m_connected;
  std::string m_fee_address;
  uint64_t m_fee_amount = 0;
  std::string m_daemon_cert;
  bool m_daemon_no_verify;
};

}
