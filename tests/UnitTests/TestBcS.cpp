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

#include "Transfers/BlockchainSynchronizer.h"
#include "Transfers/TransfersConsumer.h"

#include "crypto/hash.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "Logging/ConsoleLogger.h"

#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"
#include "EventWaiter.h"

using namespace Crypto;
using namespace CryptoNote;


namespace {
Transaction createTx(ITransactionReader& tx) {
  Transaction outTx;
  fromBinaryArray(outTx, tx.getTransactionData());

  return outTx;
}
}

class INodeNonTrivialRefreshStub : public INodeTrivialRefreshStub {
public:

  INodeNonTrivialRefreshStub(TestBlockchainGenerator& generator) : INodeTrivialRefreshStub(generator), blocksWasQueried(false), poolWasQueried(false) {}

  virtual void queryBlocks(std::vector<Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const Callback& callback) override {
    blocksWasQueried = true;
    INodeTrivialRefreshStub::queryBlocks(std::move(knownBlockIds), timestamp, newBlocks, startHeight, callback);
  }

  virtual void getPoolSymmetricDifference(std::vector<Hash>&& known_pool_tx_ids, Hash known_block_id, bool& is_bc_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted_tx_ids, const Callback& callback) override {
    poolWasQueried = true;
    INodeTrivialRefreshStub::getPoolSymmetricDifference(std::move(known_pool_tx_ids), known_block_id, is_bc_actual, new_txs, deleted_tx_ids, callback);
  }

  void notifyAboutPool() {
    observerManager.notify(&INodeObserver::poolChanged);
  }

  bool blocksWasQueried;
  bool poolWasQueried;

};

class INodeFunctorialStub : public INodeNonTrivialRefreshStub {
public:

  INodeFunctorialStub(TestBlockchainGenerator& generator)
    : INodeNonTrivialRefreshStub(generator)
    , queryBlocksFunctor([](const std::vector<Hash>&, uint64_t, std::vector<BlockShortEntry>&, uint32_t&, const Callback&) -> bool { return true; })
    , getPoolSymmetricDifferenceFunctor([](const std::vector<Hash>&, Hash, bool&, std::vector<std::unique_ptr<ITransactionReader>>&, std::vector<Hash>&, const Callback&)->bool {return true; }) {
  }

  virtual void queryBlocks(std::vector<Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks,
    uint32_t& startHeight, const Callback& callback) override {
    if (queryBlocksFunctor(knownBlockIds, timestamp, newBlocks, startHeight, callback)) {
      INodeNonTrivialRefreshStub::queryBlocks(std::move(knownBlockIds), timestamp, newBlocks, startHeight, callback);
    }
  }

  virtual void getPoolSymmetricDifference(std::vector<Hash>&& known_pool_tx_ids, Hash known_block_id, bool& is_bc_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted_tx_ids, const Callback& callback) override {
    if (getPoolSymmetricDifferenceFunctor(known_pool_tx_ids, known_block_id, is_bc_actual, new_txs, deleted_tx_ids, callback)) {
      INodeNonTrivialRefreshStub::getPoolSymmetricDifference(std::move(known_pool_tx_ids), known_block_id, is_bc_actual, new_txs, deleted_tx_ids, callback);
    }
  }

  std::function<bool(const std::vector<Hash>&, uint64_t, std::vector<BlockShortEntry>&, uint32_t&, const Callback&)> queryBlocksFunctor;
  std::function<bool(const std::vector<Hash>&, Hash, bool&, std::vector<std::unique_ptr<ITransactionReader>>&, std::vector<Hash>&, const Callback&)> getPoolSymmetricDifferenceFunctor;
};

class IBlockchainSynchronizerTrivialObserver : public IBlockchainSynchronizerObserver {
public:
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override { m_current = current; m_total = total; }
  virtual void synchronizationCompleted(std::error_code result) override { completionResult = result; }

  std::error_code completionResult;
  uint32_t m_current;
  uint32_t m_total;
};

class IBlockchainSynchronizerFunctorialObserver : public IBlockchainSynchronizerObserver {
public:
  IBlockchainSynchronizerFunctorialObserver() : updFunc([](uint32_t, uint32_t) {}), syncFunc([](std::error_code) {}) {
  }

  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override { updFunc(current, total); }
  virtual void synchronizationCompleted(std::error_code result) override { syncFunc(result); }

  std::function<void(uint32_t, uint32_t)> updFunc;
  std::function<void(std::error_code)> syncFunc;
};

class ConsumerStub : public IBlockchainConsumer {
public:
  ConsumerStub(const Hash& genesisBlockHash) {
    m_blockchain.push_back(genesisBlockHash);
  }

  void addPoolTransaction(const Crypto::Hash& hash) {
    m_pool.emplace(hash);
  }

  virtual SynchronizationStart getSyncStart() override {
    SynchronizationStart start = { 0, 0 };
    return start;
  }

  virtual void addObserver(IBlockchainConsumerObserver* observer) override {
  }

  virtual void removeObserver(IBlockchainConsumerObserver* observer) override {
  }

  virtual void onBlockchainDetach(uint32_t height) override {
    assert(height < m_blockchain.size());
    m_blockchain.resize(height);
  }

  virtual bool onNewBlocks(const CompleteBlock* blocks, uint32_t startHeight, uint32_t count) override {
    //assert(m_blockchain.size() == startHeight);
    while (count--) {
      m_blockchain.push_back(blocks->blockHash);
      ++blocks;
    }
    return true;
  }

  const std::vector<Hash>& getBlockchain() const {
    return m_blockchain;
  }

  virtual const std::unordered_set<Crypto::Hash>& getKnownPoolTxIds() const override {
    return m_pool;
  }

  virtual std::error_code onPoolUpdated(const std::vector<std::unique_ptr<ITransactionReader>>& addedTransactions, const std::vector<Hash>& deletedTransactions) override {
    for (const auto& tx: addedTransactions) {
      m_pool.emplace(tx->getTransactionHash());
    }

    for (auto& hash : deletedTransactions) {
      m_pool.erase(hash);
    }

    return std::error_code();
  }

  std::error_code addUnconfirmedTransaction(const ITransactionReader& /*transaction*/) override {
    throw std::runtime_error("Not implemented");
  }

  void removeUnconfirmedTransaction(const Crypto::Hash& /*transactionHash*/) override {
    throw std::runtime_error("Not implemented");
  }

private:
  std::unordered_set<Crypto::Hash> m_pool;
  std::vector<Hash> m_blockchain;
};

class BcSTest : public ::testing::Test, public IBlockchainSynchronizerObserver {
public:
  BcSTest() :
    m_logger(Logging::ERROR),
    m_currency(CurrencyBuilder(m_logger).currency()),
    generator(m_currency),
    m_node(generator),
    m_sync(m_node, m_logger, m_currency.genesisBlockHash()) {
    m_node.setGetNewBlocksLimit(5); // sync max 5 blocks per request
  }

  void addConsumers(size_t count = 1) {
    while (count--) {
      std::shared_ptr<ConsumerStub> stub(new ConsumerStub(m_currency.genesisBlockHash()));
      m_sync.addConsumer(stub.get());
      m_consumers.push_back(stub);
    }
  }

  void checkSyncedBlockchains() {
    std::vector<Hash> generatorBlockchain;
    std::transform(
      generator.getBlockchain().begin(),
      generator.getBlockchain().end(),
      std::back_inserter(generatorBlockchain),
      [](const BlockTemplate& b) { return CachedBlock(b).getBlockHash(); });

    for (const auto& consumer : m_consumers) {
      ASSERT_EQ(consumer->getBlockchain(), generatorBlockchain);
    }
  }

  void startSync() {
    syncCompleted = std::promise<std::error_code>();
    syncCompletedFuture = syncCompleted.get_future();
    m_sync.addObserver(this);
    m_sync.start();
    syncCompletedFuture.get();
    m_sync.removeObserver(this);
  }

  void refreshSync() {
    syncCompleted = std::promise<std::error_code>();
    syncCompletedFuture = syncCompleted.get_future();
    m_sync.addObserver(this);
    m_node.updateObservers();
    syncCompletedFuture.get();
    m_sync.removeObserver(this);
  }

  void synchronizationCompleted(std::error_code result) override {
    decltype(syncCompleted) detachedPromise = std::move(syncCompleted);
    detachedPromise.set_value(result);
  }

protected:
  Logging::ConsoleLogger m_logger;
  Currency m_currency;
  TestBlockchainGenerator generator;

  INodeFunctorialStub m_node;
  BlockchainSynchronizer m_sync;
  std::vector<std::shared_ptr<ConsumerStub>> m_consumers;

  std::promise<std::error_code> syncCompleted;
  std::future<std::error_code> syncCompletedFuture;
};

TEST_F(BcSTest, addConsumerStopped) {
  ASSERT_NO_THROW(addConsumers());
}

TEST_F(BcSTest, addConsumerStartStop) {
  addConsumers();
  m_sync.start();
  m_sync.stop();
  ASSERT_NO_THROW(addConsumers());
}

TEST_F(BcSTest, addConsumerStartThrow) {
  addConsumers();
  m_sync.start();
  ASSERT_ANY_THROW(addConsumers());
  m_sync.stop();
}

TEST_F(BcSTest, removeConsumerWhichIsNotExist) {
  ConsumerStub c(m_currency.genesisBlockHash());
  ASSERT_FALSE(m_sync.removeConsumer(&c));
}

TEST_F(BcSTest, removeConsumerStartThrow) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  ASSERT_ANY_THROW(m_sync.removeConsumer(&c));
  m_sync.stop();
}

TEST_F(BcSTest, removeConsumerStopped) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  EXPECT_EQ(true, m_sync.removeConsumer(&c));
}

TEST_F(BcSTest, removeConsumerStartStop) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  m_sync.stop();
  EXPECT_EQ(true, m_sync.removeConsumer(&c));
}

TEST_F(BcSTest, getConsumerStateWhichIsNotExist) {
  ConsumerStub c(m_currency.genesisBlockHash());
  EXPECT_EQ(nullptr, m_sync.getConsumerState(&c));
}

TEST_F(BcSTest, getConsumerStateStartThrow) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  ASSERT_ANY_THROW(m_sync.getConsumerState(&c));
  m_sync.stop();
}

TEST_F(BcSTest, getConsumerStateStopped) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  EXPECT_NE(nullptr, m_sync.getConsumerState(&c));
}

TEST_F(BcSTest, getConsumerStateStartStop) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  m_sync.stop();
  EXPECT_NE(nullptr, m_sync.getConsumerState(&c));
}

TEST_F(BcSTest, startWithoutConsumersThrow) {
  ASSERT_ANY_THROW(m_sync.start());
}

TEST_F(BcSTest, doubleStart) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  ASSERT_ANY_THROW(m_sync.start());
  m_sync.stop();
}

TEST_F(BcSTest, startAfterStop) {
  addConsumers();
  m_sync.start();
  m_sync.stop();
  ASSERT_NO_THROW(m_sync.start());
  m_sync.stop();
}

TEST_F(BcSTest, startAndObserve) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  ASSERT_ANY_THROW(m_sync.start());
  m_sync.stop();
}

TEST_F(BcSTest, noObservationsBeforeStart) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_node.updateObservers();
  ASSERT_FALSE(m_node.blocksWasQueried);
}

TEST_F(BcSTest, noObservationsAfterStop) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  m_sync.stop();
  m_node.blocksWasQueried = false;
  m_node.updateObservers();
  ASSERT_FALSE(m_node.blocksWasQueried);
}

TEST_F(BcSTest, stopOnCreation) {
  ASSERT_NO_THROW(m_sync.stop());
}

TEST_F(BcSTest, doubleStopAfterStart) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  m_sync.stop();
  ASSERT_NO_THROW(m_sync.stop());
}

TEST_F(BcSTest, stopIsWaiting) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  generator.generateEmptyBlocks(20);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;

  bool flag = false;

  o1.updFunc = [&e, &flag](uint32_t, uint32_t) {
    e.notify(); std::this_thread::sleep_for(std::chrono::milliseconds(1000)); flag = true;

  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(flag, true);
}

TEST_F(BcSTest, syncCompletedError) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  generator.generateEmptyBlocks(20);
  IBlockchainSynchronizerTrivialObserver o;
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;

  o1.updFunc = [&e](uint32_t curr, uint32_t total) {
    e.notify(); std::this_thread::sleep_for(std::chrono::milliseconds(200));
  };

  m_sync.addObserver(&o);
  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(std::errc::interrupted, o.completionResult);
}

TEST_F(BcSTest, onLastKnownBlockHeightUpdated) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  generator.generateEmptyBlocks(20);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();

  e.wait();
  m_node.blocksWasQueried = false;
  m_node.poolWasQueried = false;
  m_node.updateObservers();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(true, m_node.blocksWasQueried);
  EXPECT_EQ(true, m_node.poolWasQueried);
}

TEST_F(BcSTest, onPoolChanged) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  generator.generateEmptyBlocks(20);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();

  e.wait();
  m_node.poolWasQueried = false;
  m_node.notifyAboutPool();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(true, m_node.poolWasQueried);
}

TEST_F(BcSTest, serializationCheck) {
  addConsumers(2);

  std::stringstream memstream;
  m_sync.save(memstream);

  ASSERT_GT(memstream.str().size(), 0);

  std::string first = memstream.str();

  BlockchainSynchronizer sync2(m_node, m_logger, m_currency.genesisBlockHash());

  ASSERT_NO_THROW(sync2.load(memstream));
  std::stringstream memstream2;
  m_sync.save(memstream2);
  EXPECT_EQ(memstream2.str(), first);
}

class FunctorialPoolConsumerStub : public ConsumerStub {
public:
  FunctorialPoolConsumerStub(const Hash& genesisBlockHash) : ConsumerStub(genesisBlockHash) {}

  virtual std::error_code onPoolUpdated(const std::vector<std::unique_ptr<ITransactionReader>>& addedTransactions, const std::vector<Hash>& deletedTransactions) override {
    return onPoolUpdatedFunctor(addedTransactions, deletedTransactions);
  }

  std::function<std::error_code(const std::vector<std::unique_ptr<ITransactionReader>>&, const std::vector<Hash>&)> onPoolUpdatedFunctor;
};

TEST_F(BcSTest, firstPoolSynchronizationCheck) {
  auto tx1ptr = createTransaction();
  auto tx2ptr = createTransaction();
  auto tx3ptr = createTransaction();

  auto tx1 = ::createTx(*tx1ptr.get());
  auto tx2 = ::createTx(*tx2ptr.get());
  auto tx3 = ::createTx(*tx3ptr.get());

  auto tx1hash = getObjectHash(tx1);
  auto tx2hash = getObjectHash(tx2);
  auto tx3hash = getObjectHash(tx3);

  std::unordered_set<Hash> firstExpectedPool = { tx1hash, tx2hash, tx3hash };
  std::unordered_set<Hash> secondExpectedPool = { tx2hash };

  std::vector<Hash> expectedDeletedPoolAnswer = { tx3hash };
  std::vector<Transaction> expectedNewPoolAnswer = { tx1 };
  std::vector<Hash> expectedNewPoolAnswerHashes = { tx1hash };

  FunctorialPoolConsumerStub c1(m_currency.genesisBlockHash());
  FunctorialPoolConsumerStub c2(m_currency.genesisBlockHash());

  c1.addPoolTransaction(tx1hash);
  c1.addPoolTransaction(tx2hash);

  c2.addPoolTransaction(tx2hash);
  c2.addPoolTransaction(tx3hash);

  std::vector<Hash> c1ResponseDeletedPool;
  std::vector<Hash> c2ResponseDeletedPool;
  std::vector<Hash> c1ResponseNewPool;
  std::vector<Hash> c2ResponseNewPool;

  c1.onPoolUpdatedFunctor = [&](const std::vector<std::unique_ptr<ITransactionReader>>& new_txs, const std::vector<Hash>& deleted)->std::error_code {
    c1ResponseDeletedPool.assign(deleted.begin(), deleted.end());
    for (const auto& tx: new_txs) {
      Hash hash = tx->getTransactionHash();
      c1ResponseNewPool.push_back(reinterpret_cast<const Hash&>(hash));
    }

    return std::error_code();
  };

  c2.onPoolUpdatedFunctor = [&](const std::vector<std::unique_ptr<ITransactionReader>>& new_txs, const std::vector<Hash>& deleted)->std::error_code {
    c2ResponseDeletedPool.assign(deleted.begin(), deleted.end());
    for (const auto& tx: new_txs) {
      Hash hash = tx->getTransactionHash();
      c2ResponseNewPool.push_back(reinterpret_cast<const Hash&>(hash));
    }

    return std::error_code();
  };

  m_sync.addConsumer(&c1);
  m_sync.addConsumer(&c2);

  int requestsCount = 0;
  std::unordered_set<Hash> firstKnownPool;
  std::unordered_set<Hash> secondKnownPool;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<Hash>& known, Hash last, bool& is_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 1) {
      // Ignore request to delete outdated transactions
      callback(std::error_code());
      return false;
    }

    new_txs.clear();
    for (const auto& tx : expectedNewPoolAnswer) {
      new_txs.push_back(createTransactionPrefix(tx));
    }

    deleted.assign(expectedDeletedPoolAnswer.begin(), expectedDeletedPoolAnswer.end());

    if (requestsCount == 2) {
      firstKnownPool.insert(known.begin(), known.end());
    }

    if (requestsCount == 3) {
      secondKnownPool.insert(known.begin(), known.end());
    }

    callback(std::error_code());

    return false;
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(3, requestsCount);
  EXPECT_EQ(firstExpectedPool, firstKnownPool);
  EXPECT_EQ(secondExpectedPool, secondKnownPool);
  EXPECT_EQ(expectedDeletedPoolAnswer, c1ResponseDeletedPool);
  EXPECT_EQ(expectedDeletedPoolAnswer, c2ResponseDeletedPool);
  EXPECT_EQ(expectedNewPoolAnswerHashes, c1ResponseNewPool);
  EXPECT_EQ(expectedNewPoolAnswerHashes, c2ResponseNewPool);
}

TEST_F(BcSTest, firstPoolSynchronizationCheckNonActual) {
  addConsumers(2);
  m_consumers.front()->addPoolTransaction(Crypto::rand<Crypto::Hash>());

  int requestsCount = 0;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<Hash>& known, Hash last, bool& is_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 2) {
      is_actual = false;
    }

    callback(std::error_code());
    return false;
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(4, requestsCount);
}

TEST_F(BcSTest, firstPoolSynchronizationCheckGetPoolErr) {
  addConsumers(2);
  m_consumers.front()->addPoolTransaction(Crypto::rand<Crypto::Hash>());

  int requestsCount = 0;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<Hash>& known, Hash last, bool& is_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 2) {
      callback(std::make_error_code(std::errc::invalid_argument));
    } else {
      callback(std::error_code());
    }

    return false;
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_node.notifyAboutPool();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(4, requestsCount);
}

TEST_F(BcSTest, poolSynchronizationCheckActual) {
  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();

  int requestsCount = 0;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<Hash>& known, Hash last, bool& is_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 1) {
      is_actual = false;
    }

    callback(std::error_code());
    return false;
  };

  m_node.notifyAboutPool();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(2, requestsCount);
}

TEST_F(BcSTest, poolSynchronizationCheckError) {
  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();

  int requestsCount = 0;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<Hash>& known, Hash last, bool& is_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 1) {
      callback(std::make_error_code(std::errc::invalid_argument));
    } else {
      callback(std::error_code());
    }
    return false;
  };

  m_node.notifyAboutPool();
  e.wait();
  EXPECT_NE(0, errc.value());
  m_node.notifyAboutPool(); //error, notify again
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(2, requestsCount);
}

TEST_F(BcSTest, poolSynchronizationCheckTxAdded) {
  auto tx1ptr = createTransaction();
  auto tx1 = ::createTx(*tx1ptr.get());
  auto tx1hash = getObjectHash(tx1);

  std::vector<Transaction> newPoolAnswer = { tx1 };
  std::vector<Hash> expectedKnownPoolHashes = { tx1hash };

  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();

  int requestsCount = 0;
  std::vector<Hash> knownPool;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<Hash>& known, Hash last, bool& is_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 1) {
      new_txs.clear();
      for (const auto& tx: newPoolAnswer) {
        new_txs.push_back(createTransactionPrefix(tx));
      }
    }

    if (requestsCount == 2) {
      knownPool.assign(known.begin(), known.end());
    }

    callback(std::error_code());

    return false;
  };

  m_node.notifyAboutPool();
  e.wait();
  m_node.notifyAboutPool();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(2, requestsCount);
  EXPECT_EQ(expectedKnownPoolHashes, knownPool);
}

TEST_F(BcSTest, poolSynchronizationCheckTxDeleted) {
  auto tx1ptr = createTransaction();
  auto tx1 = ::createTx(*tx1ptr.get());
  auto tx1hash = getObjectHash(tx1);

  std::vector<Transaction> newPoolAnswer = { tx1 };
  std::vector<Hash> deletedPoolAnswer = { tx1hash };
  std::vector<Hash> expectedKnownPoolHashes = {};


  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();

  int requestsCount = 0;
  std::vector<Hash> knownPool;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<Hash>& known, Hash last, bool& is_actual,
          std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;


    if (requestsCount == 1) {
      new_txs.clear();
      for (const auto& tx: newPoolAnswer) {
        new_txs.push_back(createTransactionPrefix(tx));
      }
    }

    if (requestsCount == 2) {
      deleted.assign(deletedPoolAnswer.begin(), deletedPoolAnswer.end());
    }

    if (requestsCount == 3) {
      knownPool.assign(known.begin(), known.end());
    }

    callback(std::error_code());

    return false;
  };

  m_node.notifyAboutPool(); // add
  e.wait();
  m_node.notifyAboutPool(); // delete
  e.wait();
  m_node.notifyAboutPool(); //getknown
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(3, requestsCount);
  EXPECT_EQ(expectedKnownPoolHashes, knownPool);
}

TEST_F(BcSTest, poolSynchronizationCheckNotification) {
  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  EXPECT_EQ(true, e.wait_for(std::chrono::milliseconds(1000)));
  m_sync.stop();
}

TEST_F(BcSTest, poolSynchronizationCheckConsumersNotififcation) {
  FunctorialPoolConsumerStub c1(m_currency.genesisBlockHash());
  FunctorialPoolConsumerStub c2(m_currency.genesisBlockHash());

  bool c1Notified = false;
  bool c2Notified = false;
  c1.onPoolUpdatedFunctor = [&](const std::vector<std::unique_ptr<ITransactionReader>>& new_txs, const std::vector<Hash>& deleted)->std::error_code {
    c1Notified = true;
    return std::error_code();
  };

  c2.onPoolUpdatedFunctor = [&](const std::vector<std::unique_ptr<ITransactionReader>>& new_txs, const std::vector<Hash>& deleted)->std::error_code {
    c2Notified = true;
    return std::error_code();
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = [&e](std::error_code) {
    e.notify();
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c1);
  m_sync.addConsumer(&c2);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  ASSERT_TRUE(c1Notified);
  ASSERT_TRUE(c2Notified);
}

TEST_F(BcSTest, poolSynchronizationCheckConsumerReturnError) {
  FunctorialPoolConsumerStub c1(m_currency.genesisBlockHash());
  FunctorialPoolConsumerStub c2(m_currency.genesisBlockHash());

  bool c1Notified = false;
  bool c2Notified = false;
  c1.onPoolUpdatedFunctor = [&](const std::vector<std::unique_ptr<ITransactionReader>>& new_txs, const std::vector<Hash>& deleted)->std::error_code {
    c1Notified = true;
    return std::make_error_code(std::errc::invalid_argument);
  };

  c2.onPoolUpdatedFunctor = [&](const std::vector<std::unique_ptr<ITransactionReader>>& new_txs, const std::vector<Hash>& deleted)->std::error_code {
    c2Notified = true;
    return std::make_error_code(std::errc::invalid_argument);
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c1);
  m_sync.addConsumer(&c2);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  ASSERT_TRUE(c1Notified != c2Notified);
  EXPECT_NE(0, errc.value());
}

class FunctorialBlockhainConsumerStub : public ConsumerStub {
public:

  FunctorialBlockhainConsumerStub(const Hash& genesisBlockHash) : ConsumerStub(genesisBlockHash), onBlockchainDetachFunctor([](uint32_t) {}) {}

  virtual bool onNewBlocks(const CompleteBlock* blocks, uint32_t startHeight, uint32_t count) override {
    return onNewBlocksFunctor(blocks, startHeight, count);
  }

  virtual void onBlockchainDetach(uint32_t height) override {
    onBlockchainDetachFunctor(height);
  }

  std::function<bool(const CompleteBlock*, uint32_t, size_t)> onNewBlocksFunctor;
  std::function<void(uint32_t)> onBlockchainDetachFunctor;
};

TEST_F(BcSTest, checkINodeError) {
  addConsumers(1);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };

  m_node.queryBlocksFunctor = [](const std::vector<Hash>& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const INode::Callback& callback) -> bool {
    callback(std::make_error_code(std::errc::invalid_argument));
    return false;
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), errc);
}

TEST_F(BcSTest, checkConsumerError) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    if (!errc) {
      errc = ec;
    }
  };

  generator.generateEmptyBlocks(10);

  c.onNewBlocksFunctor = [](const CompleteBlock*, uint32_t, size_t) -> bool {
    return false;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), errc);

  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};
}

TEST_F(BcSTest, checkBlocksRequesting) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };


  size_t blocksExpected = 20;

  generator.generateEmptyBlocks(blocksExpected - 1); //-1 for genesis
  m_node.setGetNewBlocksLimit(3);

  size_t blocksRequested = 0;

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint32_t, size_t count) -> bool {
    blocksRequested += count;
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(blocksExpected, blocksRequested);
}

TEST_F(BcSTest, checkConsumerHeightReceived) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };


  uint32_t firstlySnchronizedHeight = 20;

  generator.generateEmptyBlocks(static_cast<size_t>(firstlySnchronizedHeight - 1));//-1 for genesis
  m_node.setGetNewBlocksLimit(50);

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint32_t startHeight, size_t) -> bool {
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();

  generator.generateEmptyBlocks(20);

  ConsumerStub fake_c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&fake_c);
  uint32_t receivedStartHeight = 0;
  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint32_t startHeight, size_t) -> bool {
    receivedStartHeight = startHeight;
    return true;
  };

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(firstlySnchronizedHeight + 1, receivedStartHeight);
}

TEST_F(BcSTest, checkConsumerOldBlocksNotIvoked) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };

  generator.generateEmptyBlocks(20);
  m_node.setGetNewBlocksLimit(50);

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint32_t startHeight, size_t) -> bool {
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();

  ConsumerStub fake_c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&fake_c);

  bool onNewBlocksInvoked = false;

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint64_t startHeight, size_t) -> bool {
    onNewBlocksInvoked = true;
    return true;
  };

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  ASSERT_FALSE(onNewBlocksInvoked);
}

TEST_F(BcSTest, checkConsumerHeightReceivedOnDetach) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };

  generator.generateEmptyBlocks(20);
  m_node.setGetNewBlocksLimit(50);

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint32_t startHeight, size_t) -> bool {
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();

  uint32_t alternativeHeight = 10;

  m_node.startAlternativeChain(alternativeHeight);  
  generator.generateEmptyBlocks(20);

  uint32_t receivedStartHeight = 0;
  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint32_t startHeight, size_t) -> bool {
    receivedStartHeight = startHeight;
    return true;
  };

  uint32_t receivedetachHeight = 0;
  c.onBlockchainDetachFunctor = [&](uint32_t detachHeight) {
    receivedetachHeight = detachHeight;
  };

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(alternativeHeight, receivedetachHeight);
  EXPECT_EQ(alternativeHeight, receivedStartHeight);
}

TEST_F(BcSTest, checkStatePreservingBetweenSynchronizations) {
  addConsumers(1);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };

  generator.generateEmptyBlocks(20);

  Hash lastBlockHash = CachedBlock(generator.getBlockchain().back()).getBlockHash();

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();

  Hash receivedLastBlockHash;

  m_node.queryBlocksFunctor = [&receivedLastBlockHash](const std::vector<Hash>& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const INode::Callback& callback) -> bool {
    receivedLastBlockHash = knownBlockIds.front();
    startHeight = 1;
    callback(std::make_error_code(std::errc::interrupted));
    return false;
  };

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(lastBlockHash, receivedLastBlockHash);
}

TEST_F(BcSTest, checkBlocksRerequestingOnError) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  bool waitForFirstError = true;
  o1.syncFunc = [&](std::error_code ec) {
    if (ec && waitForFirstError) {
      waitForFirstError = false;
      e.notify();
    } else if (!ec && !waitForFirstError) {
      e.notify();
    }
  };

  generator.generateEmptyBlocks(20);
  m_node.setGetNewBlocksLimit(10);
  
  std::atomic<int> requestsCount(0);
  std::list<Hash> firstlyKnownBlockIdsTaken;
  std::list<Hash> secondlyKnownBlockIdsTaken;

  std::vector<Hash> firstlyReceivedBlocks;
  std::vector<Hash> secondlyReceivedBlocks;


  c.onNewBlocksFunctor = [&](const CompleteBlock* blocks, uint32_t, size_t count) -> bool {
    if (requestsCount == 2) {
      for (size_t i = 0; i < count; ++i) {
        firstlyReceivedBlocks.push_back(blocks[i].blockHash);
      }

      return false;
    }

    if (requestsCount == 3) {
      for (size_t i = 0; i < count; ++i) {
        secondlyReceivedBlocks.push_back(blocks[i].blockHash);
      }
    }

    return true;   
  };

  m_node.queryBlocksFunctor = [&](const std::vector<Hash>& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const INode::Callback& callback) -> bool {
    ++requestsCount;

    if (requestsCount == 2) {
      firstlyKnownBlockIdsTaken.assign(knownBlockIds.begin(), knownBlockIds.end());
    }

    if (requestsCount == 3) {
      secondlyKnownBlockIdsTaken.assign(knownBlockIds.begin(), knownBlockIds.end());
    }

    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(firstlyKnownBlockIdsTaken, secondlyKnownBlockIdsTaken);
  EXPECT_EQ(firstlyReceivedBlocks, secondlyReceivedBlocks);
}

TEST_F(BcSTest, checkTxOrder) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = [&](std::error_code ec) {
    e.notify();
    errc = ec;
  };

  auto tx1ptr = createTransaction();
  auto tx2ptr = createTransaction();
  auto tx3ptr = createTransaction();

  auto tx1 = ::createTx(*tx1ptr.get());
  auto tx2 = ::createTx(*tx2ptr.get());
  auto tx3 = ::createTx(*tx3ptr.get());

  auto tx1hash = getObjectHash(tx1);
  auto tx2hash = getObjectHash(tx2);
  auto tx3hash = getObjectHash(tx3);

  generator.generateEmptyBlocks(2);

  auto last_block = generator.getBlockchain().back();

  BlockShortEntry bse;
  bse.hasBlock = true;
  bse.blockHash = CachedBlock(last_block).getBlockHash();
  bse.block = last_block;
  bse.txsShortInfo.push_back({tx1hash, tx1});
  bse.txsShortInfo.push_back({tx2hash, tx2});
  bse.txsShortInfo.push_back({tx3hash, tx3});

  std::vector<Hash> expectedTxHashes = { getObjectHash(last_block.baseTransaction), tx1hash, tx2hash, tx3hash };

  int requestNumber = 0;

  m_node.queryBlocksFunctor = [&bse, &requestNumber](const std::vector<Hash>& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const INode::Callback& callback) -> bool {
    startHeight = 1;
    newBlocks.push_back(bse);
    if (requestNumber > 0) {
      callback(std::make_error_code(std::errc::interrupted));
    } else {
      callback(std::error_code());
    }

    requestNumber++;
    return false;
  };

  std::vector<Hash> receivedTxHashes = {};

  c.onNewBlocksFunctor = [&](const CompleteBlock* blocks, uint32_t, size_t count) -> bool {
    for (auto& tx : blocks[count - 1].transactions) {
      auto hash = tx->getTransactionHash();
      receivedTxHashes.push_back(*reinterpret_cast<Hash*>(&hash));
    }

    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(expectedTxHashes, receivedTxHashes);
}
