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

#include "gtest/gtest.h"

#include <system_error>
#include <thread>

#include <boost/range/combine.hpp>

#include "EventWaiter.h"
#include "ICoreStub.h"
#include "ICryptoNoteProtocolQueryStub.h"
#include "INodeStubs.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "DataBaseMock.h"
#include "TestBlockchainGenerator.h"

#include "CryptoNoteCore/CryptoNoteTools.h"

#include "Logging/FileLogger.h"

#include "BlockchainExplorer/BlockchainExplorer.h"

using namespace Crypto;
using namespace CryptoNote;

namespace {
Transaction createTx(ITransactionReader& tx) {
  Transaction outTx;
  fromBinaryArray(outTx, tx.getTransactionData());
  return outTx;
}

struct CallbackStatus {
  CallbackStatus() {}

  bool wait() { return waiter.wait_for(std::chrono::milliseconds(3000)); }
  bool ok() { return waiter.wait_for(std::chrono::milliseconds(3000)) && !static_cast<bool>(code); }
  void setStatus(const std::error_code& ec) { code = ec; waiter.notify(); }
  std::error_code getStatus() const { return code; }

  std::error_code code;
  EventWaiter waiter;
};

class dummyObserver : public IBlockchainObserver {
public:
  virtual ~dummyObserver() {}
};

class smartObserver : public IBlockchainObserver {
public:
  virtual ~smartObserver() {}

  virtual void blockchainUpdated(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks) override {
    blockchainUpdatedCallback(newBlocks, orphanedBlocks);
  }

  virtual void poolUpdated(const std::vector<TransactionDetails>& newTransactions, const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions) override {
    poolUpdatedCallback(newTransactions, removedTransactions);
  }

  virtual void blockchainSynchronized(const BlockDetails& topBlock) override {
    blockchainSynchronizedCallback(topBlock);
  }

  void setCallback(const std::function<void(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks)>& cb) {
    blockchainUpdatedCallback = cb;
  }

  void setCallback(const std::function<void(const std::vector<TransactionDetails>& newTransactions, const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions)>& cb) {
    poolUpdatedCallback = cb;
  }

  void setCallback(std::function<void(const BlockDetails& topBlock)> cb) {
    blockchainSynchronizedCallback = std::move(cb);
  }

private:
  std::function<void(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks)> blockchainUpdatedCallback;
  std::function<void(const std::vector<TransactionDetails>& newTransactions, const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions)> poolUpdatedCallback;
  std::function<void(const BlockDetails& topBlock)> blockchainSynchronizedCallback;
};

class BlockchainExplorerTests : public ::testing::Test {
public:
  BlockchainExplorerTests() :
    currency(CurrencyBuilder(logger).currency()),
    generator(currency),
    nodeStub(generator),
    blockchainExplorer(nodeStub, logger) {
  }
  void SetUp() override;
  void TearDown() override;

protected:
  Currency currency;
  TestBlockchainGenerator generator;
  INodeTrivialRefreshStub nodeStub;
  Logging::FileLogger logger;
  dummyObserver observer;
  DataBaseMock database;
  BlockchainExplorer blockchainExplorer;
};

void BlockchainExplorerTests::SetUp() {
  logger.init("/dev/null");
  ASSERT_NO_THROW(blockchainExplorer.init());
}

void BlockchainExplorerTests::TearDown() {
  ASSERT_NO_THROW(blockchainExplorer.shutdown());
}
}

TEST_F(BlockchainExplorerTests, initOk) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_NO_THROW(newExplorer.init());
}

TEST_F(BlockchainExplorerTests, shutdownOk) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_NO_THROW(newExplorer.init());
  ASSERT_NO_THROW(newExplorer.shutdown());
}

TEST_F(BlockchainExplorerTests, doubleInit) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_NO_THROW(newExplorer.init());
  ASSERT_ANY_THROW(newExplorer.init());
}

TEST_F(BlockchainExplorerTests, shutdownNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.shutdown());
}

TEST_F(BlockchainExplorerTests, addObserver) {
  ASSERT_TRUE(blockchainExplorer.addObserver(&observer));
}

TEST_F(BlockchainExplorerTests, addObserverNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.addObserver(&observer));
}

TEST_F(BlockchainExplorerTests, removeObserver) {
  ASSERT_TRUE(blockchainExplorer.addObserver(&observer));
  ASSERT_TRUE(blockchainExplorer.removeObserver(&observer));
}

TEST_F(BlockchainExplorerTests, removeObserverNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.addObserver(&observer));
  ASSERT_ANY_THROW(newExplorer.removeObserver(&observer));
}

TEST_F(BlockchainExplorerTests, removeObserverNotAdded) {
  ASSERT_FALSE(blockchainExplorer.removeObserver(&observer));
}

TEST_F(BlockchainExplorerTests, getBlocksByHeightGenesis) {
  std::vector<uint32_t> blockHeights;
  blockHeights.push_back(0);
  std::vector<std::vector<BlockDetails>> blocks;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHeights, blocks));
  ASSERT_EQ(blocks.size(), 1);
  EXPECT_EQ(blockHeights.size(), blocks.size());
  ASSERT_EQ(blocks.front().size(), 1);
  EXPECT_EQ(blocks.front().front().index, 0);
  EXPECT_FALSE(blocks.front().front().isAlternative);
}

TEST_F(BlockchainExplorerTests, getBlocksByHeightMany) {
  const uint32_t NUMBER_OF_BLOCKS = 10;
  std::vector<uint32_t> blockHeights;
  for (uint32_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    blockHeights.push_back(i);
  }
  std::vector<std::vector<BlockDetails>> blocks;

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_GE(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHeights, blocks));
  EXPECT_EQ(blocks.size(), NUMBER_OF_BLOCKS);
  ASSERT_EQ(blockHeights.size(), blocks.size());

  auto range = boost::combine(blockHeights, blocks);
  for (const boost::tuple<size_t, std::vector<BlockDetails>>& sameHeight : range) {
    EXPECT_EQ(sameHeight.get<1>().size(), 1);
    for (const BlockDetails& block : sameHeight.get<1>()) {
      EXPECT_EQ(block.index, sameHeight.get<0>());
      EXPECT_FALSE(block.isAlternative);
    }
  }
}

TEST_F(BlockchainExplorerTests, getBlocksByHeightFail) {
  const uint32_t NUMBER_OF_BLOCKS = 10;
  std::vector<uint32_t> blockHeights;
  for (uint32_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    blockHeights.push_back(i);
  }
  std::vector<std::vector<BlockDetails>> blocks;

  EXPECT_LT(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  ASSERT_ANY_THROW(blockchainExplorer.getBlocks(blockHeights, blocks));
}

TEST_F(BlockchainExplorerTests, getBlocksByHeightNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  std::vector<uint32_t> blockHeights;
  blockHeights.push_back(0);
  std::vector<std::vector<BlockDetails>> blocks;
  ASSERT_ANY_THROW(newExplorer.getBlocks(blockHeights, blocks));
}

TEST_F(BlockchainExplorerTests, getBlocksByHashGenesis) {
  std::vector<Hash> blockHashes;
  ASSERT_GE(generator.getBlockchain().size(), 1);

  auto genesisHash = CachedBlock(generator.getBlockchain().front()).getBlockHash();
  blockHashes.push_back(genesisHash);
  std::vector<BlockDetails> blocks;

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHashes, blocks));
  ASSERT_EQ(blocks.size(), 1);
  EXPECT_EQ(blockHashes.size(), blocks.size());

  Hash expectedHash = genesisHash;
  EXPECT_EQ(blocks.front().hash, expectedHash);
  EXPECT_EQ(blocks.front().hash, blockHashes.front());
  EXPECT_FALSE(blocks.front().isAlternative);
}

TEST_F(BlockchainExplorerTests, getBlocksByHashMany) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<Hash> blockHashes;

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_GE(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  for (const auto& block : generator.getBlockchain()) {
    if (blockHashes.size() == NUMBER_OF_BLOCKS) {
      break;
    }
    blockHashes.push_back(CachedBlock(block).getBlockHash());
  }

  std::vector<BlockDetails> blocks;

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHashes, blocks));
  EXPECT_EQ(blocks.size(), NUMBER_OF_BLOCKS);
  ASSERT_EQ(blockHashes.size(), blocks.size());

  auto range = boost::combine(blockHashes, blocks);
  for (const boost::tuple<Hash, BlockDetails>& hashWithBlock : range) {
    EXPECT_EQ(hashWithBlock.get<0>(), hashWithBlock.get<1>().hash);
    EXPECT_FALSE(hashWithBlock.get<1>().isAlternative);
  }
}

TEST_F(BlockchainExplorerTests, getBlocksByHashFail) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<Hash> blockHashes;

  for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    blockHashes.push_back(boost::value_initialized<Hash>());
  }

  std::vector<BlockDetails> blocks;

  EXPECT_LT(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);
  ASSERT_ANY_THROW(blockchainExplorer.getBlocks(blockHashes, blocks));

}

TEST_F(BlockchainExplorerTests, getBlocksByHashNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  std::vector<Hash> blockHashes;
  auto genesisHash = CachedBlock(generator.getBlockchain().front()).getBlockHash();
  blockHashes.push_back(genesisHash);
  std::vector<BlockDetails> blocks;
  ASSERT_ANY_THROW(newExplorer.getBlocks(blockHashes, blocks));
}

TEST_F(BlockchainExplorerTests, getBlockchainTop) {
  BlockDetails topBlock;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
  EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
  EXPECT_FALSE(topBlock.isAlternative);
}

TEST_F(BlockchainExplorerTests, getBlockchainTopNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  BlockDetails topBlock;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_ANY_THROW(newExplorer.getBlockchainTop(topBlock));
}

TEST_F(BlockchainExplorerTests, getTransactionFromBlockchain) {
  auto txptr = createTransaction();
  auto tx = ::createTx(*txptr.get());
  generator.addTxToBlockchain(tx);

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<Hash> transactionHashes;
  Hash hash = getObjectHash(tx);
  transactionHashes.push_back(hash);

  std::vector<TransactionDetails> transactions;

  ASSERT_TRUE(blockchainExplorer.getTransactions(transactionHashes, transactions));
  ASSERT_EQ(transactions.size(), 1);
  EXPECT_EQ(transactions.size(), transactionHashes.size());

  EXPECT_EQ(transactions.front().hash, transactionHashes.front());
  EXPECT_TRUE(transactions.front().inBlockchain);
}

TEST_F(BlockchainExplorerTests, getTransactionFromPool) {
  auto txptr = createTransaction();
  auto tx = ::createTx(*txptr.get());
  generator.putTxToPool(tx);

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<Hash> transactionHashes;
  Hash hash = getObjectHash(tx);
  transactionHashes.push_back(hash);

  std::vector<TransactionDetails> transactions;

  ASSERT_TRUE(blockchainExplorer.getTransactions(transactionHashes, transactions));
  ASSERT_EQ(transactions.size(), 1);
  EXPECT_EQ(transactions.size(), transactionHashes.size());

  EXPECT_EQ(transactions.front().hash, transactionHashes.front());
  EXPECT_FALSE(transactions.front().inBlockchain);
}

TEST_F(BlockchainExplorerTests, getTransactionsMany) {
  size_t POOL_TX_NUMBER = 10;
  size_t BLOCKCHAIN_TX_NUMBER = 10;
  std::vector<Hash> poolTxs;
  std::vector<Hash> blockchainTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    Hash hash = getObjectHash(tx);
    poolTxs.push_back(hash);
    generator.putTxToPool(tx);
  }

  for (size_t i = 0; i < BLOCKCHAIN_TX_NUMBER; ++i) {
    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    Hash hash = getObjectHash(tx);
    blockchainTxs.push_back(hash);
    generator.addTxToBlockchain(tx);
  }

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<Hash> transactionHashes;
  std::copy(poolTxs.begin(), poolTxs.end(), std::back_inserter(transactionHashes));
  std::copy(blockchainTxs.begin(), blockchainTxs.end(), std::back_inserter(transactionHashes));

  std::vector<TransactionDetails> transactions;

  ASSERT_TRUE(blockchainExplorer.getTransactions(transactionHashes, transactions));
  ASSERT_EQ(transactions.size(), POOL_TX_NUMBER + BLOCKCHAIN_TX_NUMBER);
  EXPECT_EQ(transactions.size(), transactionHashes.size());

  for (const Hash& poolTxHash : poolTxs) {
    auto iter = std::find_if(
      transactions.begin(),
      transactions.end(),
      [&poolTxHash](const TransactionDetails& txDetails) -> bool {
      return poolTxHash == txDetails.hash;
    }
    );
    EXPECT_NE(iter, transactions.end());
    EXPECT_EQ(iter->hash, poolTxHash);
    EXPECT_FALSE(iter->inBlockchain);
  }

  for (const Hash& blockchainTxHash : blockchainTxs) {
    auto iter = std::find_if(
      transactions.begin(),
      transactions.end(),
      [&blockchainTxHash](const TransactionDetails& txDetails) -> bool {
      return blockchainTxHash == txDetails.hash;
    }
    );
    EXPECT_NE(iter, transactions.end());
    EXPECT_EQ(iter->hash, blockchainTxHash);
    EXPECT_TRUE(iter->inBlockchain);
  }
}

TEST_F(BlockchainExplorerTests, getTransactionsFail) {
  size_t POOL_TX_NUMBER = 10;
  size_t BLOCKCHAIN_TX_NUMBER = 10;
  std::vector<Hash> poolTxs;
  std::vector<Hash> blockchainTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    Hash hash = getObjectHash(tx);
    poolTxs.push_back(hash);
    generator.putTxToPool(tx);
  }

  for (size_t i = 0; i < BLOCKCHAIN_TX_NUMBER; ++i) {
    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    Hash hash = getObjectHash(tx);
    blockchainTxs.push_back(hash);
    generator.addTxToBlockchain(tx);
  }

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<Hash> transactionHashes;
  transactionHashes.push_back(boost::value_initialized<Hash>());

  std::vector<TransactionDetails> transactions;

  ASSERT_ANY_THROW(blockchainExplorer.getTransactions(transactionHashes, transactions));
}

TEST_F(BlockchainExplorerTests, getTransactionsNotInited) {
  auto txptr = createTransaction();
  auto tx = ::createTx(*txptr.get());
  generator.addTxToBlockchain(tx);

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<Hash> transactionHashes;
  Hash hash = getObjectHash(tx);
  transactionHashes.push_back(hash);

  std::vector<TransactionDetails> transactions;

  BlockchainExplorer newExplorer(nodeStub, logger);

  ASSERT_ANY_THROW(newExplorer.getTransactions(transactionHashes, transactions));
}

TEST_F(BlockchainExplorerTests, getPoolStateEmpty) {
  BlockDetails topBlock;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
  EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
  EXPECT_FALSE(topBlock.isAlternative);

  std::vector<Hash> knownPoolTransactionHashes;
  Hash knownBlockchainTop = topBlock.hash;
  bool isBlockchainActual;

  std::vector<TransactionDetails> newTransactions;
  std::vector<Hash> removedTransactions;

  ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
  EXPECT_TRUE(isBlockchainActual);

  EXPECT_EQ(newTransactions.size(), 0);
  EXPECT_EQ(removedTransactions.size(), 0);
}

TEST_F(BlockchainExplorerTests, getPoolStateMany) {
  size_t POOL_TX_NUMBER = 10;
  std::vector<Hash> poolTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    Hash hash = getObjectHash(tx);
    poolTxs.push_back(hash);
    generator.putTxToPool(tx);
  }

  {
    BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isAlternative);

    std::vector<Hash> knownPoolTransactionHashes;
    Hash knownBlockchainTop = topBlock.hash;
    bool isBlockchainActual;

    std::vector<TransactionDetails> newTransactions;
    std::vector<Hash> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_TRUE(isBlockchainActual);

    EXPECT_EQ(newTransactions.size(), POOL_TX_NUMBER);
    EXPECT_EQ(removedTransactions.size(), 0);

    for (const Hash& poolTxHash : poolTxs) {
      auto iter = std::find_if(
        newTransactions.begin(),
        newTransactions.end(),
        [&poolTxHash](const TransactionDetails& txDetails) -> bool {
        return poolTxHash == txDetails.hash;
      }
      );
      EXPECT_NE(iter, newTransactions.end());
      EXPECT_EQ(iter->hash, poolTxHash);
      EXPECT_FALSE(iter->inBlockchain);
    }
  }

  generator.putTxPoolToBlockchain();

  {
    BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isAlternative);

    std::vector<Hash> knownPoolTransactionHashes;
    Hash knownBlockchainTop = topBlock.hash;
    bool isBlockchainActual;

    std::vector<TransactionDetails> newTransactions;
    std::vector<Hash> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_TRUE(isBlockchainActual);

    EXPECT_EQ(newTransactions.size(), 0);
    EXPECT_EQ(removedTransactions.size(), 0);
  }

  {
    BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isAlternative);

    std::vector<Hash> knownPoolTransactionHashes = poolTxs;
    Hash knownBlockchainTop = topBlock.hash;
    bool isBlockchainActual;

    std::vector<TransactionDetails> newTransactions;
    std::vector<Hash> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_TRUE(isBlockchainActual);

    EXPECT_EQ(newTransactions.size(), 0);
    EXPECT_EQ(removedTransactions.size(), POOL_TX_NUMBER);

    for (const Hash& poolTxHash : knownPoolTransactionHashes) {
      auto iter = std::find(
        removedTransactions.begin(),
        removedTransactions.end(),
        poolTxHash
        );
      EXPECT_NE(iter, removedTransactions.end());
      EXPECT_EQ(*iter, poolTxHash);
    }
  }

  auto txptr = createTransaction();
  auto tx = ::createTx(*txptr.get());
  Hash newTxHash = getObjectHash(tx);
  generator.putTxToPool(tx);

  {
    BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isAlternative);

    std::vector<Hash> knownPoolTransactionHashes = poolTxs;
    Hash knownBlockchainTop = topBlock.hash;
    bool isBlockchainActual;

    std::vector<TransactionDetails> newTransactions;
    std::vector<Hash> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_TRUE(isBlockchainActual);

    ASSERT_EQ(newTransactions.size(), 1);
    EXPECT_EQ(newTransactions.front().hash, newTxHash);
    EXPECT_EQ(removedTransactions.size(), POOL_TX_NUMBER);

    for (const Hash& poolTxHash : knownPoolTransactionHashes) {
      auto iter = std::find(
        removedTransactions.begin(),
        removedTransactions.end(),
        poolTxHash
        );
      EXPECT_NE(iter, removedTransactions.end());
      EXPECT_EQ(*iter, poolTxHash);
    }
  }

  {
    BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    std::vector<Hash> knownPoolTransactionHashes;
    Hash knownBlockchainTop = boost::value_initialized<Hash>();
    bool isBlockchainActual;

    std::vector<TransactionDetails> newTransactions;
    std::vector<Hash> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_FALSE(isBlockchainActual);
  }
}

TEST_F(BlockchainExplorerTests, getPoolStateNotInited) {

  std::vector<Hash> knownPoolTransactionHashes;
  Hash knownBlockchainTop = boost::value_initialized<Hash>();
  bool isBlockchainActual;

  std::vector<TransactionDetails> newTransactions;
  std::vector<Hash> removedTransactions;

  BlockchainExplorer newExplorer(nodeStub, logger);

  ASSERT_ANY_THROW(newExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
}

TEST_F(BlockchainExplorerTests, getRewardBlocksWindow) {
  ASSERT_EQ(blockchainExplorer.getRewardBlocksWindow(), parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW);
}

TEST_F(BlockchainExplorerTests, getRewardBlocksWindowNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.getRewardBlocksWindow());
}

TEST_F(BlockchainExplorerTests, getFullRewardMaxBlockSize) {
  ASSERT_EQ(blockchainExplorer.getFullRewardMaxBlockSize(1), parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1);
  ASSERT_EQ(blockchainExplorer.getFullRewardMaxBlockSize(2), parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2);
  ASSERT_EQ(blockchainExplorer.getFullRewardMaxBlockSize(3), parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
}

TEST_F(BlockchainExplorerTests, getFullRewardMaxBlockSizeNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.getFullRewardMaxBlockSize(1));
}

TEST_F(BlockchainExplorerTests, isSynchronizedFalse) {
  ASSERT_FALSE(blockchainExplorer.isSynchronized());
}

TEST_F(BlockchainExplorerTests, isSynchronizedNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.isSynchronized());
}

TEST_F(BlockchainExplorerTests, isSynchronizedNotification) {
  smartObserver observer;
  CallbackStatus status;

  std::function<void(const BlockDetails& topBlock)> cb = [&status, this](const BlockDetails& topBlock) {
    EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
    status.setStatus(std::error_code());
  };
  observer.setCallback(cb);
  blockchainExplorer.addObserver(&observer);

  nodeStub.setSynchronizedStatus(true);
  ASSERT_TRUE(blockchainExplorer.isSynchronized());
  ASSERT_TRUE(status.wait());
}

TEST_F(BlockchainExplorerTests, blockchainUpdatedEmpty) {
  smartObserver observer;
  CallbackStatus status;

  observer.setCallback(
    static_cast<std::function<void(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks)>>(
      [&status, this](const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks) {
        EXPECT_EQ(newBlocks.size(), 0);
        EXPECT_EQ(orphanedBlocks.size(), 0);
        status.setStatus(std::error_code());
      }));
  blockchainExplorer.addObserver(&observer);

  nodeStub.sendLocalBlockchainUpdated();
  ASSERT_FALSE(status.wait()); // observer is not called cause blockchain height didn't change
}

TEST_F(BlockchainExplorerTests, blockchainUpdatedMany) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<Hash> blockHashes;

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_GE(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  for (auto iter = generator.getBlockchain().begin() + 2; iter != generator.getBlockchain().end(); iter++) {
    if (blockHashes.size() == NUMBER_OF_BLOCKS) {
      break;
    }
    auto hash = CachedBlock(*iter).getBlockHash();
    blockHashes.push_back(hash);
  }

  smartObserver observer;
  CallbackStatus status;

  std::function<void(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks)> cb = 
    [&status, &blockHashes, this, NUMBER_OF_BLOCKS](const std::vector<BlockDetails>& newBlocks,
                                                            const std::vector<BlockDetails>& orphanedBlocks) {
    EXPECT_EQ(newBlocks.size(), NUMBER_OF_BLOCKS);
    EXPECT_EQ(orphanedBlocks.size(), 0);

    auto range = boost::combine(blockHashes, newBlocks);
    for (const boost::tuple<Hash, BlockDetails>& hashWithBlock : range) {
      EXPECT_EQ(hashWithBlock.get<0>(), hashWithBlock.get<1>().hash);
      EXPECT_FALSE(hashWithBlock.get<1>().isAlternative);
    }

    status.setStatus(std::error_code());
  };
  observer.setCallback(std::move(cb));
  blockchainExplorer.addObserver(&observer);

  nodeStub.sendLocalBlockchainUpdated();
  ASSERT_TRUE(status.wait());
}

TEST_F(BlockchainExplorerTests, poolUpdatedEmpty) {
  smartObserver observer;
  CallbackStatus status;

  std::function<void(const std::vector<TransactionDetails>& newTransactions, 
      const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions)> cb = 
    [&status, this](const std::vector<TransactionDetails>& newTransactions,
                            const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions) {
    EXPECT_EQ(newTransactions.size(), 0);
    EXPECT_EQ(removedTransactions.size(), 0);
    status.setStatus(std::error_code());
  };
  observer.setCallback(cb);
  blockchainExplorer.addObserver(&observer);

  nodeStub.sendPoolChanged();
  ASSERT_FALSE(status.wait());
}

TEST_F(BlockchainExplorerTests, poolUpdatedMany) {
  size_t POOL_TX_NUMBER = 10;
  std::vector<Hash> poolTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    Hash hash = getObjectHash(tx);
    poolTxs.push_back(hash);
    generator.putTxToPool(tx);
  }
  nodeStub.setSynchronizedStatus(true);
  {
    BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isAlternative);

    smartObserver observer;
    CallbackStatus status;

    std::function<void(const std::vector<TransactionDetails>& newTransactions, 
      const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions)> cb =
        [&status, &poolTxs, this, POOL_TX_NUMBER](const std::vector<TransactionDetails>& newTransactions,
        const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions) {
      EXPECT_EQ(newTransactions.size(), POOL_TX_NUMBER);
      EXPECT_EQ(removedTransactions.size(), 0);

      for (const Hash& poolTxHash : poolTxs) {
        auto iter = std::find_if(
            newTransactions.begin(), newTransactions.end(),
            [&poolTxHash](const TransactionDetails& txDetails) -> bool { return poolTxHash == txDetails.hash; });
        EXPECT_NE(iter, newTransactions.end());
        EXPECT_EQ(iter->hash, poolTxHash);
        EXPECT_FALSE(iter->inBlockchain);
      }
      status.setStatus(std::error_code());
    };
    observer.setCallback(cb);

    std::function<
      void(const std::vector<BlockDetails>& newBlocks,
      const std::vector<BlockDetails>& orphanedBlocks)
    > cb1 = [&status, this](const std::vector<BlockDetails>& newBlocks,
    const std::vector<BlockDetails>& orphanedBlocks) {};
    observer.setCallback(cb1);

    nodeStub.sendLocalBlockchainUpdated();

    blockchainExplorer.addObserver(&observer);

    nodeStub.sendPoolChanged();
    ASSERT_TRUE(status.wait());
    blockchainExplorer.removeObserver(&observer);
  }

  generator.putTxPoolToBlockchain();

  {
    BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isAlternative);

    smartObserver observer;
    CallbackStatus status;
    CallbackStatus status1;

    observer.setCallback(
      static_cast<std::function<void(const std::vector<TransactionDetails>& newTransactions, 
        const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions)>>(
      [&](const std::vector<TransactionDetails>& newTransactions,
                             const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions) {
      EXPECT_EQ(newTransactions.size(), 1);
      EXPECT_EQ(removedTransactions.size(), POOL_TX_NUMBER);

      for (const Hash& poolTxHash : poolTxs) {
        auto iter = std::find_if(removedTransactions.begin(), removedTransactions.end(),
                                 [&poolTxHash](const std::pair<Hash, TransactionRemoveReason>& txDetails) -> bool {
                                   return poolTxHash == txDetails.first;
                                 });
        EXPECT_NE(iter, removedTransactions.end());
        EXPECT_EQ(iter->first, poolTxHash);
        EXPECT_EQ(iter->second, TransactionRemoveReason::INCLUDED_IN_BLOCK);
      }
      status.setStatus(std::error_code());
    }));

    observer.setCallback(
      static_cast<std::function<void(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks)>>(
      [&](const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks) {
        status1.setStatus(std::error_code());
      }));

    blockchainExplorer.addObserver(&observer);

    generator.generateEmptyBlocks(1);
    nodeStub.sendPoolChanged();
    nodeStub.sendLocalBlockchainUpdated();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_TRUE(status1.wait());

    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    generator.putTxToPool(tx);

    nodeStub.sendPoolChanged();
    ASSERT_TRUE(status.wait());
    blockchainExplorer.removeObserver(&observer);
  }
}

TEST_F(BlockchainExplorerTests, poolUpdatedManyNotSynchronized) {
  size_t POOL_TX_NUMBER = 10;
  std::vector<Hash> poolTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    Hash hash = getObjectHash(tx);
    poolTxs.push_back(hash);
    generator.putTxToPool(tx);
  }
  nodeStub.setSynchronizedStatus(false);

  BlockDetails topBlock;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
  EXPECT_EQ(topBlock.index, generator.getBlockchain().size() - 1);
  EXPECT_FALSE(topBlock.isAlternative);

  smartObserver observer;
  CallbackStatus status;

  std::function<void(const std::vector<TransactionDetails>& newTransactions, 
      const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions)> cb = 
    [&status, &poolTxs, this, POOL_TX_NUMBER](const std::vector<TransactionDetails>& newTransactions,
      const std::vector<std::pair<Hash, TransactionRemoveReason>>& removedTransactions) {
    EXPECT_EQ(newTransactions.size(), POOL_TX_NUMBER);
    EXPECT_EQ(removedTransactions.size(), 0);

    for (const Hash& poolTxHash : poolTxs) {
      auto iter = std::find_if(
        newTransactions.begin(),
        newTransactions.end(),
        [&poolTxHash](const TransactionDetails& txDetails) -> bool {
        return poolTxHash == txDetails.hash;
      }
      );
      EXPECT_NE(iter, newTransactions.end());
      EXPECT_EQ(iter->hash, poolTxHash);
      EXPECT_FALSE(iter->inBlockchain);
    }
    status.setStatus(std::error_code());
  };
  observer.setCallback(cb);

  std::function<void(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks)> cb1 = 
    [&status, this](const std::vector<BlockDetails>& newBlocks,
    const std::vector<BlockDetails>& orphanedBlocks) {};
  observer.setCallback(cb1);

  nodeStub.sendLocalBlockchainUpdated();

  blockchainExplorer.addObserver(&observer);

  nodeStub.sendPoolChanged();
  ASSERT_FALSE(status.wait());
  blockchainExplorer.removeObserver(&observer);

}

TEST_F(BlockchainExplorerTests, unexpectedTermination) {
  smartObserver observer;

  std::function<void(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks)> cb = 
    [this](const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks) {
    EXPECT_EQ(newBlocks.size(), 0);
    EXPECT_EQ(orphanedBlocks.size(), 0);
  };
  observer.setCallback(cb);
  blockchainExplorer.addObserver(&observer);

  for (uint8_t i = 0; i < 100; ++i)
    nodeStub.sendLocalBlockchainUpdated();

  blockchainExplorer.removeObserver(&observer);
}

TEST_F(BlockchainExplorerTests, unexpectedExeption) {
  smartObserver observer;
  CallbackStatus status;

  std::function<void(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks)> cb = 
    [&status, this](const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks) {
    EXPECT_EQ(newBlocks.size(), 1);
    EXPECT_EQ(orphanedBlocks.size(), 0);
    status.setStatus(std::error_code());
    throw std::system_error(std::error_code());
  };
  observer.setCallback(cb);
  blockchainExplorer.addObserver(&observer);

  generator.generateEmptyBlocks(1); // update height
  nodeStub.sendLocalBlockchainUpdated();
  ASSERT_TRUE(status.wait());
}

class GetBlocksByTimestampsNode: public INodeTrivialRefreshStub {
public:
  GetBlocksByTimestampsNode(TestBlockchainGenerator& generator, bool consumerTests = false) : INodeTrivialRefreshStub(generator, consumerTests) {}

  virtual void getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount, std::vector<Crypto::Hash>& blockHashes, const Callback& callback) override {
    auto begin = timestampIndex.lower_bound(timestampBegin);
    auto end = timestampIndex.upper_bound(timestampBegin + secondsCount);
    for (auto it = begin; it != end; ++it) {
      blockHashes.insert(blockHashes.end(), it->second.cbegin(), it->second.cend());
    }

    callback(std::error_code());
  }

  void setBlockTimestamp(const Crypto::Hash& blockHash, uint64_t timestamp) {
    timestampIndex[timestamp].push_back(blockHash);
  }

private:
  std::map<uint64_t, std::vector<Crypto::Hash>> timestampIndex;
};

TEST_F(BlockchainExplorerTests, getBlocksByTimestampGenesis) {
  ASSERT_GE(generator.getBlockchain().size(), 1);

  auto genesisHash = CachedBlock(generator.getBlockchain().front()).getBlockHash();

  GetBlocksByTimestampsNode node(generator);
  node.setBlockTimestamp(genesisHash, 0);

  BlockchainExplorer explorer(node, logger);
  explorer.init();

  std::vector<BlockDetails> blocks;

  uint32_t totalBlocksNumber;

  ASSERT_TRUE(explorer.getBlocks(0, 0, 1, blocks, totalBlocksNumber));
  ASSERT_EQ(blocks.size(), 1);
  EXPECT_EQ(totalBlocksNumber, 1);

  Hash expectedHash = genesisHash;
  EXPECT_EQ(blocks.front().hash, expectedHash);
  EXPECT_EQ(blocks.front().timestamp, 0);
  EXPECT_FALSE(blocks.front().isAlternative);
}

TEST_F(BlockchainExplorerTests, getBlocksByTimestampMany) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<Hash> blockHashes;

  uint64_t startTime = static_cast<uint64_t>(time(0) + currency.difficultyTarget() - 1);

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);

  GetBlocksByTimestampsNode node(generator);
  auto& blockchain = generator.getBlockchain();
  for (const auto& block: blockchain) {
    node.setBlockTimestamp(CachedBlock(block).getBlockHash(), block.timestamp);
  }

  BlockchainExplorer explorer(node, logger);
  explorer.init();

  node.sendLocalBlockchainUpdated();
  std::this_thread::sleep_for(std::chrono::milliseconds(200)); // dealing with async code is easy

  uint64_t endTime = startTime + currency.difficultyTarget() * NUMBER_OF_BLOCKS;

  ASSERT_EQ(generator.getBlockchain().size(), NUMBER_OF_BLOCKS + 2);

  for (auto iter = generator.getBlockchain().begin() + 2; iter != generator.getBlockchain().end(); iter++) {
    blockHashes.push_back(CachedBlock(*iter).getBlockHash());
  }

  std::vector<BlockDetails> blocks;

  uint32_t totalBlocksNumber;

  ASSERT_TRUE(explorer.getBlocks(startTime, endTime, NUMBER_OF_BLOCKS, blocks, totalBlocksNumber));
  EXPECT_EQ(blocks.size(), NUMBER_OF_BLOCKS);
  EXPECT_EQ(totalBlocksNumber, NUMBER_OF_BLOCKS);
  ASSERT_EQ(blockHashes.size(), blocks.size());

  auto range = boost::combine(blockHashes, blocks);
  for (const boost::tuple<Hash, BlockDetails>& hashWithBlock : range) {
    EXPECT_EQ(hashWithBlock.get<0>(), hashWithBlock.get<1>().hash);
    EXPECT_FALSE(hashWithBlock.get<1>().isAlternative);
  }
}

TEST_F(BlockchainExplorerTests, getBlocksByTimestampFail) {

  uint64_t startTime = currency.difficultyTarget() + 1;

  std::vector<BlockDetails> blocks;

  uint32_t totalBlocksNumber;

  EXPECT_EQ(generator.getBlockchain().size(), 2);
  ASSERT_ANY_THROW(blockchainExplorer.getBlocks(startTime, startTime + 5, 1, blocks, totalBlocksNumber));
  ASSERT_EQ(0, blocks.size());
}

TEST_F(BlockchainExplorerTests, getBlocksByTimestampNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);
  uint64_t startTime = static_cast<uint64_t>(time(NULL));
  std::vector<BlockDetails> blocks;
  uint32_t totalBlocksNumber;
  ASSERT_ANY_THROW(newExplorer.getBlocks(startTime, startTime, 1, blocks, totalBlocksNumber));
}

TEST_F(BlockchainExplorerTests, generatedTransactions) {
  const size_t NUMBER_OF_BLOCKS = 10;
  const size_t POOL_TX_NUMBER = 10;
  std::vector<uint32_t> blockHeights;
  for (uint32_t i = 0; i < NUMBER_OF_BLOCKS + 3; ++i) {
    blockHeights.push_back(i);
  }

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = createTransaction();
    auto tx = ::createTx(*txptr.get());
    generator.putTxToPool(tx);
  }

  std::vector<std::vector<BlockDetails>> blocks;

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  generator.putTxPoolToBlockchain();

  ASSERT_EQ(generator.getBlockchain().size(), NUMBER_OF_BLOCKS + 3);

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHeights, blocks));
  EXPECT_EQ(blocks.size(), NUMBER_OF_BLOCKS + 3);
  ASSERT_EQ(blockHeights.size(), blocks.size());

  auto range = boost::combine(blockHeights, blocks);
  for (const boost::tuple<size_t, std::vector<BlockDetails>>& sameHeight : range) {
    EXPECT_EQ(sameHeight.get<1>().size(), 1);
    for (const BlockDetails& block : sameHeight.get<1>()) {
      EXPECT_EQ(block.index, sameHeight.get<0>());
      EXPECT_FALSE(block.isAlternative);
      if (block.index != NUMBER_OF_BLOCKS + 2) {
        EXPECT_EQ(block.alreadyGeneratedTransactions, block.index + 1);
      } else {
        EXPECT_EQ(block.alreadyGeneratedTransactions, block.index + 1 + POOL_TX_NUMBER);
      }
    }
  }
}

class GetTransactionHashByPaymentIdNode: public INodeTrivialRefreshStub {
public:
  GetTransactionHashByPaymentIdNode(TestBlockchainGenerator& generator, bool consumerTests = false): INodeTrivialRefreshStub(generator, consumerTests) {
  }

  virtual void getTransactionHashesByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes, const Callback& callback) override {
    if (index.count(paymentId) == 0) {
      return;
    }

    transactionHashes = index.at(paymentId);

    callback(std::error_code());
  }

  void setTransactionPaymentId(const Crypto::Hash& paymentId, const Crypto::Hash& transactionHash) {
    index[paymentId].push_back(transactionHash);
  }

private:
  std::unordered_map<Crypto::Hash, std::vector<Crypto::Hash>> index;

};

TEST_F(BlockchainExplorerTests, getTransactionsByPaymentId) {
  size_t PAYMENT_ID_NUMBER = 3;
  size_t TX_PER_PAYMENT_ID = 5;
  std::unordered_map<Hash, Hash> txs;
  std::vector<Hash> paymentIds;

  GetTransactionHashByPaymentIdNode node(generator);
  BlockchainExplorer explorer(node, logger);
  explorer.init();

  for (size_t i = 0; i < PAYMENT_ID_NUMBER; ++i) {
    Hash randomPaymentId;
    for (uint8_t& j : randomPaymentId.data) {
      j = rand();
    }
    paymentIds.push_back(randomPaymentId);

    for (size_t j = 0; j < TX_PER_PAYMENT_ID; ++j) {
      auto txptr = createTransaction();

      txptr->setPaymentId(randomPaymentId);
      auto tx = ::createTx(*txptr.get());

      Hash hash = getObjectHash(tx);
      txs.emplace(hash, randomPaymentId);

      generator.addTxToBlockchain(tx);

      node.setTransactionPaymentId(randomPaymentId, hash);
    }
  }

  node.sendLocalBlockchainUpdated();
  std::this_thread::sleep_for(std::chrono::milliseconds(200)); // smart trick to deal with async code

  for (auto paymentId : paymentIds) {
    std::vector<TransactionDetails> transactions;

    ASSERT_TRUE(explorer.getTransactionsByPaymentId(paymentId, transactions));
    ASSERT_EQ(transactions.size(), TX_PER_PAYMENT_ID);

    for (auto transaction : transactions) {
      auto iter = txs.find(transaction.hash);
      ASSERT_NE(iter, txs.end());
      EXPECT_EQ(iter->second, paymentId);
      EXPECT_EQ(iter->second, transaction.paymentId);
    }
  }
}

TEST_F(BlockchainExplorerTests, getTransactionsByPaymentIdFail) {
  std::vector<TransactionDetails> transactions;

  Hash randomPaymentId;
  std::generate(std::begin(randomPaymentId.data), std::end(randomPaymentId.data), [] { return rand(); });

  EXPECT_EQ(generator.getBlockchain().size(), 2);
  ASSERT_FALSE(blockchainExplorer.getTransactionsByPaymentId(randomPaymentId, transactions));
}

TEST_F(BlockchainExplorerTests, getTransactionsByPaymentIdNotInited) {
  BlockchainExplorer newExplorer(nodeStub, logger);

  Hash randomPaymentId;
  for (uint8_t& i : randomPaymentId.data) {
    i = rand();
  }

  std::vector<TransactionDetails> transactions;

  EXPECT_EQ(generator.getBlockchain().size(), 2);
  ASSERT_ANY_THROW(newExplorer.getTransactionsByPaymentId(randomPaymentId, transactions));
}
