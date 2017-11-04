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

#include "BlockchainExplorer.h"

#include <future>
#include <functional>
#include <memory>
#include <utility>

#include "BlockchainExplorerErrors.h"
#include "Common/StdOutputStream.h"
#include "Common/StdInputStream.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteConfig.h"

#include "ITransaction.h"

using namespace Logging;
using namespace Crypto;

namespace CryptoNote {

class ContextCounterHolder
{
public:
  ContextCounterHolder(BlockchainExplorer::AsyncContextCounter& counter) : counter(counter) {}
  ~ContextCounterHolder() { counter.delAsyncContext(); }

private:
  BlockchainExplorer::AsyncContextCounter& counter;
};

class NodeRequest {
public:

  NodeRequest(const std::function<void(const INode::Callback&)>& request) : requestFunc(request) {}

  std::error_code performBlocking() {
    std::promise<std::error_code> promise;
    std::future<std::error_code> future = promise.get_future();
    requestFunc([&](std::error_code c) {
      blockingCompleteionCallback(std::move(promise), c);
    });
    return future.get();
  }

  void performAsync(BlockchainExplorer::AsyncContextCounter& asyncContextCounter, const INode::Callback& callback) {
    asyncContextCounter.addAsyncContext();
    requestFunc(std::bind(&NodeRequest::asyncCompleteionCallback, callback, std::ref(asyncContextCounter), std::placeholders::_1));
  }

private:
  void blockingCompleteionCallback(std::promise<std::error_code> promise, std::error_code ec) {
    promise.set_value(ec);
  }

  static void asyncCompleteionCallback(const INode::Callback& callback, BlockchainExplorer::AsyncContextCounter& asyncContextCounter, std::error_code ec) {
    ContextCounterHolder counterHolder(asyncContextCounter);
    try {
      callback(ec);
    } catch (...) {
      return;
    } 
  }

  const std::function<void(const INode::Callback&)> requestFunc;
};


BlockchainExplorer::PoolUpdateGuard::PoolUpdateGuard() :
  m_state(State::NONE) {
}

bool BlockchainExplorer::PoolUpdateGuard::beginUpdate() {
  auto state = m_state.load();
  for (;;) {
    switch (state) {
    case State::NONE:
      if (m_state.compare_exchange_weak(state, State::UPDATING)) {
        return true;
      }
      break;

    case State::UPDATING:
      if (m_state.compare_exchange_weak(state, State::UPDATE_REQUIRED)) {
        return false;
      }
      break;

    case State::UPDATE_REQUIRED:
      return false;

    default:
      assert(false);
      return false;
    }
  }
}

bool BlockchainExplorer::PoolUpdateGuard::endUpdate() {
  auto state = m_state.load();
  for (;;) {
    assert(state != State::NONE);

    if (m_state.compare_exchange_weak(state, State::NONE)) {
      return state == State::UPDATE_REQUIRED;
    }
  }
}

class ScopeExitHandler {
public:
  ScopeExitHandler(std::function<void()>&& handler) :
    m_handler(std::move(handler)),
    m_cancelled(false) {
  }

  ~ScopeExitHandler() {
    if (!m_cancelled) {
      m_handler();
    }
  }

  void reset() {
    m_cancelled = true;
  }

private:
  std::function<void()> m_handler;
  bool m_cancelled;
};

BlockchainExplorer::BlockchainExplorer(INode& node, Logging::ILogger& logger) :
  node(node), 
  logger(logger, "BlockchainExplorer"),
  database(database),
  state(NOT_INITIALIZED), 
  synchronized(false), 
  observersCounter(0) {
}

BlockchainExplorer::~BlockchainExplorer() {}
    
bool BlockchainExplorer::addObserver(IBlockchainObserver* observer) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }
  observersCounter.fetch_add(1);
  return observerManager.add(observer);
}

bool BlockchainExplorer::removeObserver(IBlockchainObserver* observer) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }
  if (observersCounter.load() != 0) {
    observersCounter.fetch_sub(1);
  }
  return observerManager.remove(observer);
}

struct StateRollback {
  StateRollback(std::atomic<State>& s) : state(s) {
    state.store(INITIALIZED);
  }
  void commit() { done = true; }
  ~StateRollback() {
    if (!done) {
      state.store(NOT_INITIALIZED);
    }
  }
  bool done = false;
  std::atomic<State>& state;
};

void BlockchainExplorer::init() {
  if (state.load() != NOT_INITIALIZED) {
    logger(ERROR) << "Init called on already initialized BlockchainExplorer.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::ALREADY_INITIALIZED));
  }

  if (!getBlockchainTop(knownBlockchainTop, false)) {
    logger(ERROR) << "Can't get blockchain top.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }

  std::vector<Crypto::Hash> knownPoolTransactionHashes;
  bool isBlockchainActual;
  std::vector<TransactionDetails> newTransactions;
  std::vector<Crypto::Hash> removedTransactions;
  StateRollback stateRollback(state);
  if (!getPoolState(knownPoolTransactionHashes, knownBlockchainTop.hash, isBlockchainActual, newTransactions, removedTransactions)) {
    logger(ERROR) << "Can't get pool state.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }

  assert(removedTransactions.empty());

  if (node.addObserver(this)) {
    stateRollback.commit();
  } else {
    logger(ERROR) << "Can't add observer to node.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
}

void BlockchainExplorer::shutdown() {
  if (state.load() != INITIALIZED) {
    logger(ERROR) << "Shutdown called on not initialized BlockchainExplorer.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  node.removeObserver(this);
  asyncContextCounter.waitAsyncContextsFinish();
  state.store(NOT_INITIALIZED);
}

bool BlockchainExplorer::getBlocks(const std::vector<uint32_t>& blockIndexes, std::vector<std::vector<BlockDetails>>& blocks) {
  return getBlocks(blockIndexes, blocks, true);
}

bool BlockchainExplorer::getBlocks(const std::vector<uint32_t>& blockIndexes, std::vector<std::vector<BlockDetails>>& blocks, bool checkInitialization) {
  if (checkInitialization && state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  if (blockIndexes.empty()) {
    return true;
  }

  logger(DEBUGGING) << "Get blocks by index request came.";
  NodeRequest request([&](const INode::Callback& cb) { node.getBlocks(blockIndexes, blocks, cb); });
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get blocks by index: " << ec.message();
    throw std::system_error(ec);
  }
  assert(blocks.size() == blockIndexes.size());
  return true;
}

bool BlockchainExplorer::getBlocks(const std::vector<Hash>& blockHashes, std::vector<BlockDetails>& blocks) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  if (blockHashes.empty()) {
    return true;
  }

  logger(DEBUGGING) << "Get blocks by hash request came.";
  NodeRequest request([&](const INode::Callback& cb) { node.getBlocks(blockHashes, blocks, cb); });
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get blocks by hash: " << ec.message();
    throw std::system_error(ec);
  }

  assert(blocks.size() == blockHashes.size());
  return true;
}

bool BlockchainExplorer::getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  if (timestampBegin > timestampEnd) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::REQUEST_ERROR), "timestampBegin must not be greater than timestampEnd");
  }

  logger(DEBUGGING) << "Get blocks by timestamp " << timestampBegin << " - " << timestampEnd << " request came.";

  std::vector<Hash> blockHashes;
  NodeRequest request([&](const INode::Callback& cb) { node.getBlockHashesByTimestamps(timestampBegin, timestampEnd - timestampBegin + 1, blockHashes, cb); });
  auto ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get blocks hashes by timestamps: " << ec.message();
    throw std::system_error(ec);
  }

  blocksNumberWithinTimestamps = static_cast<uint32_t>(blockHashes.size());

  if (blocksNumberLimit < blocksNumberWithinTimestamps) {
    blockHashes.erase(std::next(blockHashes.begin(), blocksNumberLimit), blockHashes.end());
  }

  if (blockHashes.empty()) {
    throw std::runtime_error("block hashes not found");
  }

  return getBlocks(blockHashes, blocks);
}

bool BlockchainExplorer::getBlockchainTop(BlockDetails& topBlock) {
  return getBlockchainTop(topBlock, true);
}

bool BlockchainExplorer::getBlockchainTop(BlockDetails& topBlock, bool checkInitialization) {
  if (checkInitialization && state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get blockchain top request came.";
  uint32_t lastIndex = node.getLastLocalBlockHeight();

  std::vector<uint32_t> indexes;
  indexes.push_back(std::move(lastIndex));

  std::vector<std::vector<BlockDetails>> blocks;
  if (!getBlocks(indexes, blocks, checkInitialization)) {
    logger(ERROR) << "Can't get blockchain top.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
  assert(blocks.size() == indexes.size() && blocks.size() == 1);

  bool gotMainchainBlock = false;
  for (const BlockDetails& block : blocks.back()) {
    if (!block.isAlternative) {
      topBlock = block;
      gotMainchainBlock = true;
      break;
    }
  }

  if (!gotMainchainBlock) {
    logger(ERROR) << "Can't get blockchain top: all blocks on index " << lastIndex << " are orphaned.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
  return true;
}

bool BlockchainExplorer::getTransactions(const std::vector<Hash>& transactionHashes, std::vector<TransactionDetails>& transactions) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  if (transactionHashes.empty()) {
    return true;
  }

  logger(DEBUGGING) << "Get transactions by hash request came.";
  NodeRequest request(
      [&](const INode::Callback& cb) { return node.getTransactions(transactionHashes, transactions, cb); });
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get transactions by hash: " << ec.message();
    throw std::system_error(ec);
  }
  return true;
}

bool BlockchainExplorer::getTransactionsByPaymentId(const Hash& paymentId, std::vector<TransactionDetails>& transactions) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get transactions by payment id " << paymentId << " request came.";

  std::vector<Crypto::Hash> transactionHashes;
  NodeRequest request([&](const INode::Callback& cb) { return node.getTransactionHashesByPaymentId(paymentId, transactionHashes, cb); });

  auto ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get transaction hashes: " << ec.message();
    throw std::system_error(ec);
  }

  if (transactionHashes.empty()) {
    return false;
  }

  return getTransactions(transactionHashes, transactions);
}

bool BlockchainExplorer::getPoolState(const std::vector<Hash>& knownPoolTransactionHashes, Hash knownBlockchainTopHash, bool& isBlockchainActual, std::vector<TransactionDetails>& newTransactions, std::vector<Hash>& removedTransactions) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get pool state request came.";
  std::vector<std::unique_ptr<ITransactionReader>> rawNewTransactions;

  NodeRequest request(
    [&](const INode::Callback& callback) {
      std::vector<Hash> hashes;
      for (Hash hash : knownPoolTransactionHashes) {
        hashes.push_back(std::move(hash));
      }

      node.getPoolSymmetricDifference(
        std::move(hashes),
        reinterpret_cast<Hash&>(knownBlockchainTopHash),
        isBlockchainActual,
        rawNewTransactions,
        removedTransactions,
        callback
      );
    }
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get pool state: " << ec.message();
    throw std::system_error(ec);
  }

  std::vector<Hash> newTransactionsHashes;
  for (const auto& rawTransaction : rawNewTransactions) {
    Hash transactionHash = rawTransaction->getTransactionHash();
    newTransactionsHashes.push_back(std::move(transactionHash));
  }

  return getTransactions(newTransactionsHashes, newTransactions);
}

uint64_t BlockchainExplorer::getRewardBlocksWindow() {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }
  return parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW;
}

uint64_t BlockchainExplorer::getFullRewardMaxBlockSize(uint8_t majorVersion) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  if (majorVersion >= BLOCK_MAJOR_VERSION_3) {
    return parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
  } else if (majorVersion == BLOCK_MAJOR_VERSION_2) {
    return parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2;
  } else {
    return parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1;
  }
}

bool BlockchainExplorer::isSynchronized() {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Synchronization status request came.";
  bool syncStatus = false;
  NodeRequest request([&](const INode::Callback& cb) { node.isSynchronized(syncStatus, cb); });
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get synchronization status: " << ec.message();
    throw std::system_error(ec);
  }

  synchronized.store(syncStatus);
  return syncStatus;
}

void BlockchainExplorer::poolChanged() {
  logger(DEBUGGING) << "Got poolChanged notification.";

  if (!synchronized.load() || observersCounter.load() == 0) {
    return;
  }

  if (!poolUpdateGuard.beginUpdate()) {
    return;
  }

  ScopeExitHandler poolUpdateEndGuard(std::bind(&BlockchainExplorer::poolUpdateEndHandler, this));

  std::unique_lock<std::mutex> lock(mutex);

  auto rawNewTransactionsPtr = std::make_shared<std::vector<std::unique_ptr<ITransactionReader>>>();
  auto removedTransactionsPtr = std::make_shared<std::vector<Hash>>();
  auto isBlockchainActualPtr = std::make_shared<bool>(false);

  NodeRequest request(
    [this, rawNewTransactionsPtr, removedTransactionsPtr, isBlockchainActualPtr](const INode::Callback& callback) {
      std::vector<Hash> hashes;
      hashes.reserve(knownPoolState.size());
      for (const std::pair<Hash, TransactionDetails>& kv : knownPoolState) {
        hashes.push_back(kv.first);
      }
      node.getPoolSymmetricDifference(
        std::move(hashes),
        reinterpret_cast<Hash&>(knownBlockchainTop.hash),
        *isBlockchainActualPtr,
        *rawNewTransactionsPtr,
        *removedTransactionsPtr,
        callback
      );
    }
  );

  request.performAsync(asyncContextCounter,
    [this, rawNewTransactionsPtr, removedTransactionsPtr, isBlockchainActualPtr](std::error_code ec) {
      ScopeExitHandler poolUpdateEndGuard(std::bind(&BlockchainExplorer::poolUpdateEndHandler, this));

      if (ec) {
        logger(ERROR) << "Can't send poolChanged notification because can't get pool symmetric difference: " << ec.message();
        return;
      }

      std::unique_lock<std::mutex> lock(mutex);

      std::shared_ptr<std::vector<Hash>> newTransactionsHashesPtr = std::make_shared<std::vector<Hash>>();
      newTransactionsHashesPtr->reserve(rawNewTransactionsPtr->size());
      for (const auto& rawTransaction : *rawNewTransactionsPtr) {
        auto hash = rawTransaction->getTransactionHash();
        logger(DEBUGGING) << "Pool responded with new transaction: " << hash;
        if (knownPoolState.count(hash) == 0) {
          newTransactionsHashesPtr->push_back(hash);
        }
      }
      
      auto removedTransactionsHashesPtr = std::make_shared<std::vector<std::pair<Hash, TransactionRemoveReason>>>();
      removedTransactionsHashesPtr->reserve(removedTransactionsPtr->size());
      for (const Hash& hash : *removedTransactionsPtr) {
        logger(DEBUGGING) << "Pool responded with deleted transaction: " << hash;
        auto iter = knownPoolState.find(hash);
        if (iter != knownPoolState.end()) {
          removedTransactionsHashesPtr->push_back({
              hash,
              TransactionRemoveReason::INCLUDED_IN_BLOCK // Can't have real reason here.
          });
        }
      }

      std::shared_ptr<std::vector<TransactionDetails>> newTransactionsPtr = std::make_shared<std::vector<TransactionDetails>>();
      newTransactionsPtr->reserve(newTransactionsHashesPtr->size());
      NodeRequest request([&](const INode::Callback& cb) { 
        node.getTransactions(*newTransactionsHashesPtr, *newTransactionsPtr, cb);
      });

      request.performAsync(asyncContextCounter,
        [this, newTransactionsHashesPtr, newTransactionsPtr, removedTransactionsHashesPtr](std::error_code ec) {
          ScopeExitHandler poolUpdateEndGuard(std::bind(&BlockchainExplorer::poolUpdateEndHandler, this));

          if (ec) {
            logger(ERROR) << "Can't send poolChanged notification because can't get transactions: " << ec.message();
            return;
          }

          {
            std::unique_lock<std::mutex> lock(mutex);
            for (const TransactionDetails& tx : *newTransactionsPtr) {
              if (knownPoolState.count(tx.hash) == 0) {
                knownPoolState.emplace(tx.hash, tx);
              }
            }

            for (const std::pair<Crypto::Hash, TransactionRemoveReason> kv : *removedTransactionsHashesPtr) {
              auto iter = knownPoolState.find(kv.first);
              if (iter != knownPoolState.end()) {
                knownPoolState.erase(iter);
              }
            }
          }

          if (!newTransactionsPtr->empty() || !removedTransactionsHashesPtr->empty()) {
            observerManager.notify(&IBlockchainObserver::poolUpdated, *newTransactionsPtr, *removedTransactionsHashesPtr);
            logger(DEBUGGING) << "poolUpdated notification was successfully sent.";
          }
        }
      );

      poolUpdateEndGuard.reset();
    }
  );

  poolUpdateEndGuard.reset();
}

void BlockchainExplorer::poolUpdateEndHandler() {
  if (poolUpdateGuard.endUpdate()) {
    poolChanged();
  }
}

void BlockchainExplorer::blockchainSynchronized(uint32_t topIndex) {
  logger(DEBUGGING) << "Got blockchainSynchronized notification.";

  synchronized.store(true);

  if (observersCounter.load() == 0) {
    return;
  }

  BlockDetails topBlock;
  {
    std::unique_lock<std::mutex> lock(mutex);
    topBlock = knownBlockchainTop;
  }

  if (topBlock.index == topIndex) {
    observerManager.notify(&IBlockchainObserver::blockchainSynchronized, topBlock);
    return;
  }

  std::shared_ptr<std::vector<uint32_t>> blockIndexesPtr = std::make_shared<std::vector<uint32_t>>();
  std::shared_ptr<std::vector<std::vector<BlockDetails>>> blocksPtr = std::make_shared<std::vector<std::vector<BlockDetails>>>();

  blockIndexesPtr->push_back(topIndex);

  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
        const std::vector<uint32_t>&,
          std::vector<std::vector<BlockDetails>>&, 
          const INode::Callback&
        )
      >(&INode::getBlocks), 
      std::ref(node), 
      std::cref(*blockIndexesPtr),
      std::ref(*blocksPtr),
      std::placeholders::_1
    )
  );

  request.performAsync(asyncContextCounter,
    [this, blockIndexesPtr, blocksPtr, topIndex](std::error_code ec) {
      if (ec) {
        logger(ERROR) << "Can't send blockchainSynchronized notification because can't get blocks by height: " << ec.message();
        return;
      }
      assert(blocksPtr->size() == blockIndexesPtr->size() && blocksPtr->size() == 1);

      auto mainchainBlockIter = std::find_if_not(blocksPtr->front().cbegin(), blocksPtr->front().cend(), [](const BlockDetails& block){ return block.isAlternative; });
      assert(mainchainBlockIter != blocksPtr->front().cend());

      observerManager.notify(&IBlockchainObserver::blockchainSynchronized, *mainchainBlockIter);
      logger(DEBUGGING) << "blockchainSynchronized notification was successfully sent.";
    }
  );
}

void BlockchainExplorer::localBlockchainUpdated(uint32_t index) {
  logger(DEBUGGING) << "Got localBlockchainUpdated notification.";
  
  std::unique_lock<std::mutex> lock(mutex);
  assert(index >= knownBlockchainTop.index);
  if (index == knownBlockchainTop.index) {
    return;
  }

  auto blockIndexesPtr = std::make_shared<std::vector<uint32_t>>();
  auto blocksPtr = std::make_shared<std::vector<std::vector<BlockDetails>>>();

  for (uint32_t i = knownBlockchainTop.index + 1; i <= index; ++i) {
    blockIndexesPtr->push_back(i);
  }

  NodeRequest request([=](const INode::Callback& cb) { node.getBlocks(*blockIndexesPtr, *blocksPtr, cb); });

  request.performAsync(asyncContextCounter,
    [this, blockIndexesPtr, blocksPtr](std::error_code ec) {
      if (ec) {
        logger(ERROR) << "Can't send blockchainUpdated notification because can't get blocks by height: " << ec.message();
        return;
      }
      assert(blocksPtr->size() == blockIndexesPtr->size());
      handleBlockchainUpdatedNotification(*blocksPtr);
    }
  );
}

void BlockchainExplorer::chainSwitched(uint32_t newTopIndex, uint32_t commonRoot, const std::vector<Crypto::Hash>& hashes) {
  assert(newTopIndex > commonRoot);
  std::shared_ptr<std::vector<uint32_t>> blockIndexesPtr = std::make_shared<std::vector<uint32_t>>();
  std::shared_ptr<std::vector<std::vector<BlockDetails>>> blocksPtr = std::make_shared<std::vector<std::vector<BlockDetails>>>();
  blockIndexesPtr->reserve(newTopIndex - commonRoot);
  blocksPtr->reserve(newTopIndex - commonRoot);

  for (uint32_t i = commonRoot + 1; i <= newTopIndex; ++i) {
    blockIndexesPtr->push_back(i);
  }

  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
          const std::vector<uint32_t>&,
          std::vector<std::vector<BlockDetails>>&,
          const INode::Callback&
        )
      >(&INode::getBlocks),
      std::ref(node),
      std::cref(*blockIndexesPtr),
      std::ref(*blocksPtr),
      std::placeholders::_1
    )
  );

  request.performAsync(asyncContextCounter,
    [this, blockIndexesPtr, blocksPtr](std::error_code ec) {
      if (ec) {
        logger(ERROR) << "Can't send blockchainUpdated notification because can't get blocks by height: " << ec.message();
        return;
      }
      assert(blocksPtr->size() == blockIndexesPtr->size());
      handleBlockchainUpdatedNotification(*blocksPtr);
    }
  );
}

void BlockchainExplorer::handleBlockchainUpdatedNotification(const std::vector<std::vector<BlockDetails>>& blocks) {
  std::vector<BlockDetails> newBlocks;
  std::vector<BlockDetails> alternativeBlocks;
  {
    std::unique_lock<std::mutex> lock(mutex);

    BlockDetails topMainchainBlock;
    bool gotTopMainchainBlock = false;
    uint64_t topHeight = 0;

    for (const std::vector<BlockDetails>& sameHeightBlocks : blocks) {
      for (const BlockDetails& block : sameHeightBlocks) {
        if (topHeight < block.index) {
          topHeight = block.index;
          gotTopMainchainBlock = false;
        }

        if (block.isAlternative) {
          alternativeBlocks.push_back(block);
        } else {
          //assert(block.hash != knownBlockchainTop.hash);
          newBlocks.push_back(block);
          if (!gotTopMainchainBlock) {
            topMainchainBlock = block;
            gotTopMainchainBlock = true;
          }
        }
      }
    }

    assert(gotTopMainchainBlock);

    knownBlockchainTop = topMainchainBlock;
  }

  observerManager.notify(&IBlockchainObserver::blockchainUpdated, newBlocks, alternativeBlocks);
  logger(DEBUGGING) << "localBlockchainUpdated notification was successfully sent.";
}

}
