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

#include "NodeRpcProxy.h"
#include "NodeErrors.h"

#include <atomic>
#include <system_error>
#include <thread>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/EventLock.h>
#include <System/Timer.h>
#include <CryptoNoteCore/TransactionApi.h>
#include "Common/FormatTools.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"
#include "Rpc/JsonRpc.h"

#ifndef AUTO_VAL_INIT
#define AUTO_VAL_INIT(n) boost::value_initialized<decltype(n)>()
#endif

using namespace Crypto;
using namespace Common;
using namespace System;

namespace CryptoNote {

namespace {

std::error_code interpretResponseStatus(const std::string& status) {
  if (CORE_RPC_STATUS_BUSY == status) {
    return make_error_code(error::NODE_BUSY);
  } else if (CORE_RPC_STATUS_OK != status) {
    return make_error_code(error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

}

NodeRpcProxy::NodeRpcProxy(const std::string& nodeHost, unsigned short nodePort, const std::string &daemon_path, const bool &daemon_ssl) :
    m_rpcTimeout(10000),
    m_pullInterval(5000),
    m_nodeHost(nodeHost),
    m_nodePort(nodePort),
    m_daemon_path(daemon_path),
    m_connected(false),
    m_daemon_ssl(daemon_ssl),
    m_daemon_cert(""),
    m_daemon_no_verify(false),
    m_peerCount(0),
    m_networkHeight(0),
    m_nodeHeight(0),
    m_nextDifficulty(0),
    m_nextReward(0),
    m_minimalFee(CryptoNote::parameters::MAXIMUM_FEE),
    m_alreadyGeneratedCoins(0),
    m_transactionsCount(0),
    m_transactionsPoolSize(0),
    m_altBlocksCount(0),
    m_outConnectionsCount(0),
    m_incConnectionsCount(0),
    m_rpcConnectionsCount(0),
    m_whitePeerlistSize(0),
    m_greyPeerlistSize(0)
{
  resetInternalState();
}

NodeRpcProxy::~NodeRpcProxy() {
  try {
    shutdown();
  } catch (std::exception&) {
  }
}

void NodeRpcProxy::setRootCert(const std::string &path) {
  if (m_daemon_cert.empty()) m_daemon_cert = path;
}

void NodeRpcProxy::disableVerify() {
  if (!m_daemon_no_verify) m_daemon_no_verify = true;
}

void NodeRpcProxy::resetInternalState() {
  m_stop = false;
  m_peerCount.store(0, std::memory_order_relaxed);
  m_networkHeight.store(0, std::memory_order_relaxed);
  lastLocalBlockHeaderInfo.index = 0;
  lastLocalBlockHeaderInfo.majorVersion = 0;
  lastLocalBlockHeaderInfo.minorVersion = 0;
  lastLocalBlockHeaderInfo.timestamp = 0;
  lastLocalBlockHeaderInfo.hash = CryptoNote::NULL_HASH;
  lastLocalBlockHeaderInfo.prevHash = CryptoNote::NULL_HASH;
  lastLocalBlockHeaderInfo.nonce = 0;
  lastLocalBlockHeaderInfo.isAlternative = false;
  lastLocalBlockHeaderInfo.depth = 0;
  lastLocalBlockHeaderInfo.difficulty = 0;
  lastLocalBlockHeaderInfo.reward = 0;
  m_knownTxs.clear();
}

void NodeRpcProxy::init(const INode::Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != STATE_NOT_INITIALIZED) {
    callback(make_error_code(error::ALREADY_INITIALIZED));
    return;
  }

  m_state = STATE_INITIALIZING;
  resetInternalState();
  m_workerThread = std::thread([this, callback] {
    workerThread(callback);
  });
}

bool NodeRpcProxy::shutdown() {
  std::unique_lock<std::mutex> lock(m_mutex);

  if (m_state == STATE_NOT_INITIALIZED) {
    return true;
  } else if (m_state == STATE_INITIALIZING) {
    m_cv_initialized.wait(lock, [this] { return m_state != STATE_INITIALIZING; });
    if (m_state == STATE_NOT_INITIALIZED) {
      return true;
    }
  }

  assert(m_state == STATE_INITIALIZED);
  assert(m_dispatcher != nullptr);

  m_dispatcher->remoteSpawn([this]() {
    m_stop = true;
    // Run all spawned contexts
    m_dispatcher->yield();
  });

  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
  m_state = STATE_NOT_INITIALIZED;
  m_cv_initialized.notify_all();
  return true;
}

void NodeRpcProxy::workerThread(const INode::Callback& initialized_callback) {
  try {
    Dispatcher dispatcher;
    m_dispatcher = &dispatcher;
    ContextGroup contextGroup(dispatcher);
    m_context_group = &contextGroup;
    HttpClient httpClient(dispatcher, m_nodeHost, m_nodePort, m_daemon_ssl);
    m_httpClient = &httpClient;
    if (!m_daemon_cert.empty()) m_httpClient->setRootCert(m_daemon_cert);
    if (m_daemon_no_verify) m_httpClient->disableVerify();
    Event httpEvent(dispatcher);
    m_httpEvent = &httpEvent;
    m_httpEvent->set();

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      assert(m_state == STATE_INITIALIZING);
      m_state = STATE_INITIALIZED;
      m_cv_initialized.notify_all();
    }

    initialized_callback(std::error_code());

    contextGroup.spawn([this]() {
      Timer pullTimer(*m_dispatcher);
      while (!m_stop) {
        updateNodeStatus();
        if (!m_stop) {
          pullTimer.sleep(std::chrono::milliseconds(m_pullInterval));
        }
      }
    });

    contextGroup.wait();
    // Make sure all remote spawns are executed
    m_dispatcher->yield();
  } catch (std::exception&) {
  }

  m_dispatcher = nullptr;
  m_context_group = nullptr;
  m_httpClient = nullptr;
  m_httpEvent = nullptr;
  m_connected = false;
  m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
}

void NodeRpcProxy::updateNodeStatus() {
  bool updateBlockchain = true;
  while (updateBlockchain) {
    updateBlockchainStatus();
    updateBlockchain = !updatePoolStatus();
  }
  getFeeAddress(); // Get public node's fee info
}

bool NodeRpcProxy::updatePoolStatus() {
  std::vector<Crypto::Hash> knownTxs = getKnownTxsVector();
  Crypto::Hash tailBlock = lastLocalBlockHeaderInfo.hash;

  bool isBcActual = false;
  std::vector<std::unique_ptr<ITransactionReader>> addedTxs;
  std::vector<Crypto::Hash> deletedTxsIds;

  std::error_code ec = doGetPoolSymmetricDifference(std::move(knownTxs), tailBlock, isBcActual, addedTxs, deletedTxsIds);
  if (ec) {
    return true;
  }

  if (!isBcActual) {
    return false;
  }

  if (!addedTxs.empty() || !deletedTxsIds.empty()) {
    updatePoolState(addedTxs, deletedTxsIds);
    m_observerManager.notify(&INodeObserver::poolChanged);
  }

  return true;
}

void NodeRpcProxy::updateBlockchainStatus() {
  CryptoNote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::response rsp = AUTO_VAL_INIT(rsp);

  std::error_code ec = jsonRpcCommand("getlastblockheader", req, rsp);

  if (!ec) {
    Crypto::Hash blockHash;
    Crypto::Hash prevBlockHash;
    if (!parse_hash256(rsp.block_header.hash, blockHash) || !parse_hash256(rsp.block_header.prev_hash, prevBlockHash)) {
      return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    uint32_t blockIndex = rsp.block_header.height;
    if (blockHash != lastLocalBlockHeaderInfo.hash) {
      lastLocalBlockHeaderInfo.index = blockIndex;
      lastLocalBlockHeaderInfo.majorVersion = rsp.block_header.major_version;
      lastLocalBlockHeaderInfo.minorVersion = rsp.block_header.minor_version;
      lastLocalBlockHeaderInfo.timestamp = rsp.block_header.timestamp;
      lastLocalBlockHeaderInfo.hash = blockHash;
      lastLocalBlockHeaderInfo.prevHash = prevBlockHash;
      lastLocalBlockHeaderInfo.nonce = rsp.block_header.nonce;
      lastLocalBlockHeaderInfo.isAlternative = rsp.block_header.orphan_status;
      lastLocalBlockHeaderInfo.depth = rsp.block_header.depth;
      lastLocalBlockHeaderInfo.difficulty = rsp.block_header.difficulty;
      lastLocalBlockHeaderInfo.reward = rsp.block_header.reward;
      lock.unlock();
      m_observerManager.notify(&INodeObserver::localBlockchainUpdated, blockIndex);
    }
  }

  CryptoNote::COMMAND_RPC_GET_INFO::request getInfoReq = AUTO_VAL_INIT(getInfoReq);
  CryptoNote::COMMAND_RPC_GET_INFO::response getInfoResp = AUTO_VAL_INIT(getInfoResp);

  ec = jsonCommand("getinfo", getInfoReq, getInfoResp);
  if (!ec) {
    //a quirk to let wallets work with previous versions daemons.
    //Previous daemons didn't have the 'last_known_block_index' parameter in RPC so it may have zero value.
    std::unique_lock<std::mutex> lock(m_mutex);
    auto lastKnownBlockIndex = std::max(getInfoResp.last_known_block_index, lastLocalBlockHeaderInfo.index);
    lock.unlock();
    if (m_networkHeight.load(std::memory_order_relaxed) != lastKnownBlockIndex) {
      m_networkHeight.store(lastKnownBlockIndex, std::memory_order_relaxed);
      m_observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, m_networkHeight.load(std::memory_order_relaxed));
    }

    updatePeerCount(getInfoResp.incoming_connections_count + getInfoResp.outgoing_connections_count);

    m_minimalFee.store(getInfoResp.min_fee, std::memory_order_relaxed);
    m_nodeHeight.store(getInfoResp.height, std::memory_order_relaxed);
    m_nextDifficulty.store(getInfoResp.difficulty, std::memory_order_relaxed);
    m_nextReward.store(getInfoResp.next_reward, std::memory_order_relaxed);
    m_transactionsCount.store(getInfoResp.transactions_count, std::memory_order_relaxed);
    m_transactionsPoolSize.store(getInfoResp.transactions_pool_size, std::memory_order_relaxed);
    m_altBlocksCount.store(getInfoResp.alt_blocks_count, std::memory_order_relaxed);
    m_outConnectionsCount.store(getInfoResp.outgoing_connections_count, std::memory_order_relaxed);
    m_incConnectionsCount.store(getInfoResp.incoming_connections_count, std::memory_order_relaxed);
    m_rpcConnectionsCount.store(getInfoResp.rpc_connections_count, std::memory_order_relaxed);
    m_whitePeerlistSize.store(getInfoResp.white_peerlist_size, std::memory_order_relaxed);
    m_greyPeerlistSize.store(getInfoResp.grey_peerlist_size, std::memory_order_relaxed);
    m_nodeVersion = getInfoResp.version;
    uint64_t alreadyGenCoins;
    if (Common::Format::parseAmount(boost::lexical_cast<std::string>(getInfoResp.already_generated_coins), alreadyGenCoins)) {
      m_alreadyGeneratedCoins.store(alreadyGenCoins, std::memory_order_relaxed);
    }
  }

  if (m_connected != m_httpClient->isConnected()) {
    m_connected = m_httpClient->isConnected();
    m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
  }
}

void NodeRpcProxy::updatePeerCount(size_t peerCount) {
  if (peerCount != m_peerCount) {
    m_peerCount = peerCount;
    m_observerManager.notify(&INodeObserver::peerCountUpdated, m_peerCount.load(std::memory_order_relaxed));
  }
}

void NodeRpcProxy::updatePoolState(const std::vector<std::unique_ptr<ITransactionReader>>& addedTxs, const std::vector<Crypto::Hash>& deletedTxsIds) {
  for (const auto& hash : deletedTxsIds) {
    m_knownTxs.erase(hash);
  }

  for (const auto& tx : addedTxs) {
    Hash hash = tx->getTransactionHash();
    m_knownTxs.emplace(std::move(hash));
  }
}

void NodeRpcProxy::getFeeAddress() {
  CryptoNote::COMMAND_RPC_GET_FEE_ADDRESS::request ireq = AUTO_VAL_INIT(ireq);
  CryptoNote::COMMAND_RPC_GET_FEE_ADDRESS::response iresp = AUTO_VAL_INIT(iresp);

  std::error_code ec = jsonCommand("feeaddress", ireq, iresp);

  if (ec || iresp.status != CORE_RPC_STATUS_OK) {
    return;
  }
  m_fee_address = iresp.fee_address;
  m_fee_amount = iresp.fee_amount;

  return;
}

std::string NodeRpcProxy::feeAddress() const {
  return m_fee_address;
}

uint64_t NodeRpcProxy::feeAmount() const {
  return m_fee_amount;
}

std::vector<Crypto::Hash> NodeRpcProxy::getKnownTxsVector() const {
  return std::vector<Crypto::Hash>(m_knownTxs.begin(), m_knownTxs.end());
}

bool NodeRpcProxy::addObserver(INodeObserver* observer) {
  return m_observerManager.add(observer);
}

bool NodeRpcProxy::removeObserver(INodeObserver* observer) {
  return m_observerManager.remove(observer);
}

bool NodeRpcProxy::addObserver(CryptoNote::INodeRpcProxyObserver* observer) {
  return m_rpcProxyObserverManager.add(observer);
}

bool NodeRpcProxy::removeObserver(CryptoNote::INodeRpcProxyObserver* observer) {
  return m_rpcProxyObserverManager.remove(observer);
}

size_t NodeRpcProxy::getPeerCount() const {
  return m_peerCount.load(std::memory_order_relaxed);
}

uint32_t NodeRpcProxy::getLastLocalBlockHeight() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return lastLocalBlockHeaderInfo.index;
}

uint32_t NodeRpcProxy::getLastKnownBlockHeight() const {
  return m_networkHeight.load(std::memory_order_relaxed);
}

uint32_t NodeRpcProxy::getLocalBlockCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return lastLocalBlockHeaderInfo.index + 1;
}

uint32_t NodeRpcProxy::getKnownBlockCount() const {
  return m_networkHeight.load(std::memory_order_relaxed) + 1;
}

uint64_t NodeRpcProxy::getLastLocalBlockTimestamp() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return lastLocalBlockHeaderInfo.timestamp;
}

uint64_t NodeRpcProxy::getMinimalFee() const {
  return m_minimalFee.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getNextDifficulty() const {
  return m_nextDifficulty.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getNextReward() const {
  return m_nextReward.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getAlreadyGeneratedCoins() const {
  return m_alreadyGeneratedCoins.load(std::memory_order_relaxed);
}

BlockHeaderInfo NodeRpcProxy::getLastLocalBlockHeaderInfo() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return lastLocalBlockHeaderInfo;
}

uint32_t NodeRpcProxy::getNodeHeight() const {
  return m_nodeHeight.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getTransactionsCount() const {
  return m_transactionsCount.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getTransactionsPoolSize() const {
  return m_transactionsPoolSize.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getAltBlocksCount() const {
  return m_altBlocksCount.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getOutConnectionsCount() const {
  return m_outConnectionsCount.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getIncConnectionsCount() const {
  return m_incConnectionsCount.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getRpcConnectionsCount() const {
  return m_rpcConnectionsCount.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getWhitePeerlistSize() const {
  return m_whitePeerlistSize.load(std::memory_order_relaxed);
}

uint64_t NodeRpcProxy::getGreyPeerlistSize() const {
  return m_greyPeerlistSize.load(std::memory_order_relaxed);
}

std::string NodeRpcProxy::getNodeVersion() const {
  return m_nodeVersion;
}

void NodeRpcProxy::relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doRelayTransaction, this, transaction), callback);
}

void NodeRpcProxy::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
                                          std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs,
                                          const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetRandomOutsByAmounts, this, std::move(amounts), outsCount, std::ref(outs)),
    callback);
}

void NodeRpcProxy::getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds,
                                std::vector<CryptoNote::block_complete_entry>& newBlocks,
                                uint32_t& startHeight,
                                const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetNewBlocks, this, std::move(knownBlockIds), std::ref(newBlocks),
    std::ref(startHeight)), callback);
}

void NodeRpcProxy::getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                   std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetTransactionOutsGlobalIndices, this, transactionHash,
    std::ref(outsGlobalIndices)), callback);
}

void NodeRpcProxy::queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks,
  uint32_t& startHeight, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doQueryBlocksLite, this, std::move(knownBlockIds), timestamp,
          std::ref(newBlocks), std::ref(startHeight)), callback);
}

void NodeRpcProxy::getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
        std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest([this, knownPoolTxIds, knownBlockId, &isBcActual, &newTxs, &deletedTxIds] () mutable -> std::error_code {
    return this->doGetPoolSymmetricDifference(std::move(knownPoolTxIds), knownBlockId, isBcActual, newTxs, deletedTxIds); } , callback);
}

void NodeRpcProxy::getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetBlocksByHeight, this, std::cref(blockHeights), std::ref(blocks)), callback);
}

void NodeRpcProxy::getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetBlocksByHash, this, std::cref(blockHashes), std::ref(blocks)), callback);
}

void NodeRpcProxy::getBlock(const uint32_t blockHeight, BlockDetails &block, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetBlock, this, blockHeight, std::ref(block)), callback);
}

void NodeRpcProxy::getBlockTimestamp(uint32_t height, uint64_t& timestamp, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetBlockTimestamp, this, height, std::ref(timestamp)), callback);
}

std::error_code NodeRpcProxy::doGetBlockTimestamp(uint32_t height, uint64_t& timestamp) {
  COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT::response rsp = AUTO_VAL_INIT(rsp);
  req.height = height;
  std::error_code ec = jsonRpcCommand("getblocktimestamp", req, rsp);

  timestamp = rsp.timestamp;
  return ec;
}

void NodeRpcProxy::getConnections(std::vector<p2pConnection>& connections, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetConnections, this, std::ref(connections)), callback);
}

std::error_code NodeRpcProxy::doGetConnections(std::vector<p2pConnection>& connections) {
  COMMAND_RPC_GET_CONNECTIONS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_CONNECTIONS::response rsp = AUTO_VAL_INIT(rsp);

  std::error_code ec = jsonCommand("getconnections", req, rsp);

  if (ec || rsp.status != CORE_RPC_STATUS_OK) {
    return ec;
  }

  for (const auto& p : rsp.connections) {
    p2pConnection c;

    c.version = p.version;
    c.connection_state = get_protocol_state_from_string(p.state);
    c.connection_id = boost::lexical_cast<boost::uuids::uuid>(p.connection_id);
    c.remote_ip = Common::stringToIpAddress(p.remote_ip);
    c.remote_port = p.remote_port;
    c.is_incoming = p.is_incoming;
    c.started = p.started;
    c.remote_blockchain_height = p.remote_blockchain_height;
    c.last_response_height = p.last_response_height;

    connections.push_back(c);
  }

  return ec;
}

void NodeRpcProxy::getTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetTransaction, this, std::cref(transactionHash), std::ref(transaction)), callback);
}


void NodeRpcProxy::getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetTransactions, this, std::cref(transactionHashes), std::ref(transactions)), callback);
}

void NodeRpcProxy::getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::isSynchronized(bool& syncStatus, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

std::error_code NodeRpcProxy::doRelayTransaction(const CryptoNote::Transaction& transaction) {
  COMMAND_RPC_SEND_RAW_TRANSACTION::request req;
  COMMAND_RPC_SEND_RAW_TRANSACTION::response rsp;
  req.tx_as_hex = toHex(toBinaryArray(transaction));
  return jsonCommand("sendrawtransaction", req, rsp);
}

std::error_code NodeRpcProxy::doGetRandomOutsByAmounts(std::vector<uint64_t>& amounts, uint64_t outsCount,
                                                       std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs) {
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response rsp = AUTO_VAL_INIT(rsp);
  req.amounts = std::move(amounts);
  req.outs_count = outsCount;

  std::error_code ec = binaryCommand("getrandom_outs.bin", req, rsp);
  if (!ec) {
    outs = std::move(rsp.outs);
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetNewBlocks(std::vector<Crypto::Hash>& knownBlockIds,
                                             std::vector<CryptoNote::block_complete_entry>& newBlocks,
                                             uint32_t& startHeight) {
  CryptoNote::COMMAND_RPC_GET_BLOCKS_FAST::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_BLOCKS_FAST::response rsp = AUTO_VAL_INIT(rsp);
  req.block_ids = std::move(knownBlockIds);

  std::error_code ec = binaryCommand("getblocks.bin", req, rsp);
  if (!ec) {
    newBlocks = std::move(rsp.blocks);
    startHeight = static_cast<uint32_t>(rsp.start_height);
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                                std::vector<uint32_t>& outsGlobalIndices) {
  CryptoNote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response rsp = AUTO_VAL_INIT(rsp);
  req.txid = transactionHash;

  std::error_code ec = binaryCommand("get_o_indexes.bin", req, rsp);
  if (!ec) {
    outsGlobalIndices.clear();
    for (auto idx : rsp.o_indexes) {
      outsGlobalIndices.push_back(static_cast<uint32_t>(idx));
    }
  }

  return ec;
}

std::error_code NodeRpcProxy::doQueryBlocksLite(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp,
        std::vector<CryptoNote::BlockShortEntry>& newBlocks, uint32_t& startHeight) {
  CryptoNote::COMMAND_RPC_QUERY_BLOCKS_LITE::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_QUERY_BLOCKS_LITE::response rsp = AUTO_VAL_INIT(rsp);

  req.blockIds = knownBlockIds;
  req.timestamp = timestamp;

  std::error_code ec = binaryCommand("queryblockslite.bin", req, rsp);
  if (ec) {
    return ec;
  }

  startHeight = static_cast<uint32_t>(rsp.startHeight);

  for (auto& item: rsp.items) {
    BlockShortEntry bse;
    bse.hasBlock = false;

    bse.blockHash = std::move(item.blockId);
    if (!item.block.empty()) {
      if (!fromBinaryArray(bse.block, asBinaryArray(item.block))) {
        return std::make_error_code(std::errc::invalid_argument);
      }

      bse.hasBlock = true;
    }

    for (const auto& txp: item.txPrefixes) {
      TransactionShortInfo tsi;
      tsi.txId = txp.txHash;
      tsi.txPrefix = txp.txPrefix;
      bse.txsShortInfo.push_back(std::move(tsi));
    }

    newBlocks.push_back(std::move(bse));
  }

  return std::error_code();
}

std::error_code NodeRpcProxy::doGetPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
        std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds) {
  CryptoNote::COMMAND_RPC_GET_POOL_CHANGES_LITE::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_POOL_CHANGES_LITE::response rsp = AUTO_VAL_INIT(rsp);

  req.tailBlockId = knownBlockId;
  req.knownTxsIds = knownPoolTxIds;

  std::error_code ec = binaryCommand("get_pool_changes_lite.bin", req, rsp);

  if (ec) {
    return ec;
  }

  isBcActual = rsp.isTailBlockActual;

  deletedTxIds = std::move(rsp.deletedTxsIds);

  for (const auto& tpi : rsp.addedTxs) {
    newTxs.push_back(createTransactionPrefix(tpi.txPrefix, tpi.txHash));
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetBlocksByHeight(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) {
  COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::response resp = AUTO_VAL_INIT(resp);

  req.blockHeights = blockHeights;

  std::error_code ec = jsonCommand("get_blocks_details_by_heights", req, resp);
  if (ec) {
    return ec;
  }

  auto tmp = std::move(resp.blocks);
  blocks.push_back(tmp);

  return ec;
}

std::error_code NodeRpcProxy::doGetBlocksByHash(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks) {
  COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::response resp = AUTO_VAL_INIT(resp);

  req.blockHashes = blockHashes;

  std::error_code ec = jsonCommand("get_blocks_details_by_hashes", req, resp);
  if (ec) {
    return ec;
  }

  blocks = std::move(resp.blocks);
  return ec;
}

std::error_code NodeRpcProxy::doGetBlock(const uint32_t blockHeight, BlockDetails& block) {
  COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response resp = AUTO_VAL_INIT(resp);

  req.blockHeight = blockHeight;

  std::error_code ec = jsonCommand("get_block_details_by_height", req, resp);

  if (ec) {
    return ec;
  }

  block = std::move(resp.block);

  return ec;
}

std::error_code NodeRpcProxy::doGetTransactionHashesByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes) {
  COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::response resp = AUTO_VAL_INIT(resp);

  req.paymentId = Common::podToHex(paymentId);
  std::error_code ec = jsonCommand("get_transaction_hashes_by_payment_id", req, resp);
  if (ec) {
    return ec;
  }

  transactionHashes = std::move(resp.transactionHashes);
  return ec;
}

std::error_code NodeRpcProxy::doGetTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction) {
  COMMAND_RPC_GET_TRANSACTIONS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_TRANSACTIONS::response  resp = AUTO_VAL_INIT(resp);

  req.txs_hashes.push_back(Common::podToHex(transactionHash));

  std::error_code ec = jsonCommand("gettransactions", req, resp);
  if (ec) {
    return ec;
  }

  if (resp.missed_txs.size() > 0) {
    return make_error_code(CryptoNote::error::REQUEST_ERROR);
  }

  BinaryArray tx_blob;
  if (!Common::fromHex(resp.txs_as_hex[0], tx_blob)) {
    return make_error_code(error::INTERNAL_NODE_ERROR);
  }

  Crypto::Hash tx_hash = NULL_HASH;
  Crypto::Hash tx_prefixt_hash = NULL_HASH;
  if (!parseAndValidateTransactionFromBinaryArray(tx_blob, transaction, tx_hash, tx_prefixt_hash) || tx_hash != transactionHash) {
    return make_error_code(error::INTERNAL_NODE_ERROR);
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions) {
  COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES::response resp = AUTO_VAL_INIT(resp);

  req.transactionHashes = transactionHashes;
  std::error_code ec = jsonCommand("get_transaction_details_by_hashes", req, resp);
  if (ec) {
    return ec;
  }

  transactions = std::move(resp.transactions);
  return ec;
}

void NodeRpcProxy::scheduleRequest(std::function<std::error_code()>&& procedure, const Callback& callback) {
  // callback is located on stack, so copy it inside binder
  class Wrapper {
  public:
    Wrapper(std::function<void(std::function<std::error_code()>&, Callback&)>&& _func,
            std::function<std::error_code()>&& _procedure, const Callback& _callback)
        : func(std::move(_func)), procedure(std::move(_procedure)), callback(std::move(_callback)) {
    }
    Wrapper(const Wrapper& other)
        : func(other.func), procedure(other.procedure), callback(other.callback) {
    }
    Wrapper(Wrapper&& other) // must be noexcept
        : func(std::move(other.func)), procedure(std::move(other.procedure)), callback(std::move(other.callback)) {
    }
    void operator()() {
      func(procedure, callback);
    }
  private:
    std::function<void(std::function<std::error_code()>&, Callback&)> func;
    std::function<std::error_code()> procedure;
    Callback callback;
  };
  assert(m_dispatcher != nullptr && m_context_group != nullptr);
  m_dispatcher->remoteSpawn(Wrapper([this](std::function<std::error_code()>& procedure, Callback& callback) {
    m_context_group->spawn(Wrapper([this](std::function<std::error_code()>& procedure, const Callback& callback) {
        if (m_stop) {
          callback(std::make_error_code(std::errc::operation_canceled));
        } else {
          std::error_code ec = procedure();
          if (m_connected != m_httpClient->isConnected()) {
            m_connected = m_httpClient->isConnected();
            m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
          }
          callback(m_stop ? std::make_error_code(std::errc::operation_canceled) : ec);
        }
      }, std::move(procedure), std::move(callback)));
    }, std::move(procedure), callback));
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::binaryCommand(const std::string& comm, const Request& req, Response& res) {
  std::error_code ec;

  std::string rpc_url = this->m_daemon_path + comm;

  try {
    EventLock eventLock(*m_httpEvent);
    invokeBinaryCommand(*m_httpClient, rpc_url, req, res);
    ec = interpretResponseStatus(res.status);
  } catch (const ConnectException&) {
    ec = make_error_code(error::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(error::NETWORK_ERROR);
  }

  return ec;
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::jsonCommand(const std::string& comm, const Request& req, Response& res) {
  std::error_code ec;

  std::string rpc_url = this->m_daemon_path + comm;

  try {
    EventLock eventLock(*m_httpEvent);
    invokeJsonCommand(*m_httpClient, rpc_url, req, res);
    ec = interpretResponseStatus(res.status);
  } catch (const ConnectException&) {
    ec = make_error_code(error::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(error::NETWORK_ERROR);
  }

  return ec;
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::jsonRpcCommand(const std::string& method, const Request& req, Response& res) {
  std::error_code ec = make_error_code(error::INTERNAL_NODE_ERROR);

  try {
    EventLock eventLock(*m_httpEvent);

    JsonRpc::JsonRpcRequest jsReq;

    jsReq.setMethod(method);
    jsReq.setParams(req);

    HttpRequest httpReq;
    HttpResponse httpRes;

    std::string rpc_url = this->m_daemon_path + "json_rpc";

    httpReq.addHeader("Content-Type", "application/json");
    httpReq.setUrl(rpc_url);
    httpReq.setBody(jsReq.getBody());

    m_httpClient->request(httpReq, httpRes);

    JsonRpc::JsonRpcResponse jsRes;

    if (httpRes.getStatus() == HttpResponse::STATUS_200) {
      jsRes.parse(httpRes.getBody());
      if (jsRes.getResult(res)) {
        ec = interpretResponseStatus(res.status);
      }
    }
  } catch (const ConnectException&) {
    ec = make_error_code(error::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(error::NETWORK_ERROR);
  }

  return ec;
}

}
