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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "Common/ObserverManager.h"
#include "INode.h"

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
  NodeRpcProxy(const std::string& nodeHost, unsigned short nodePort);
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

  virtual void getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount, std::vector<Crypto::Hash>& blockHashes, const Callback& callback) override;
  virtual void getTransactionHashesByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes, const Callback& callback) override;

  virtual BlockHeaderInfo getLastLocalBlockHeaderInfo() const override;

  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) override;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint16_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override;
  virtual void getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::RawBlock>& newBlocks, uint32_t& startHeight, const Callback& callback) override;
  virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override;
  virtual void queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const Callback& callback) override;
  virtual void getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
          std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) override;
  virtual void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback) override;
  virtual void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) override;
  virtual void getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) override;
  virtual void getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) override;
  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override;

  unsigned int rpcTimeout() const { return m_rpcTimeout; }
  void rpcTimeout(unsigned int val) { m_rpcTimeout = val; }

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

  std::error_code doGetBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount, std::vector<Crypto::Hash>& blockHashes);
  std::error_code doRelayTransaction(const CryptoNote::Transaction& transaction);
  std::error_code doGetRandomOutsByAmounts(std::vector<uint64_t>& amounts, uint16_t outsCount,
                                           std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result);
  std::error_code doGetNewBlocks(std::vector<Crypto::Hash>& knownBlockIds,
    std::vector<CryptoNote::RawBlock>& newBlocks, uint32_t& startHeight);
  std::error_code doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                    std::vector<uint32_t>& outsGlobalIndices);
  std::error_code doQueryBlocksLite(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp,
    std::vector<CryptoNote::BlockShortEntry>& newBlocks, uint32_t& startHeight);
  std::error_code doGetPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
          std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds);
  std::error_code doGetBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks);
  std::error_code doGetTransactionHashesByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes);
  std::error_code doGetTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions);

  void scheduleRequest(std::function<std::error_code()>&& procedure, const Callback& callback);
  template <typename Request, typename Response>
  std::error_code binaryCommand(const std::string& url, const Request& req, Response& res);
  template <typename Request, typename Response>
  std::error_code jsonCommand(const std::string& url, const Request& req, Response& res);
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

  const std::string m_nodeHost;
  const unsigned short m_nodePort;
  unsigned int m_rpcTimeout;
  HttpClient* m_httpClient = nullptr;
  System::Event* m_httpEvent = nullptr;

  uint64_t m_pullInterval;

  // Internal state
  bool m_stop = false;
  std::atomic<size_t> m_peerCount;
  std::atomic<uint32_t> m_networkHeight;

  BlockHeaderInfo lastLocalBlockHeaderInfo;
  //protect it with mutex if decided to add worker threads
  std::unordered_set<Crypto::Hash> m_knownTxs;

  bool m_connected;
};
}
