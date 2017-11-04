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

#include "InProcessNode.h"

#include <functional>
#include <future>
#include <boost/utility/value_init.hpp>
#include <CryptoNoteCore/TransactionApi.h>

#include <System/RemoteContext.h>

#include "CryptoNoteConfig.h"
#include "Common/StringTools.h"
#include "Common/ScopeExit.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/VerificationContext.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h"
#include "InProcessNodeErrors.h"
#include "Common/StringTools.h"

using namespace Crypto;
using namespace Common;

namespace CryptoNote {

namespace {

//executes function in dispatcher's context from any thread
//add <void> specialisation when needed
template <class ReturnType>
class RemotelySpawnedSyncContext {
public:
  RemotelySpawnedSyncContext(System::Dispatcher& dispatcher, std::atomic<size_t>& counter, System::Event& counterEvent, std::function<ReturnType()>&& function) :
    called(false)
  {
    future = promise.get_future();

    counter++;
    dispatcher.remoteSpawn([this, function, &counter, &counterEvent] () {
      Tools::ScopeExit guard([&counter, &counterEvent] () {
        counter--;
        counterEvent.set();
      });

      try {
        promise.set_value(function());
      } catch (std::exception&) {
        promise.set_exception(std::current_exception());
      }
    });
  }

  ~RemotelySpawnedSyncContext() {
    try {
      if (!called) {
        future.get();
      }
    } catch (std::exception&){
    }
  }

  ReturnType get() {
    called = true;
    return future.get();
  }

  RemotelySpawnedSyncContext(const RemotelySpawnedSyncContext&) = delete;
  RemotelySpawnedSyncContext& operator=(const RemotelySpawnedSyncContext&) = delete;

  RemotelySpawnedSyncContext(RemotelySpawnedSyncContext&&) = delete;
  RemotelySpawnedSyncContext& operator=(RemotelySpawnedSyncContext&&) = delete;

private:
  std::promise<ReturnType> promise;
  std::future<ReturnType> future;
  std::atomic<bool> called;
};

class RemoteContextCounterWrapper {
public:
  RemoteContextCounterWrapper(System::Dispatcher& dispatcher_, std::function<void()>&& function_, std::atomic<size_t>& contextCounter_, System::Event& contextCounterEvent_):
    dispatcher(dispatcher_),
    function(std::move(function_)),
    contextCounter(contextCounter_),
    contextCounterEvent(contextCounterEvent_)
  {
  }

  void operator()() {
    contextCounter++;
    Tools::ScopeExit guard([this] () {
      contextCounter--;
      contextCounterEvent.set();
    });

    System::RemoteContext<void> remoteContext(dispatcher, [this] {
      function();
    });

    remoteContext.get();
  }

private:
  System::Dispatcher& dispatcher;
  std::function<void()> function;
  std::atomic<size_t>& contextCounter;
  System::Event& contextCounterEvent;
};

void remoteSpawn(System::Dispatcher& dispatcher, std::function<void()>&& func, std::atomic<size_t>& contextCounter, System::Event& contextCounterEvent) {
  contextCounter++;

  dispatcher.remoteSpawn([func, &contextCounter, &contextCounterEvent] () {
    Tools::ScopeExit guard([&contextCounter, &contextCounterEvent] () {
      contextCounter--;
      contextCounterEvent.set();
    });

    func();
  });
}

uint64_t getBlockReward(const BlockTemplate& block) {
  uint64_t reward = 0;
  for (const TransactionOutput& out : block.baseTransaction.outputs) {
    reward += out.amount;
  }
  return reward;
}

}

InProcessNode::InProcessNode(CryptoNote::ICore& core, CryptoNote::ICryptoNoteProtocolHandler& protocol,
                             System::Dispatcher& disp)
    : state(NOT_INITIALIZED), contextGroup(dispatcher), contextCounter(0), contextCounterEvent(disp), core(core), protocol(protocol),
      messageQueue(dispatcher), dispatcher(disp) {
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
    executeInRemoteThread([callback, ec] { callback(ec); });
    return;
  }

  protocol.addObserver(this);
  core.addMessageQueue(messageQueue);

  contextCounter++;
  contextGroup.spawn([this] {
    using namespace Messages;

    Tools::ScopeExit guard([this] () {
      contextCounter--;
      contextCounterEvent.set();
    });

    try {
      while (true) {
        messageQueue.front().match(
          [this](const NewBlock& msg) {
            auto topBlockIndex = this->core.getTopBlockIndex();
            executeInRemoteThread([this, topBlockIndex] () { blockchainUpdated(topBlockIndex); });
          },
          [this](const NewAlternativeBlock& msg) {
            auto topBlockIndex = this->core.getTopBlockIndex();
            executeInRemoteThread([this, topBlockIndex] () { blockchainUpdated(topBlockIndex); });
          },
          [this](const ChainSwitch& msg) {
            auto topBlockIndex = this->core.getTopBlockIndex();
            executeInRemoteThread([this, msg, topBlockIndex] () {
              chainSwitched(topBlockIndex, msg.commonRootIndex, msg.blocksFromCommonRoot);
              blockchainUpdated(topBlockIndex);
            });
          },
          [this](const AddTransaction& msg) {
            executeInRemoteThread([this] () { poolUpdated(); });
          },
          [this](const DeleteTransaction& msg) {
            executeInRemoteThread([this] () { poolUpdated(); });
          }
        );

        messageQueue.pop();
      }
    } catch (System::InterruptedException&) {
    }
  });

  updateLastLocalBlockHeaderInfo();
  state = INITIALIZED;
  executeInRemoteThread([callback, ec] { callback(ec); });
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
  core.removeMessageQueue(messageQueue); // TODO: add RAII guard
  resetLastLocalBlockHeaderInfo();
  state = NOT_INITIALIZED;
  messageQueue.stop();

  lock.unlock();

  while(contextCounter > 0) {
    contextCounterEvent.wait();
    contextCounterEvent.clear();
  }

  return true;
}

//must be called from dispatcher's thread
void InProcessNode::executeInRemoteThread(std::function<void()>&& func) {
  System::RemoteContext<void> remoteContext(dispatcher, std::move(func));
  remoteContext.get();
}

//may be called from any thread
void InProcessNode::executeInDispatcherThread(std::function<void()>&& func) {
  remoteSpawn(dispatcher, std::move(func), contextCounter, contextCounterEvent);
}

void InProcessNode::getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds,
                                 std::vector<CryptoNote::RawBlock>& newBlocks, uint32_t& startIndex,
                                 const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([&newBlocks, &startIndex, callback, knownBlockIds, this] () mutable {
      auto ec = doGetNewBlocks(std::move(knownBlockIds), newBlocks, startIndex);
      executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doGetNewBlocks(const std::vector<Crypto::Hash>& knownBlockIds,
                                              std::vector<CryptoNote::RawBlock>& newBlocks, uint32_t& startHeight) {
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

    auto blockTemplate = core.getBlockByIndex(0);
    if (knownBlockIds.back() != CryptoNote::CachedBlock(blockTemplate).getBlockHash()) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    uint32_t totalBlockCount;
    auto supplement = core.findBlockchainSupplement(knownBlockIds, CryptoNote::COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT,
                                                    totalBlockCount, startHeight);

    for (const auto& blockId : supplement) {
      assert(core.hasBlock(blockId));
      auto completeBlock = core.getBlockByHash(blockId);

      RawBlock be;
      be.block = toBinaryArray(completeBlock);

      be.transactions.reserve(completeBlock.transactionHashes.size());
      std::vector<BinaryArray> binaryTransactions;
      std::vector<Crypto::Hash> missed;
      core.getTransactions(completeBlock.transactionHashes, binaryTransactions, missed);
      std::move(std::begin(binaryTransactions), std::end(binaryTransactions), std::back_inserter(be.transactions));
      newBlocks.push_back(std::move(be));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                    std::vector<uint32_t>& outsGlobalIndices,
                                                    const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=, &outsGlobalIndices] () {
    auto ec = doGetTransactionOutsGlobalIndices(transactionHash, outsGlobalIndices);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                                 std::vector<uint32_t>& outsGlobalIndices) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    bool r = core.getTransactionGlobalIndexes(transactionHash, outsGlobalIndices);
    if (!r) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getRandomOutsByAmounts(
    std::vector<uint64_t>&& amounts, uint16_t outsCount,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result,
    const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=, &result] () mutable {
    auto ec = doGetRandomOutsByAmounts(std::move(amounts), outsCount, result);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doGetRandomOutsByAmounts(
    std::vector<uint64_t>&& amounts, uint16_t outsCount,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response res;

    std::vector<uint32_t> indices;
    std::vector<Crypto::PublicKey> keys;

    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> tmpResult;
    for (auto amount : amounts) {
      indices.clear();
      keys.clear();

      if (!core.getRandomOutputs(amount, outsCount, indices, keys)) {
        return make_error_code(CryptoNote::error::REQUEST_ERROR);
      }

      assert(indices.size() == keys.size());

      CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount outsForAmount;
      outsForAmount.amount = amount;
      for (size_t i = 0; i < indices.size(); ++i) {
        outsForAmount.outs.push_back( {indices[i], keys[i]} );
      }

      tmpResult.push_back(std::move(outsForAmount));
    }

    result = std::move(tmpResult);
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=] () {
    auto ec = doRelayTransaction(transaction);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doRelayTransaction(const CryptoNote::Transaction& transaction) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    if (!core.addTransactionToPool(toBinaryArray(transaction))) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    protocol.relayTransactions({toBinaryArray(transaction)});
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

  RemotelySpawnedSyncContext<size_t> context(dispatcher, contextCounter, contextCounterEvent, [this] () {
    return protocol.getPeerCount();
  });

  return context.get();
}

uint32_t InProcessNode::getLocalBlockCount() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return lastLocalBlockHeaderInfo.index + 1;
}

uint32_t InProcessNode::getKnownBlockCount() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  RemotelySpawnedSyncContext<uint32_t> context(dispatcher, contextCounter, contextCounterEvent, [this] {
    return protocol.getObservedHeight();
  });

  return context.get();
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

  RemotelySpawnedSyncContext<uint32_t> context(dispatcher, contextCounter, contextCounterEvent, [this] {
    return protocol.getObservedHeight() - 1;
  });

  return context.get();
}

uint64_t InProcessNode::getLastLocalBlockTimestamp() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return lastLocalBlockHeaderInfo.timestamp;
}

BlockHeaderInfo InProcessNode::getLastLocalBlockHeaderInfo() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return lastLocalBlockHeaderInfo;
}

void InProcessNode::getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount, std::vector<Crypto::Hash>& blockHashes, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
  lock.unlock();

  executeInDispatcherThread([this, timestampBegin, secondsCount, &blockHashes, callback] () mutable {
    std::error_code ec;

    try {
      blockHashes = core.getBlockHashesByTimestamps(timestampBegin, secondsCount);
    } catch (std::system_error& e) {
      ec = e.code();
    } catch (std::exception&) {
      ec = make_error_code(error::INTERNAL_NODE_ERROR);
    }

    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

void InProcessNode::getTransactionHashesByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
  lock.unlock();

  executeInDispatcherThread([this, &paymentId, &transactionHashes, callback] () mutable {
    std::error_code ec;

    try {
      transactionHashes = core.getTransactionHashesByPaymentId(paymentId);
    } catch (std::system_error& e) {
      ec = e.code();
    } catch (std::exception&) {
      ec = make_error_code(error::INTERNAL_NODE_ERROR);
    }

    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

void InProcessNode::peerCountUpdated(size_t count) {
  observerManager.notify(&INodeObserver::peerCountUpdated, count);
}

void InProcessNode::lastKnownBlockHeightUpdated(uint32_t height) {
  observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, height - 1);
}

void InProcessNode::blockchainUpdated(uint32_t topBlockIndex) {
  std::unique_lock<std::mutex> lock(mutex);
  updateLastLocalBlockHeaderInfo();
  lock.unlock();
  observerManager.notify(&INodeObserver::localBlockchainUpdated, topBlockIndex /*core.getTopBlockIndex()*/);
}

void InProcessNode::chainSwitched(uint32_t topBlockIndex, uint32_t commonRoot, const std::vector<Crypto::Hash>& hashes) {
  observerManager.notify(&INodeObserver::chainSwitched, topBlockIndex, commonRoot, hashes);
}

void InProcessNode::poolUpdated() {
  observerManager.notify(&INodeObserver::poolChanged);
}

void InProcessNode::updateLastLocalBlockHeaderInfo() {
  Hash topBlockHash;
  uint32_t topBlockIndex;
  BlockTemplate block;
  Difficulty difficulty;
  try {
    topBlockHash = core.getTopBlockHash();
    topBlockIndex = core.getTopBlockIndex();
    block = core.getBlockByIndex(topBlockIndex);
    difficulty = core.getBlockDifficulty(topBlockIndex);
  } catch (const std::exception&) {
    return;
  }

  lastLocalBlockHeaderInfo.index = topBlockIndex;
  lastLocalBlockHeaderInfo.majorVersion = block.majorVersion;
  lastLocalBlockHeaderInfo.minorVersion = block.minorVersion;
  lastLocalBlockHeaderInfo.timestamp  = block.timestamp;
  lastLocalBlockHeaderInfo.hash = topBlockHash;
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

void InProcessNode::queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp,
                                std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight,
                                const Callback& callback) {
  auto lock = std::unique_lock<std::mutex>{mutex};
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=, &newBlocks, &startHeight] () mutable {
    auto ec = doQueryBlocksLite(std::move(knownBlockIds), timestamp, newBlocks, startHeight);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doQueryBlocksLite(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp,
                                                 std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight) {
  uint32_t currentHeight, fullOffset;
  std::vector<CryptoNote::BlockShortInfo> entries;

  if (!core.queryBlocksLite(knownBlockIds, timestamp, startHeight, currentHeight, fullOffset, entries)) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  for (const auto& entry : entries) {
    BlockShortEntry bse;
    bse.blockHash = entry.blockId;
    bse.hasBlock = false;

    if (!entry.block.empty()) {
      bse.hasBlock = true;
      if (!fromBinaryArray(bse.block, entry.block)) {
        return std::make_error_code(std::errc::invalid_argument);
      }
    }

    for (const auto& tsi : entry.txPrefixes) {
      TransactionShortInfo tpi;
      tpi.txId = tsi.txHash;
      tpi.txPrefix = tsi.txPrefix;

      bse.txsShortInfo.push_back(std::move(tpi));
    }

    newBlocks.push_back(std::move(bse));
  }

  return std::error_code();
}

void InProcessNode::getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId,
                                               bool& isBcActual,
                                               std::vector<std::unique_ptr<ITransactionReader>>& newTxs,
                                               std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=, &isBcActual, &newTxs, &deletedTxIds] () mutable {
    auto ec = doGetPoolSymmetricDifference(std::move(knownPoolTxIds), knownBlockId, isBcActual, newTxs, deletedTxIds);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doGetPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId,
                                                 bool& isBcActual,
                                                 std::vector<std::unique_ptr<ITransactionReader>>& newTxs,
                                                 std::vector<Crypto::Hash>& deletedTxIds) {
  std::error_code ec;

  std::vector<TransactionPrefixInfo> added;
  isBcActual = core.getPoolChangesLite(knownBlockId, knownPoolTxIds, added, deletedTxIds);

  try {
    for (const auto& tx : added) {
      newTxs.push_back(createTransactionPrefix(tx.txPrefix, tx.txHash));
    }
  } catch (std::system_error& ex) {
    ec = ex.code();
  } catch (std::exception&) {
    ec = make_error_code(std::errc::invalid_argument);
  }

  return ec;
}

void InProcessNode::getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out,
                                                         const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=, &out] () {
    auto ec = doGetOutputByMultisigGlobalIndex(amount, gindex, out);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doGetOutputByMultisigGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out) {
  std::error_code ec = std::error_code();
  auto result = core.getMultisignatureOutput(amount, gindex);
  if (!result) {
    ec = make_error_code(std::errc::invalid_argument);
    return ec;
  }

  out = result->first;
  return ec;
}

void InProcessNode::getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks,
                              const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=, &blocks] () {
    auto ec = doGetBlocks(blockHeights, blocks);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doGetBlocks(const std::vector<uint32_t>& blockIndexes,
                                           std::vector<std::vector<BlockDetails>>& blocks) {
  try {
    auto topIndex = core.getTopBlockIndex();
    for (auto index : blockIndexes) {
      if (index > topIndex) {
        return make_error_code(CryptoNote::error::REQUEST_ERROR);
      }
      Crypto::Hash hash = core.getBlockHashByIndex(index);
      BlockDetails blockDetails = core.getBlockDetails(hash);
      std::vector<BlockDetails> blocksOnSameIndex;
      blocksOnSameIndex.push_back(std::move(blockDetails));

      // Getting alternative blocks
      std::vector<Crypto::Hash> alternativeBlocks = core.getAlternativeBlockHashesByIndex(index);
      for (const auto& alternativeBlockHash : alternativeBlocks) {
        BlockDetails alternativeBlockDetails = core.getBlockDetails(alternativeBlockHash);
        blocksOnSameIndex.push_back(std::move(alternativeBlockDetails));
      }
      blocks.push_back(std::move(blocksOnSameIndex));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

void InProcessNode::getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks,
                              const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=, &blocks] () {
    auto ec = doGetBlocks(blockHashes, blocks);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doGetBlocks(const std::vector<Crypto::Hash>& blockHashes,
                                           std::vector<BlockDetails>& blocks) {
  try {
    for (auto& hash : blockHashes) {
      if (!core.hasBlock(hash)) {
        return make_error_code(CryptoNote::error::REQUEST_ERROR);
      }
      BlockDetails blockDetails = core.getBlockDetails(hash);
      blocks.push_back(std::move(blockDetails));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}


void InProcessNode::getTransactions(const std::vector<Crypto::Hash>& transactionHashes,
                                    std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  executeInDispatcherThread([=, &transactions] () {
    auto ec = doGetTransactions(transactionHashes, transactions);
    executeInRemoteThread([callback, ec] () { callback(ec); });
  });
}

std::error_code InProcessNode::doGetTransactions(const std::vector<Crypto::Hash>& transactionHashes,
                                                 std::vector<TransactionDetails>& transactions) {
  try {
    for (const auto& hash : transactionHashes) {
      if (!core.hasTransaction(hash)) {
        return make_error_code(CryptoNote::error::REQUEST_ERROR);
      }
      TransactionDetails transactionDetails = core.getTransactionDetails(hash);
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

  executeInDispatcherThread([=, &syncStatus] () {
    syncStatus = protocol.isSynchronized();
    executeInRemoteThread([callback] () { callback({}); });
  });
}

} //namespace CryptoNote
