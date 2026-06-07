// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include "BlockchainExplorer.h"

#include <future>
#include <functional>
#include <memory>

#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteConfig.h"

#include "BlockchainExplorerErrors.h"
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
    requestFunc([&](std::error_code c){
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

void BlockchainExplorer::init() {
  if (state.load() != NOT_INITIALIZED) {
    logger(ERROR) << "Init called on already initialized BlockchainExplorer.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::ALREADY_INITIALIZED));
  }
  if (node.addObserver(this)) {
    state.store(INITIALIZED);
  } else {
    logger(ERROR) << "Can't add observer to node.";
    state.store(NOT_INITIALIZED);
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
  if (getBlockchainTop(knownBlockchainTop)) {
    knownBlockchainTopHeight = knownBlockchainTop.height;
  } else {
    logger(ERROR) << "Can't get blockchain top.";
    state.store(NOT_INITIALIZED);
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

bool BlockchainExplorer::getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get blocks by height request came.";
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
      std::cref(blockHeights), 
      std::ref(blocks),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get blocks by height: " << ec.message();
    throw std::system_error(ec);
  }
  assert(blocks.size() == blockHeights.size());
  return true;
}

bool BlockchainExplorer::getBlocks(const std::vector<Hash>& blockHashes, std::vector<BlockDetails>& blocks) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get blocks by hash request came.";
  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
          const std::vector<Hash>&, 
          std::vector<BlockDetails>&, 
          const INode::Callback&
        )
      >(&INode::getBlocks), 
      std::ref(node), 
      std::cref(reinterpret_cast<const std::vector<Hash>&>(blockHashes)), 
      std::ref(blocks),
      std::placeholders::_1
    )
  );
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

  logger(DEBUGGING) << "Get blocks by timestamp request came.";
  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
          uint64_t,
          uint64_t, 
          uint32_t,
          std::vector<BlockDetails>&, 
          uint32_t&,
          const INode::Callback&
        )
      >(&INode::getBlocks), 
      std::ref(node), 
      timestampBegin,
      timestampEnd,
      blocksNumberLimit,
      std::ref(blocks),
      std::ref(blocksNumberWithinTimestamps),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get blocks by timestamp: " << ec.message();
    throw std::system_error(ec);
  }
  return true;
}

bool BlockchainExplorer::getBlockchainTop(BlockDetails& topBlock) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get blockchain top request came.";
  uint32_t lastHeight = node.getLastLocalBlockHeight();

  std::vector<uint32_t> heights;
  heights.push_back(std::move(lastHeight));

  std::vector<std::vector<BlockDetails>> blocks;
  if (!getBlocks(heights, blocks)) {
    logger(ERROR) << "Can't get blockchain top.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
  assert(blocks.size() == heights.size() && blocks.size() == 1);

  bool gotMainchainBlock = false;
  for (const BlockDetails& block : blocks.back()) {
    if (!block.isOrphaned) {
      topBlock = block;
      gotMainchainBlock = true;
      break;
    }
  }

  if (!gotMainchainBlock) {
    logger(ERROR) << "Can't get blockchain top: all blocks on height " << lastHeight << " are orphaned.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
  return true;
}

bool BlockchainExplorer::getTransactions(const std::vector<Hash>& transactionHashes, std::vector<TransactionDetails>& transactions) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get transactions by hash request came.";
  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
          const std::vector<Hash>&, 
          std::vector<TransactionDetails>&, 
          const INode::Callback&
        )
      >(&INode::getTransactions), 
      std::ref(node), 
      std::cref(reinterpret_cast<const std::vector<Hash>&>(transactionHashes)), 
      std::ref(transactions),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get transactions by hash: " << ec.message();
    throw std::system_error(ec);
  }
  return true;
}

bool BlockchainExplorer::getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get transactions by timestamp request came.";
  NodeRequest request(
    std::bind(
      &INode::getPoolTransactions, 
      std::ref(node), 
      timestampBegin,
      timestampEnd,
      transactionsNumberLimit,
      std::ref(transactions),
      std::ref(transactionsNumberWithinTimestamps),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get transactions by timestamp: " << ec.message();
    throw std::system_error(ec);
  }
  return true;
}

bool BlockchainExplorer::getTransactionsByPaymentId(const Hash& paymentId, std::vector<TransactionDetails>& transactions) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get transactions by payment id request came.";
  NodeRequest request(
    std::bind(
      &INode::getTransactionsByPaymentId, 
      std::ref(node), 
      std::cref(reinterpret_cast<const Hash&>(paymentId)), 
      std::ref(transactions),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get transactions by payment id: " << ec.message();
    throw std::system_error(ec);
  }
  return true;
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
  NodeRequest request(
    std::bind(
      &INode::isSynchronized, 
      std::ref(node), 
      std::ref(syncStatus),
      std::placeholders::_1
    )
  );
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

  std::shared_ptr<std::vector<std::unique_ptr<ITransactionReader>>> rawNewTransactionsPtr = std::make_shared<std::vector<std::unique_ptr<ITransactionReader>>>();
  std::shared_ptr<std::vector<Hash>> removedTransactionsPtr = std::make_shared<std::vector<Hash>>();
  std::shared_ptr<bool> isBlockchainActualPtr = std::make_shared<bool>(false);

  NodeRequest request(
    [this, rawNewTransactionsPtr, removedTransactionsPtr, isBlockchainActualPtr](const INode::Callback& callback) {
      std::vector<Hash> hashes;
      for (const Hash& hash : knownPoolState) {
        hashes.push_back(std::move(hash));
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
      for (const auto& rawTransaction : *rawNewTransactionsPtr) {
        auto hash = rawTransaction->getTransactionHash();
        Hash transactionHash = reinterpret_cast<const Hash&>(hash);
        bool inserted = knownPoolState.emplace(transactionHash).second;
        if (inserted) {
          newTransactionsHashesPtr->push_back(std::move(transactionHash));
        }
      }
      
      std::shared_ptr<std::vector<std::pair<Hash, TransactionRemoveReason>>> removedTransactionsHashesPtr = std::make_shared<std::vector<std::pair<Hash, TransactionRemoveReason>>>();
      for (const Hash hash : *removedTransactionsPtr) {
        auto iter = knownPoolState.find(hash);
        if (iter != knownPoolState.end()) {
          removedTransactionsHashesPtr->push_back({
              hash,
              TransactionRemoveReason::INCLUDED_IN_BLOCK // Can't have real reason here.
          });
          knownPoolState.erase(iter);
        }
      }

      std::shared_ptr<std::vector<TransactionDetails>> newTransactionsPtr = std::make_shared<std::vector<TransactionDetails>>();
      NodeRequest request(
        std::bind(
          static_cast<
            void(INode::*)(
              const std::vector<Hash>&, 
              std::vector<TransactionDetails>&, 
              const INode::Callback&
            )
          >(&INode::getTransactions), 
          std::ref(node), 
          std::cref(*newTransactionsHashesPtr), 
          std::ref(*newTransactionsPtr),
          std::placeholders::_1
        )
      );

      request.performAsync(asyncContextCounter,
        [this, newTransactionsHashesPtr, newTransactionsPtr, removedTransactionsHashesPtr](std::error_code ec) {
          ScopeExitHandler poolUpdateEndGuard(std::bind(&BlockchainExplorer::poolUpdateEndHandler, this));

          if (ec) {
            logger(ERROR) << "Can't send poolChanged notification because can't get transactions: " << ec.message();
            return;
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

void BlockchainExplorer::blockchainSynchronized(uint32_t topHeight) {
  logger(DEBUGGING) << "Got blockchainSynchronized notification.";

  synchronized.store(true);

  if (observersCounter.load() == 0) {
    return;
  }

  std::shared_ptr<std::vector<uint32_t>> blockHeightsPtr = std::make_shared<std::vector<uint32_t>>();
  std::shared_ptr<std::vector<std::vector<BlockDetails>>> blocksPtr = std::make_shared<std::vector<std::vector<BlockDetails>>>();

  blockHeightsPtr->push_back(topHeight);

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
      std::cref(*blockHeightsPtr), 
      std::ref(*blocksPtr),
      std::placeholders::_1
    )
  );

  request.performAsync(asyncContextCounter,
    [this, blockHeightsPtr, blocksPtr, topHeight](std::error_code ec) {
      if (ec) {
        logger(ERROR) << "Can't send blockchainSynchronized notification because can't get blocks by height: " << ec.message();
        return;
      }
      assert(blocksPtr->size() == blockHeightsPtr->size() && blocksPtr->size() == 1);

      BlockDetails topMainchainBlock;
      bool gotMainchainBlock = false;
      for (const BlockDetails& block : blocksPtr->back()) {
        if (!block.isOrphaned) {
          topMainchainBlock = block;
          gotMainchainBlock = true;
          break;
        }
      }

      if (!gotMainchainBlock) {
        logger(ERROR) << "Can't send blockchainSynchronized notification because can't get blockchain top: all blocks on height " << topHeight << " are orphaned.";
        return;
      }

      observerManager.notify(&IBlockchainObserver::blockchainSynchronized, topMainchainBlock);
      logger(DEBUGGING) << "blockchainSynchronized notification was successfully sent.";
    }
  );
}

void BlockchainExplorer::localBlockchainUpdated(uint32_t height) {
  logger(DEBUGGING) << "Got localBlockchainUpdated notification.";

  if (observersCounter.load() == 0) {
    knownBlockchainTopHeight = height;
    return;
  }

  std::unique_lock<std::mutex> lock(mutex);

  assert(height >= knownBlockchainTopHeight);

  std::shared_ptr<std::vector<uint32_t>> blockHeightsPtr = std::make_shared<std::vector<uint32_t>>();
  std::shared_ptr<std::vector<std::vector<BlockDetails>>> blocksPtr = std::make_shared<std::vector<std::vector<BlockDetails>>>();

  for (uint32_t i = knownBlockchainTopHeight; i <= height; ++i) {
    blockHeightsPtr->push_back(i);
  }

  knownBlockchainTopHeight = height;

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
      std::cref(*blockHeightsPtr), 
      std::ref(*blocksPtr),
      std::placeholders::_1
    )
  );

  request.performAsync(asyncContextCounter,
    [this, blockHeightsPtr, blocksPtr](std::error_code ec) {
      if (ec) {
        logger(ERROR) << "Can't send blockchainUpdated notification because can't get blocks by height: " << ec.message();
        return;
      }
      assert(blocksPtr->size() == blockHeightsPtr->size());

      std::unique_lock<std::mutex> lock(mutex);

      BlockDetails topMainchainBlock;
      bool gotTopMainchainBlock = false;
      uint64_t topHeight = 0;

      std::vector<BlockDetails> newBlocks;
      std::vector<BlockDetails> orphanedBlocks;
      for (const std::vector<BlockDetails>& sameHeightBlocks : *blocksPtr) {
        for (const BlockDetails& block : sameHeightBlocks) {
          if (topHeight < block.height) {
            topHeight = block.height;
            gotTopMainchainBlock = false;
          }
          if (block.isOrphaned) {
            orphanedBlocks.push_back(block);
          } else {
            if (block.height > knownBlockchainTop.height || block.hash != knownBlockchainTop.hash) {
              newBlocks.push_back(block);
            }
            if (!gotTopMainchainBlock) {
              topMainchainBlock = block;
              gotTopMainchainBlock = true;
            }
          }
        }
      }

      if (!gotTopMainchainBlock) {
        logger(ERROR) << "Can't send localBlockchainUpdated notification because can't get blockchain top: all blocks on height " << topHeight << " are orphaned.";
        return;
      }

      knownBlockchainTop = topMainchainBlock;

      observerManager.notify(&IBlockchainObserver::blockchainUpdated, newBlocks, orphanedBlocks);
      logger(DEBUGGING) << "localBlockchainUpdated notification was successfully sent.";
    }
  );
}

}
