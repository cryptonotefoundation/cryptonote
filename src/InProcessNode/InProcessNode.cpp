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

#include "InProcessNode.h"

#include <functional>
#include <boost/utility/value_init.hpp>
#include <CryptoNoteCore/TransactionApi.h>

#include "CryptoNoteConfig.h"
#include "Common/Math.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/IBlock.h"
#include "CryptoNoteCore/VerificationContext.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h"
#include "InProcessNodeErrors.h"
#include "Common/StringTools.h"
#include "version.h"

using namespace Crypto;
using namespace Common;

namespace CryptoNote {

namespace {
  uint64_t getBlockReward(const Block& block) {
    uint64_t reward = 0;
    for (const TransactionOutput& out : block.baseTransaction.outputs) {
      reward += out.amount;
    }
    return reward;
  }
}

InProcessNode::InProcessNode(CryptoNote::ICore& core, CryptoNote::ICryptoNoteProtocolQuery& protocol) :
    state(NOT_INITIALIZED),
    core(core),
    protocol(protocol),
    blockchainExplorerDataBuilder(core, protocol) {
  resetLastLocalBlockHeaderInfo();
}

InProcessNode::~InProcessNode() {
  doShutdown();
}

bool InProcessNode::addObserver(INodeObserver* observer) {
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return observerManager.add(observer);
}

bool InProcessNode::removeObserver(INodeObserver* observer) {
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return observerManager.remove(observer);
}

void InProcessNode::init(const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  std::error_code ec;

  if (state != NOT_INITIALIZED) {
    ec = make_error_code(CryptoNote::error::ALREADY_INITIALIZED);
  } else {
    protocol.addObserver(this);
    core.addObserver(this);

    work.reset(new boost::asio::io_service::work(ioService));
    workerThread.reset(new std::thread(&InProcessNode::workerFunc, this));
    updateLastLocalBlockHeaderInfo();

    state = INITIALIZED;
  }

  ioService.post(std::bind(callback, ec));
}

bool InProcessNode::shutdown() {
  return doShutdown();
}

bool InProcessNode::doShutdown() {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    return false;
  }

  protocol.removeObserver(this);
  core.removeObserver(this);
  resetLastLocalBlockHeaderInfo();
  state = NOT_INITIALIZED;

  work.reset();
  ioService.stop();
  workerThread->join();
  ioService.reset();
  return true;
}

void InProcessNode::workerFunc() {
  ioService.run();
}

void InProcessNode::getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks,
  uint32_t& startHeight, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::getNewBlocksAsync,
      this,
      std::move(knownBlockIds),
      std::ref(newBlocks),
      std::ref(startHeight),
      callback
    )
  );
}

void InProcessNode::getNewBlocksAsync(std::vector<Crypto::Hash>& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks,
  uint32_t& startHeight, const Callback& callback)
{
  std::error_code ec = doGetNewBlocks(std::move(knownBlockIds), newBlocks, startHeight);
  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    // TODO code duplication see RpcServer::on_get_blocks()
    if (knownBlockIds.empty()) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    if (knownBlockIds.back() != core.getBlockIdByHeight(0)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    uint32_t totalBlockCount;
    std::vector<Crypto::Hash> supplement = core.findBlockchainSupplement(knownBlockIds, CryptoNote::COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT, totalBlockCount, startHeight);

    for (const auto& blockId : supplement) {
      assert(core.have_block(blockId));
      auto completeBlock = core.getBlock(blockId);
      assert(completeBlock != nullptr);

      CryptoNote::block_complete_entry be;
      be.block = asString(toBinaryArray(completeBlock->getBlock()));

      be.txs.reserve(completeBlock->getTransactionCount());
      for (size_t i = 0; i < completeBlock->getTransactionCount(); ++i) {
        be.txs.push_back(asString(toBinaryArray(completeBlock->getTransaction(i))));
      }

      newBlocks.push_back(std::move(be));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices,
    const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::getTransactionOutsGlobalIndicesAsync,
      this,
      std::cref(transactionHash),
      std::ref(outsGlobalIndices),
      callback
    )
  );
}

void InProcessNode::getTransactionOutsGlobalIndicesAsync(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices,
    const Callback& callback)
{
  std::error_code ec = doGetTransactionOutsGlobalIndices(transactionHash, outsGlobalIndices);
  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    bool r = core.get_tx_outputs_gindexs(transactionHash, outsGlobalIndices);
    if(!r) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::getRandomOutsByAmountsAsync,
      this,
      std::move(amounts),
      outsCount,
      std::ref(result),
      callback
    )
  );
}

void InProcessNode::getRandomOutsByAmountsAsync(std::vector<uint64_t>& amounts, uint64_t outsCount,
  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  std::error_code ec = doGetRandomOutsByAmounts(std::move(amounts), outsCount, result);
  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response res;
    CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request req;
    req.amounts = amounts;
    req.outs_count = outsCount;

    if(!core.get_random_outs_for_amounts(req, res)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    result = std::move(res.outs);
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}


void InProcessNode::relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::relayTransactionAsync,
      this,
      transaction,
      callback
    )
  );
}

void InProcessNode::relayTransactionAsync(const CryptoNote::Transaction& transaction, const Callback& callback) {
  std::error_code ec = doRelayTransaction(transaction);
  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doRelayTransaction(const CryptoNote::Transaction& transaction) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    CryptoNote::BinaryArray transactionBinaryArray = toBinaryArray(transaction);
    CryptoNote::tx_verification_context tvc = boost::value_initialized<CryptoNote::tx_verification_context>();

    if (!core.handle_incoming_tx(transactionBinaryArray, tvc, false)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    if(tvc.m_verification_failed) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    if(!tvc.m_should_be_relayed) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    CryptoNote::NOTIFY_NEW_TRANSACTIONS::request r;
    r.txs.push_back(asString(transactionBinaryArray));
    r.stem = true;
    core.get_protocol()->relay_transactions(r);
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

size_t InProcessNode::getPeerCount() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  return protocol.getPeerCount();
}

uint32_t InProcessNode::getLocalBlockCount() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return lastLocalBlockHeaderInfo.index + 1;
}

uint32_t InProcessNode::getNodeHeight() const {
  return getLocalBlockCount();
}

uint32_t InProcessNode::getKnownBlockCount() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  return protocol.getObservedHeight();
}

uint32_t InProcessNode::getLastLocalBlockHeight() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return lastLocalBlockHeaderInfo.index;
}

uint32_t InProcessNode::getLastKnownBlockHeight() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  return protocol.getObservedHeight() - 1;
}

uint64_t InProcessNode::getLastLocalBlockTimestamp() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return lastLocalBlockHeaderInfo.timestamp;
}

uint64_t InProcessNode::getMinimalFee() const {
  std::unique_lock<std::mutex> lock(mutex);
  return core.getMinimalFee();
}

uint64_t InProcessNode::getNextDifficulty() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
  return core.getNextBlockDifficulty();
}

uint64_t InProcessNode::getNextReward() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
  std::vector<size_t> blocksSizes;
  if (!core.getBackwardBlocksSizes(core.getCurrentBlockchainHeight() - 1, blocksSizes, parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW)) {
    return false;
  }
  uint64_t sizeMedian = Common::medianValue(blocksSizes);
  uint64_t nextReward = 0;
  int64_t emissionChange = 0;
  if (!core.getBlockReward(core.getCurrentBlockMajorVersion(), sizeMedian, 0, core.getTotalGeneratedAmount(), 0, nextReward, emissionChange)) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR));
  }
  return nextReward;
}

uint64_t InProcessNode::getAlreadyGeneratedCoins() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
  return core.getTotalGeneratedAmount();
}

BlockHeaderInfo InProcessNode::getLastLocalBlockHeaderInfo() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return lastLocalBlockHeaderInfo;
}

uint64_t InProcessNode::getTransactionsCount() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return core.getBlockchainTotalTransactions() - core.getCurrentBlockchainHeight();
}

uint64_t InProcessNode::getTransactionsPoolSize() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return core.getPoolTransactionsCount();
}

uint64_t InProcessNode::getAltBlocksCount() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return core.getAlternativeBlocksCount();
}

uint64_t InProcessNode::getOutConnectionsCount() const {
  // TODO NOT IMPLEMENTED

  return 0;
}

uint64_t InProcessNode::getIncConnectionsCount() const {
  // TODO NOT IMPLEMENTED

  return 0;
}

uint64_t InProcessNode::getRpcConnectionsCount() const {
  // TODO NOT IMPLEMENTED

  return 0;
}

uint64_t InProcessNode::getWhitePeerlistSize() const {
  // TODO NOT IMPLEMENTED

  return 0;
}

uint64_t InProcessNode::getGreyPeerlistSize() const {
  // TODO NOT IMPLEMENTED

  return 0;
}

std::string InProcessNode::getNodeVersion() const {
  return PROJECT_VERSION_LONG;
}

void InProcessNode::peerCountUpdated(size_t count) {
  observerManager.notify(&INodeObserver::peerCountUpdated, count);
}

void InProcessNode::lastKnownBlockHeightUpdated(uint32_t height) {
  observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, height - 1);
}

void InProcessNode::blockchainUpdated() {
  std::unique_lock<std::mutex> lock(mutex);
  updateLastLocalBlockHeaderInfo();
  uint32_t blockIndex = lastLocalBlockHeaderInfo.index;
  lock.unlock();
  observerManager.notify(&INodeObserver::localBlockchainUpdated, blockIndex);
}

void InProcessNode::poolUpdated() {
  observerManager.notify(&INodeObserver::poolChanged);
}

void InProcessNode::updateLastLocalBlockHeaderInfo() {
  uint32_t height;
  Crypto::Hash hash;
  Block block;
  uint64_t difficulty;
  /*
  do {
    core.get_blockchain_top(height, hash);
  } while (!core.getBlockByHash(hash, block) || !core.getBlockDifficulty(height, difficulty));
  */
  try {
    core.get_blockchain_top(height, hash);
    core.getBlockByHash(hash, block);
    difficulty = core.getBlockDifficulty(height, difficulty);
  } catch (const std::exception&) {
    return;
  }

  lastLocalBlockHeaderInfo.index = height;
  lastLocalBlockHeaderInfo.majorVersion = block.majorVersion;
  lastLocalBlockHeaderInfo.minorVersion = block.minorVersion;
  lastLocalBlockHeaderInfo.timestamp  = block.timestamp;
  lastLocalBlockHeaderInfo.hash = hash;
  lastLocalBlockHeaderInfo.prevHash = block.previousBlockHash;
  lastLocalBlockHeaderInfo.nonce = block.nonce;
  lastLocalBlockHeaderInfo.isAlternative = false;
  lastLocalBlockHeaderInfo.depth = 0;
  lastLocalBlockHeaderInfo.difficulty = difficulty;
  lastLocalBlockHeaderInfo.reward = getBlockReward(block);
}

void InProcessNode::resetLastLocalBlockHeaderInfo() {
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
}

void InProcessNode::blockchainSynchronized(uint32_t topHeight) {
  observerManager.notify(&INodeObserver::blockchainSynchronized, topHeight);
}

void InProcessNode::queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks,
  uint32_t& startHeight, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
          std::bind(&InProcessNode::queryBlocksLiteAsync,
                  this,
                  std::move(knownBlockIds),
                  timestamp,
                  std::ref(newBlocks),
                  std::ref(startHeight),
                  callback
          )
  );
}

void InProcessNode::queryBlocksLiteAsync(std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight,
                         const Callback& callback) {
  std::error_code ec = doQueryBlocksLite(std::move(knownBlockIds), timestamp, newBlocks, startHeight);
  callback(ec);
}

std::error_code InProcessNode::doQueryBlocksLite(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight) {
  uint32_t currentHeight, fullOffset;
  std::vector<CryptoNote::BlockShortInfo> entries;

  if (!core.queryBlocksLite(knownBlockIds, timestamp, startHeight, currentHeight, fullOffset, entries)) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  for (const auto& entry: entries) {
    BlockShortEntry bse;
    bse.blockHash = entry.blockId;
    bse.hasBlock = false;

    if (!entry.block.empty()) {
      bse.hasBlock = true;
      if (!fromBinaryArray(bse.block, asBinaryArray(entry.block))) {
        return std::make_error_code(std::errc::invalid_argument);
      }
    }

    for (const auto& tsi: entry.txPrefixes) {
      TransactionShortInfo tpi;
      tpi.txId = tsi.txHash;
      tpi.txPrefix = tsi.txPrefix;

      bse.txsShortInfo.push_back(std::move(tpi));
    }

    newBlocks.push_back(std::move(bse));
  }

  return std::error_code();

}

void InProcessNode::getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
        std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post([this, knownPoolTxIds, knownBlockId, &isBcActual, &newTxs, &deletedTxIds, callback] () mutable {
    this->getPoolSymmetricDifferenceAsync(std::move(knownPoolTxIds), knownBlockId, isBcActual, newTxs, deletedTxIds, callback);
  });
}

void InProcessNode::getPoolSymmetricDifferenceAsync(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
        std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) {
  std::error_code ec = std::error_code();

  std::vector<TransactionPrefixInfo> added;
  isBcActual = core.getPoolChangesLite(knownBlockId, knownPoolTxIds, added, deletedTxIds);

  try {
    for (const auto& tx: added) {
      newTxs.push_back(createTransactionPrefix(tx.txPrefix, tx.txHash));
    }
  } catch (std::system_error& ex) {
    ec = ex.code();
  } catch (std::exception&) {
    ec = make_error_code(std::errc::invalid_argument);
  }

  callback(ec);
}

void InProcessNode::getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post([this, amount, gindex, &out, callback]() mutable {
    this->getOutByMSigGIndexAsync(amount, gindex, out, callback);
  });
}

void InProcessNode::getOutByMSigGIndexAsync(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback) {
  std::error_code ec = std::error_code();
  bool result = core.getOutByMSigGIndex(amount, gindex, out);
  if (!result) {
    ec = make_error_code(std::errc::invalid_argument);
    callback(ec);
    return;
  }

  callback(ec);
}

void InProcessNode::getBlockTimestamp(uint32_t height, uint64_t& timestamp, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post([this, height, &timestamp, callback]() mutable {
    this->getBlockTimestampAsync(height, timestamp, callback);
  });
}

void InProcessNode::getBlockTimestampAsync(uint32_t height, uint64_t& timestamp, const Callback& callback) {
  std::error_code ec = doGetBlockTimestampAsync(height, timestamp);
  callback(ec);
}

std::error_code InProcessNode::doGetBlockTimestampAsync(uint32_t height, uint64_t& timestamp) {
  if (!core.getBlockTimestamp(height, timestamp)) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      static_cast<
        void(InProcessNode::*)(
        const std::vector<uint32_t>&,
          std::vector<std::vector<BlockDetails>>&,
          const Callback&
        )
      >(&InProcessNode::getBlocksAsync),
      this,
      std::cref(blockHeights),
      std::ref(blocks),
      callback
    )
  );
}

void InProcessNode::getBlock(const uint32_t blockHeight, BlockDetails &block, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  std::vector<uint32_t> blockHeights;
  std::vector<std::vector<BlockDetails>> blocks;
  blockHeights.push_back(blockHeight);
 
  getBlocksAsync(blockHeights, blocks, callback);

  block = blocks[0][0];
}

void InProcessNode::getBlocksAsync(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  std::error_code ec = core.executeLocked(
    std::bind(
      static_cast<
        std::error_code(InProcessNode::*)(
        const std::vector<uint32_t>&,
          std::vector<std::vector<BlockDetails>>&
        )
      >(&InProcessNode::doGetBlocks),
      this,
      std::cref(blockHeights),
      std::ref(blocks)
    )
  );
  callback(ec);
}

std::error_code InProcessNode::doGetBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) {
  try {
    uint32_t topHeight = 0;
    Crypto::Hash topHash = boost::value_initialized<Crypto::Hash>();
    core.get_blockchain_top(topHeight, topHash);
    for (const uint32_t& height : blockHeights) {
      if (height > topHeight) {
        return make_error_code(CryptoNote::error::REQUEST_ERROR);
      }
      Crypto::Hash hash = core.getBlockIdByHeight(height);
      Block block;
      if (!core.getBlockByHash(hash, block)) {
        return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
      }
      BlockDetails blockDetails;
      if (!blockchainExplorerDataBuilder.fillBlockDetails(block, blockDetails, false)) {
        return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
      }
      std::vector<BlockDetails> blocksOnSameHeight;
      blocksOnSameHeight.push_back(std::move(blockDetails));

      //Getting orphans
      std::vector<Block> orphanBlocks;
      core.getOrphanBlocksByHeight(height, orphanBlocks);
      for (const Block& orphanBlock : orphanBlocks) {
        BlockDetails orphanBlockDetails;
        if (!blockchainExplorerDataBuilder.fillBlockDetails(orphanBlock, orphanBlockDetails, false)) {
          return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
        }
        blocksOnSameHeight.push_back(std::move(orphanBlockDetails));
      }
      blocks.push_back(std::move(blocksOnSameHeight));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      static_cast<
        void(InProcessNode::*)(
          const std::vector<Crypto::Hash>&,
          std::vector<BlockDetails>&,
          const Callback&
        )
      >(&InProcessNode::getBlocksAsync),
      this,
      std::cref(blockHashes),
      std::ref(blocks),
      callback
    )
  );
}

void InProcessNode::getBlocksAsync(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  std::error_code ec = core.executeLocked(
    std::bind(
      static_cast<
        std::error_code(InProcessNode::*)(
          const std::vector<Crypto::Hash>&,
          std::vector<BlockDetails>&
        )
      >(&InProcessNode::doGetBlocks),
      this,
      std::cref(blockHashes),
      std::ref(blocks)
    )
  );
  callback(ec);
}

std::error_code InProcessNode::doGetBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks) {
  try {
    for (const Crypto::Hash& hash : blockHashes) {
      Block block;
      if (!core.getBlockByHash(hash, block)) {
        return make_error_code(CryptoNote::error::REQUEST_ERROR);
      }
      BlockDetails blockDetails;
      if (!blockchainExplorerDataBuilder.fillBlockDetails(block, blockDetails, false)) {
        return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
      }
      blocks.push_back(std::move(blockDetails));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

void InProcessNode::getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      static_cast<
        void(InProcessNode::*)(
          uint64_t,
          uint64_t,
          uint32_t,
          std::vector<BlockDetails>&,
          uint32_t&,
          const Callback&
        )
      >(&InProcessNode::getBlocksAsync),
      this,
      timestampBegin,
      timestampEnd,
      blocksNumberLimit,
      std::ref(blocks),
      std::ref(blocksNumberWithinTimestamps),
      callback
    )
  );
}

void InProcessNode::getBlocksAsync(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) {
  std::error_code ec = core.executeLocked(
    std::bind(
      static_cast<
        std::error_code(InProcessNode::*)(
          uint64_t,
          uint64_t,
          uint32_t,
          std::vector<BlockDetails>&,
          uint32_t&
        )
      >(&InProcessNode::doGetBlocks),
      this,
      timestampBegin,
      timestampEnd,
      blocksNumberLimit,
      std::ref(blocks),
      std::ref(blocksNumberWithinTimestamps)
    )
  );

  callback(ec);
}

std::error_code InProcessNode::doGetBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps) {
  try {
    std::vector<Block> rawBlocks;
    if (!core.getBlocksByTimestamp(timestampBegin, timestampEnd, blocksNumberLimit, rawBlocks, blocksNumberWithinTimestamps)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
    for (const Block& rawBlock : rawBlocks) {
      BlockDetails block;
      if (!blockchainExplorerDataBuilder.fillBlockDetails(rawBlock, block, false)) {
        return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
      }
      blocks.push_back(std::move(block));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

void InProcessNode::getTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      static_cast<
      void(InProcessNode::*)(
        const Crypto::Hash&,
        CryptoNote::Transaction&,
        const Callback&
        )
      >(&InProcessNode::getTransactionAsync),
      this,
      std::cref(transactionHash),
      std::ref(transaction),
      callback
    )
  );
}

void InProcessNode::getTransactionAsync(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction, const Callback& callback) {
  std::error_code ec = core.executeLocked(
    std::bind(
      static_cast<
      std::error_code(InProcessNode::*)(
        const Crypto::Hash&,
        CryptoNote::Transaction&
        )
      >(&InProcessNode::doGetTransaction),
      this,
      std::cref(transactionHash),
      std::ref(transaction)
    )
  );
  callback(ec);
}

std::error_code InProcessNode::doGetTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction) {
  try {
    std::list<Transaction> txs;
    std::list<Crypto::Hash> missed_txs;
    std::vector<Crypto::Hash> transactionHashes;
    transactionHashes.push_back(transactionHash);
    core.getTransactions(transactionHashes, txs, missed_txs, true);
    if (missed_txs.size() > 0) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
    transaction = std::move(txs.front());
  }
  catch (std::system_error& e) {
    return e.code();
  }
  catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

void InProcessNode::getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      static_cast<
        void(InProcessNode::*)(
          const std::vector<Crypto::Hash>&,
          std::vector<TransactionDetails>&,
          const Callback&
        )
      >(&InProcessNode::getTransactionsAsync),
      this,
      std::cref(transactionHashes),
      std::ref(transactions),
      callback
    )
  );
}

void InProcessNode::getTransactionsAsync(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::error_code ec = core.executeLocked(
    std::bind(
      static_cast<
        std::error_code(InProcessNode::*)(
          const std::vector<Crypto::Hash>&,
          std::vector<TransactionDetails>&
        )
      >(&InProcessNode::doGetTransactions),
      this,
      std::cref(transactionHashes),
      std::ref(transactions)
    )
  );
  callback(ec);
}

std::error_code InProcessNode::doGetTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions) {
  try {
    std::list<Transaction> txs;
    std::list<Crypto::Hash> missed_txs;
    core.getTransactions(transactionHashes, txs, missed_txs, true);
    if (missed_txs.size() > 0) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
    for (const Transaction& tx : txs) {
      TransactionDetails transactionDetails;
      if (!blockchainExplorerDataBuilder.fillTransactionDetails(tx, transactionDetails)) {
        return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
      }
      transactions.push_back(std::move(transactionDetails));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

void InProcessNode::getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      &InProcessNode::getPoolTransactionsAsync,
      this,
      timestampBegin,
      timestampEnd,
      transactionsNumberLimit,
      std::ref(transactions),
      std::ref(transactionsNumberWithinTimestamps),
      callback
    )
  );
}

void InProcessNode::getPoolTransactionsAsync(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback) {
  std::error_code ec = core.executeLocked(
    std::bind(
      &InProcessNode::doGetPoolTransactions,
      this,
      timestampBegin,
      timestampEnd,
      transactionsNumberLimit,
      std::ref(transactions),
      std::ref(transactionsNumberWithinTimestamps)
    )
  );

  callback(ec);
}

std::error_code InProcessNode::doGetPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps) {
  try {
    std::vector<Transaction> rawTransactions;
    if (!core.getPoolTransactionsByTimestamp(timestampBegin, timestampEnd, transactionsNumberLimit, rawTransactions, transactionsNumberWithinTimestamps)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
    for (const Transaction& rawTransaction : rawTransactions) {
      TransactionDetails transactionDetails;
      if (!blockchainExplorerDataBuilder.fillTransactionDetails(rawTransaction, transactionDetails)) {
        return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
      }
      transactions.push_back(std::move(transactionDetails));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

void InProcessNode::getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      &InProcessNode::getTransactionsByPaymentIdAsync,
      this,
      std::cref(paymentId),
      std::ref(transactions),
      callback
    )
  );
}

void InProcessNode::getTransactionsByPaymentIdAsync(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::error_code ec = core.executeLocked(
    std::bind(
      &InProcessNode::doGetTransactionsByPaymentId,
      this,
      paymentId,
      std::ref(transactions)
    )
  );

  callback(ec);
}

std::error_code InProcessNode::doGetTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions) {
  try {
    std::vector<Transaction> rawTransactions;
    if (!core.getTransactionsByPaymentId(paymentId, rawTransactions)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
    for (const Transaction& rawTransaction : rawTransactions) {
      TransactionDetails transactionDetails;
      if (!blockchainExplorerDataBuilder.fillTransactionDetails(rawTransaction, transactionDetails)) {
        return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
      }
      transactions.push_back(std::move(transactionDetails));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

void InProcessNode::isSynchronized(bool& syncStatus, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      &InProcessNode::isSynchronizedAsync,
      this,
      std::ref(syncStatus),
      callback
    )
  );
}

void InProcessNode::setRootCert(const std::string &path) {
  return;
}

void InProcessNode::disableVerify() {
  return;
}

void InProcessNode::isSynchronizedAsync(bool& syncStatus, const Callback& callback) {
  syncStatus = protocol.isSynchronized();
  callback(std::error_code());
}

void InProcessNode::getConnections(std::vector<p2pConnection>& connections, const Callback& callback) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  ioService.post(
    std::bind(&InProcessNode::getConnectionsAsync,
      this,
      std::ref(connections),
      callback
    )
  );
}

void InProcessNode::getConnectionsAsync(std::vector<p2pConnection>& connections, const Callback& callback) {
  std::error_code ec = doGetConnections(connections);
  callback(ec);
}

std::error_code InProcessNode::doGetConnections(std::vector<p2pConnection>& connections) {
  std::vector<CryptoNoteConnectionContext> peers;
  if (!protocol.getConnections(peers)) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  for (const auto& p : peers) {
    p2pConnection c;

    c.version = p.version;
    c.connection_state = static_cast<p2pConnection::state>(p.m_state);
    c.connection_id = p.m_connection_id;
    c.remote_ip = p.m_remote_ip;
    c.remote_port = p.m_remote_port;
    c.is_incoming = p.m_is_income;
    c.started = static_cast<uint64_t>(p.m_started);
    c.remote_blockchain_height = p.m_remote_blockchain_height;
    c.last_response_height = p.m_last_response_height;

    connections.push_back(c);
  }

  return std::error_code();
}

} //namespace CryptoNote
