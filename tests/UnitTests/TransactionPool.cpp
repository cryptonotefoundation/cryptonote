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

#include <algorithm>

#include <boost/filesystem/operations.hpp>

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/ITransactionValidator.h"
#include "CryptoNoteCore/ITimeProvider.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteCore/TransactionPool.h"

#include <Logging/ConsoleLogger.h>
#include <Logging/LoggerGroup.h>

#include "TransactionApiHelpers.h"

using namespace CryptoNote;
using namespace CryptoNote;

class TransactionValidator : public CryptoNote::ITransactionValidator {
  virtual bool checkTransactionInputs(const CryptoNote::Transaction& tx, BlockInfo& maxUsedBlock) override {
    return true;
  }

  virtual bool checkTransactionInputs(const CryptoNote::Transaction& tx, BlockInfo& maxUsedBlock, BlockInfo& lastFailed) override {
    return true;
  }

  virtual bool haveSpentKeyImages(const CryptoNote::Transaction& tx) override {
    return false;
  }

  virtual bool checkTransactionSize(size_t blobSize) override {
    return true;
  }
};

class FakeTimeProvider : public ITimeProvider {
public:
  FakeTimeProvider(time_t currentTime = time(nullptr))
    : timeNow(currentTime) {}

  time_t timeNow;
  virtual time_t now() override { return timeNow; }
};


class TestTransactionGenerator {

public:

  TestTransactionGenerator(const CryptoNote::Currency& currency, size_t ringSize) :
    m_currency(currency),
    m_ringSize(ringSize),
    m_miners(ringSize), 
    m_miner_txs(ringSize), 
    m_public_keys(ringSize), 
    m_public_key_ptrs(ringSize) 
  {
    rv_acc.generate();
  }

  bool createSources() {

    size_t real_source_idx = m_ringSize / 2;

    std::vector<TransactionSourceEntry::OutputEntry> output_entries;
    for (uint32_t i = 0; i < m_ringSize; ++i)
    {
      m_miners[i].generate();

      if (!m_currency.constructMinerTx(BLOCK_MAJOR_VERSION_1, 0, 0, 0, 2, 0, m_miners[i].getAccountKeys().address, m_miner_txs[i])) {
        return false;
      }

      KeyOutput tx_out = boost::get<KeyOutput>(m_miner_txs[i].outputs[0].target);
      output_entries.push_back(std::make_pair(i, tx_out.key));
      m_public_keys[i] = tx_out.key;
      m_public_key_ptrs[i] = &m_public_keys[i];
    }

    m_source_amount = m_miner_txs[0].outputs[0].amount;

    TransactionSourceEntry source_entry;
    source_entry.amount = m_source_amount;
    source_entry.realTransactionPublicKey = getTransactionPublicKeyFromExtra(m_miner_txs[real_source_idx].extra);
    source_entry.realOutputIndexInTransaction = 0;
    source_entry.outputs.swap(output_entries);
    source_entry.realOutput = real_source_idx;

    m_sources.push_back(source_entry);

    m_realSenderKeys = m_miners[real_source_idx].getAccountKeys();

    return true;
  }

  void construct(uint64_t amount, uint64_t fee, size_t outputs, Transaction& tx) {

    std::vector<TransactionDestinationEntry> destinations;
    uint64_t amountPerOut = (amount - fee) / outputs;

    for (size_t i = 0; i < outputs; ++i) {
      destinations.push_back(TransactionDestinationEntry(amountPerOut, rv_acc.getAccountKeys().address));
    }

    constructTransaction(m_realSenderKeys, m_sources, destinations, std::vector<uint8_t>(), tx, 0, m_logger);
  }

  std::vector<AccountBase> m_miners;
  std::vector<Transaction> m_miner_txs;
  std::vector<TransactionSourceEntry> m_sources;
  std::vector<Crypto::PublicKey> m_public_keys;
  std::vector<const Crypto::PublicKey*> m_public_key_ptrs;

  Logging::LoggerGroup m_logger;
  const CryptoNote::Currency& m_currency;
  const size_t m_ringSize;
  AccountKeys m_realSenderKeys;
  uint64_t m_source_amount;
  AccountBase rv_acc;
};

class tx_pool : public ::testing::Test {
public:

  tx_pool() : 
    currency(CryptoNote::CurrencyBuilder(logger).currency()) {}

protected:
  virtual void SetUp() override {
    m_configDir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("test_data_%%%%%%%%%%%%");
  }

  virtual void TearDown() override {
    boost::system::error_code ignoredErrorCode;
    boost::filesystem::remove_all(m_configDir, ignoredErrorCode);
  }

protected:
  Logging::ConsoleLogger logger;
  CryptoNote::Currency currency;
  boost::filesystem::path m_configDir;
};

namespace
{
  static const size_t textMaxCumulativeSize = std::numeric_limits<size_t>::max();

  void GenerateTransaction(const CryptoNote::Currency& currency, Transaction& tx, uint64_t fee, size_t outputs) {
    TestTransactionGenerator txGenerator(currency, 1);
    txGenerator.createSources();
    txGenerator.construct(txGenerator.m_source_amount, fee, outputs, tx);
  }
  
  template <typename Validator, typename TimeProvider>
  class TestPool {
  public:

    Validator validator;
    TimeProvider timeProvider;

    TestPool(const CryptoNote::Currency& currency, Logging::ILogger& logger) {}
  };

  class TxTestBase {
  public:
    TxTestBase(size_t ringSize) :
      m_currency(CryptoNote::CurrencyBuilder(m_logger).currency()),
      txGenerator(m_currency, ringSize)
    {
      txGenerator.createSources();
    }

    void construct(uint64_t fee, size_t outputs, Transaction& tx) {
      txGenerator.construct(txGenerator.m_source_amount, fee, outputs, tx);
    }

    Logging::ConsoleLogger m_logger;
    CryptoNote::Currency m_currency;
    CryptoNote::RealTimeProvider m_time;
    TestTransactionGenerator txGenerator;
    TransactionValidator validator;
  };

  void InitBlock(BlockTemplate& bl, uint8_t majorVersion = BLOCK_MAJOR_VERSION_1) {
    bl.majorVersion = majorVersion;
    bl.minorVersion = 0;
    bl.nonce = 0;
    bl.timestamp = time(0);
    bl.previousBlockHash = NULL_HASH;
  }

}

/*

TEST_F(tx_pool, add_one_tx)
{
  TxTestBase test(1);
  Transaction tx;

  test.construct(test.m_currency.minimumFee(), 1, tx);

  ASSERT_TRUE(test.pool.add_tx(tx, tvc, false));
  ASSERT_FALSE(tvc.m_verifivation_failed);
};

TEST_F(tx_pool, take_tx)
{
  TxTestBase test(1);
  Transaction tx;

  test.construct(test.m_currency.minimumFee(), 1, tx);

  auto txhash = getObjectHash(tx);

  TransactionValidatorState tvc;

  ASSERT_TRUE(test.pool.add_tx(tx, tvc, false));
  ASSERT_FALSE(tvc.m_verifivation_failed);

  Transaction txOut;
  size_t blobSize;
  uint64_t fee = 0;

  ASSERT_TRUE(test.pool.take_tx(txhash, txOut, blobSize, fee));
  ASSERT_EQ(fee, test.m_currency.minimumFee());
  ASSERT_EQ(tx, txOut);
};


TEST_F(tx_pool, double_spend_tx)
{
  TxTestBase test(1);
  Transaction tx, tx_double;

  test.construct(test.m_currency.minimumFee(), 1, tx);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();

  ASSERT_TRUE(test.pool.add_tx(tx, tvc, false));
  ASSERT_FALSE(tvc.m_verifivation_failed);

  test.txGenerator.rv_acc.generate(); // generate new receiver address
  test.construct(test.m_currency.minimumFee(), 1, tx_double);

  ASSERT_FALSE(test.pool.add_tx(tx_double, tvc, false));
  ASSERT_TRUE(tvc.m_verifivation_failed);
}


TEST_F(tx_pool, fillblock_same_fee)
{
  TestPool<TransactionValidator, RealTimeProvider> pool(currency, logger);
  uint64_t fee = currency.minimumFee();

  std::unordered_map<Crypto::Hash, std::unique_ptr<Transaction>> transactions;

  // generate transactions
  for (int i = 1; i <= 50; ++i) {
    TestTransactionGenerator txGenerator(currency, 1);
    txGenerator.createSources();
    
    std::unique_ptr<Transaction> txptr(new Transaction);
    Transaction& tx = *txptr;

    txGenerator.construct(txGenerator.m_source_amount, fee, i, tx);

    tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    ASSERT_TRUE(pool.add_tx(tx, tvc, false));
    ASSERT_TRUE(tvc.m_added_to_pool);

    transactions[getObjectHash(tx)] = std::move(txptr);
  }

  BlockTemplate bl;

  InitBlock(bl);

  size_t totalSize = 0;
  uint64_t txFee = 0;
  uint64_t median = 5000;

  ASSERT_TRUE(pool.fill_block_template(bl, median, textMaxCumulativeSize, 0, totalSize, txFee));
  ASSERT_TRUE(totalSize*100 < median*125);

  // now, check that the block is opimally filled
  // if fee is fixed, transactions with smaller number of outputs should be included

  size_t maxOuts = 0;

  for (auto& th : bl.transactionHashes) {
    auto iter = transactions.find(th);
    ASSERT_TRUE(iter != transactions.end());

    size_t txouts = iter->second->outputs.size();

    if (txouts > maxOuts)
      maxOuts = txouts;
  }

  ASSERT_TRUE(maxOuts <= bl.transactionHashes.size());
}


TEST_F(tx_pool, fillblock_same_size)
{
  TestPool<TransactionValidator, RealTimeProvider> pool(currency, logger);

  const uint64_t fee = currency.minimumFee();
  const size_t totalTransactions = 50;

  std::unordered_map<Crypto::Hash, std::unique_ptr<Transaction>> transactions;


  // generate transactions
  for (int i = 0; i <= totalTransactions; ++i) {

    TestTransactionGenerator txGenerator(currency, 1);
    txGenerator.createSources();

    std::unique_ptr<Transaction> txptr(new Transaction);
    Transaction& tx = *txptr;

    // interleave fee and fee*2
    txGenerator.construct(txGenerator.m_source_amount, fee + (fee * (i&1)), 1, tx);

    tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    ASSERT_TRUE(pool.add_tx(tx, tvc, false));
    ASSERT_TRUE(tvc.m_added_to_pool);

    transactions[getObjectHash(tx)] = std::move(txptr);
  }


  BlockTemplate bl;

  InitBlock(bl);

  size_t totalSize = 0;
  uint64_t txFee = 0;
  uint64_t median = 5000;

  ASSERT_TRUE(pool.fill_block_template(bl, median, textMaxCumulativeSize, 0, totalSize, txFee));
  ASSERT_TRUE(totalSize * 100 < median * 125);

  // check that fill_block_template prefers transactions with double fee

  size_t doubleFee = 0;

  for (auto& th : bl.transactionHashes) {

    auto iter = transactions.find(th);
    ASSERT_TRUE(iter != transactions.end());

    if (get_tx_fee(*iter->second) > fee)
      ++doubleFee;
  }

  ASSERT_TRUE(doubleFee == std::min(bl.transactionHashes.size(), totalTransactions / 2));

}


TEST_F(tx_pool, cleanup_stale_tx)
{
  TestPool<TransactionValidator, FakeTimeProvider> pool(currency, logger);
  const uint64_t fee = currency.minimumFee();

  time_t startTime = pool.timeProvider.now();

  for (int i = 0; i < 3; ++i) {
    Transaction tx;
    GenerateTransaction(currency, tx, fee, 1);

    tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    ASSERT_TRUE(pool.add_tx(tx, tvc, false)); // main chain
    ASSERT_TRUE(tvc.m_added_to_pool);

    pool.timeProvider.timeNow += 60 * 60 * 2; // add 2 hours
  }

  for (int i = 0; i < 5; ++i) {
    Transaction tx;
    GenerateTransaction(currency, tx, fee, 1);

    tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    ASSERT_TRUE(pool.add_tx(tx, tvc, true)); // alternative chain
    ASSERT_TRUE(tvc.m_added_to_pool);

    pool.timeProvider.timeNow += 60 * 60 * 2; // add 2 hours
  }


  ASSERT_EQ(8, pool.get_transactions_count());

  pool.timeProvider.timeNow = startTime + currency.mempoolTxLiveTime() + 3*60*60; 
  pool.on_idle(); // 2 transactions should be removed

  ASSERT_EQ(6, pool.get_transactions_count());

  pool.timeProvider.timeNow = startTime + currency.mempoolTxFromAltBlockLiveTime() + (3*2+3) * 60 * 60;
  pool.on_idle(); // all transactions from main chain and 2 transactions from altchain should be removed

  ASSERT_EQ(3, pool.get_transactions_count());
}

TEST_F(tx_pool, add_tx_after_cleanup)
{
  TestPool<TransactionValidator, FakeTimeProvider> pool(currency, logger);
  const uint64_t fee = currency.minimumFee();

  time_t startTime = pool.timeProvider.now();

  Transaction tx;
  GenerateTransaction(currency, tx, fee, 1);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  ASSERT_TRUE(pool.add_tx(tx, tvc, false)); // main chain
  ASSERT_TRUE(tvc.m_added_to_pool);

  uint64_t cleanupTime = startTime + currency.mempoolTxLiveTime() + 1;
  pool.timeProvider.timeNow = cleanupTime;
  pool.on_idle();

  pool.timeProvider.timeNow = cleanupTime + currency.numberOfPeriodsToForgetTxDeletedFromPool() * currency.mempoolTxLiveTime() + 1;
  pool.on_idle();

  ASSERT_EQ(0, pool.get_transactions_count());

  // add again
  ASSERT_TRUE(pool.add_tx(tx, tvc, false)); // main chain
  ASSERT_TRUE(tvc.m_added_to_pool);

  ASSERT_EQ(1, pool.get_transactions_count());

}

TEST_F(tx_pool, RecentlyDeletedTransactionCannotBeAddedToTxPoolAgain) {
  TestPool<TransactionValidator, FakeTimeProvider> pool(currency, logger);

  uint64_t startTime = pool.timeProvider.now();

  Transaction tx;
  GenerateTransaction(currency, tx, currency.minimumFee(), 1);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  ASSERT_TRUE(pool.add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);

  uint64_t deleteTime = startTime + currency.mempoolTxLiveTime() + 1;
  pool.timeProvider.timeNow = deleteTime;
  pool.on_idle();
  ASSERT_EQ(0, pool.get_transactions_count());

  // Try to add tx again
  ASSERT_TRUE(pool.add_tx(tx, tvc, false));
  ASSERT_FALSE(tvc.m_added_to_pool);
  ASSERT_FALSE(tvc.m_should_be_relayed);
  ASSERT_FALSE(tvc.m_verifivation_failed);
  ASSERT_FALSE(tvc.m_verifivation_impossible);

  ASSERT_EQ(0, pool.get_transactions_count());
}

TEST_F(tx_pool, RecentlyDeletedTransactionCanBeAddedAgainAfterSomeTime) {
  TestPool<TransactionValidator, FakeTimeProvider> pool(currency, logger);

  uint64_t startTime = pool.timeProvider.now();

  Transaction tx;
  GenerateTransaction(currency, tx, currency.minimumFee(), 1);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  ASSERT_TRUE(pool.add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);

  uint64_t deleteTime = startTime + currency.mempoolTxLiveTime() + 1;
  pool.timeProvider.timeNow = deleteTime;
  pool.on_idle();
  ASSERT_EQ(0, pool.get_transactions_count());

  uint64_t forgetDeletedTxTime = deleteTime + currency.numberOfPeriodsToForgetTxDeletedFromPool() * currency.mempoolTxLiveTime() + 1;
  pool.timeProvider.timeNow = forgetDeletedTxTime;
  pool.on_idle();

  // Try to add tx again
  ASSERT_TRUE(pool.add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);
  ASSERT_TRUE(tvc.m_should_be_relayed);
  ASSERT_FALSE(tvc.m_verifivation_failed);
  ASSERT_FALSE(tvc.m_verifivation_impossible);

  ASSERT_EQ(1, pool.get_transactions_count());
}

TEST_F(tx_pool, RecentlyDeletedTransactionCanBeAddedToTxPoolIfItIsReceivedInBlock) {
  TestPool<TransactionValidator, FakeTimeProvider> pool(currency, logger);

  uint64_t startTime = pool.timeProvider.now();

  Transaction tx;
  GenerateTransaction(currency, tx, currency.minimumFee(), 1);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  ASSERT_TRUE(pool.add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);

  uint64_t deleteTime = startTime + currency.mempoolTxLiveTime() + 1;
  pool.timeProvider.timeNow = deleteTime;
  pool.on_idle();
  ASSERT_EQ(0, pool.get_transactions_count());

  // Try to add tx again
  ASSERT_TRUE(pool.add_tx(tx, tvc, true));
  ASSERT_TRUE(tvc.m_added_to_pool);
  ASSERT_TRUE(tvc.m_should_be_relayed);
  ASSERT_FALSE(tvc.m_verifivation_failed);
  ASSERT_FALSE(tvc.m_verifivation_impossible);

  ASSERT_EQ(1, pool.get_transactions_count());
}

TEST_F(tx_pool, OldTransactionIsDeletedDuringTxPoolInitialization) {
  TransactionValidator validator;
  FakeTimeProvider timeProvider;
  std::unique_ptr<tx_memory_pool> pool(new tx_memory_pool(currency, validator, timeProvider, logger));
  ASSERT_TRUE(pool->init(m_configDir.string()));

  uint64_t startTime = timeProvider.now();

  Transaction tx;
  GenerateTransaction(currency, tx, currency.minimumFee(), 1);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  ASSERT_TRUE(pool->add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);

  ASSERT_TRUE(pool->deinit());
  pool.reset();

  uint64_t deleteTime = startTime + currency.mempoolTxLiveTime() + 1;
  timeProvider.timeNow = deleteTime;

  pool.reset(new tx_memory_pool(currency, validator, timeProvider, logger));
  ASSERT_TRUE(pool->init(m_configDir.string()));
  ASSERT_EQ(0, pool->get_transactions_count());
}

TEST_F(tx_pool, TransactionThatWasDeletedLongAgoIsForgottenDuringTxPoolInitialization) {
  TransactionValidator validator;
  FakeTimeProvider timeProvider;
  std::unique_ptr<tx_memory_pool> pool(new tx_memory_pool(currency, validator, timeProvider, logger));
  ASSERT_TRUE(pool->init(m_configDir.string()));

  uint64_t startTime = timeProvider.now();

  Transaction tx;
  GenerateTransaction(currency, tx, currency.minimumFee(), 1);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  ASSERT_TRUE(pool->add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);

  uint64_t deleteTime = startTime + currency.mempoolTxLiveTime() + 1;
  timeProvider.timeNow = deleteTime;
  pool->on_idle();
  ASSERT_EQ(0, pool->get_transactions_count());

  ASSERT_TRUE(pool->deinit());
  pool.reset();

  uint64_t forgetDeletedTxTime = deleteTime + currency.numberOfPeriodsToForgetTxDeletedFromPool() * currency.mempoolTxLiveTime() + 1;
  timeProvider.timeNow = forgetDeletedTxTime;

  pool.reset(new tx_memory_pool(currency, validator, timeProvider, logger));
  ASSERT_TRUE(pool->init(m_configDir.string()));

  // Try to add tx again
  ASSERT_TRUE(pool->add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);
  ASSERT_TRUE(tvc.m_should_be_relayed);
  ASSERT_FALSE(tvc.m_verifivation_failed);
  ASSERT_FALSE(tvc.m_verifivation_impossible);

  ASSERT_EQ(1, pool->get_transactions_count());
}

TEST_F(tx_pool, RecentlyDeletedTxInfoIsSerializedAndDeserialized) {
  TransactionValidator validator;
  FakeTimeProvider timeProvider;
  std::unique_ptr<tx_memory_pool> pool(new tx_memory_pool(currency, validator, timeProvider, logger));
  ASSERT_TRUE(pool->init(m_configDir.string()));

  uint64_t startTime = timeProvider.now();

  Transaction tx;
  GenerateTransaction(currency, tx, currency.minimumFee(), 1);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  ASSERT_TRUE(pool->add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);

  uint64_t deleteTime = startTime + currency.mempoolTxLiveTime() + 1;
  timeProvider.timeNow = deleteTime;
  pool->on_idle();
  ASSERT_EQ(0, pool->get_transactions_count());

  ASSERT_TRUE(pool->deinit());

  pool.reset(new tx_memory_pool(currency, validator, timeProvider, logger));
  ASSERT_TRUE(pool->init(m_configDir.string()));

  uint64_t timeBeforeCleanupDeletedTx = deleteTime + currency.numberOfPeriodsToForgetTxDeletedFromPool() * currency.mempoolTxLiveTime();
  timeProvider.timeNow = timeBeforeCleanupDeletedTx;
  pool->on_idle();

  ASSERT_TRUE(pool->add_tx(tx, tvc, false));
  ASSERT_FALSE(tvc.m_added_to_pool);
  ASSERT_FALSE(tvc.m_should_be_relayed);
  ASSERT_FALSE(tvc.m_verifivation_failed);
  ASSERT_FALSE(tvc.m_verifivation_impossible);

  ASSERT_EQ(0, pool->get_transactions_count());

  timeProvider.timeNow = timeBeforeCleanupDeletedTx + 61;
  pool->on_idle();

  // Try to add tx again
  ASSERT_TRUE(pool->add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);
  ASSERT_TRUE(tvc.m_should_be_relayed);
  ASSERT_FALSE(tvc.m_verifivation_failed);
  ASSERT_FALSE(tvc.m_verifivation_impossible);

  ASSERT_EQ(1, pool->get_transactions_count());
}


TEST_F(tx_pool, TxPoolAcceptsValidFusionTransaction) {
  // TODO fix this test
//  ASSERT_FALSE(true);

  TransactionValidator validator;
  FakeTimeProvider timeProvider;
  std::unique_ptr<tx_memory_pool> pool(new tx_memory_pool(currency, validator, timeProvider, logger));
  ASSERT_TRUE(pool->init(m_configDir.string()));

  FusionTransactionBuilder builder(currency, 10 * currency.defaultDustThreshold());
  auto tx = builder.buildTx();
  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();

  ASSERT_TRUE(pool->add_tx(tx, tvc, false));
  ASSERT_TRUE(tvc.m_added_to_pool);
  ASSERT_TRUE(tvc.m_should_be_relayed);
  ASSERT_FALSE(tvc.m_verifivation_failed);
  ASSERT_FALSE(tvc.m_verifivation_impossible);
}

TEST_F(tx_pool, TxPoolDoesNotAcceptInvalidFusionTransaction) {
  // TODO fix this test
  ASSERT_FALSE(true);

  //TransactionValidator validator;
  //FakeTimeProvider timeProvider;
  //std::unique_ptr<tx_memory_pool> pool(new tx_memory_pool(currency, validator, timeProvider, logger));
  //ASSERT_TRUE(pool->init(m_configDir.string()));

  //FusionTransactionBuilder builder(currency, 10 * currency.defaultDustThreshold());
  //builder.setInputCount(currency.fusionTxMinInputCount() - 1);
  //auto tx = builder.buildTx();
  //tx_verification_context tvc = boost::value_initialized<tx_verification_context>();

  //ASSERT_FALSE(pool->add_tx(tx, tvc, false));
  //ASSERT_FALSE(tvc.m_added_to_pool);
  //ASSERT_FALSE(tvc.m_should_be_relayed);
  //ASSERT_TRUE(tvc.m_verifivation_failed);
  //ASSERT_FALSE(tvc.m_verifivation_impossible);
}

namespace {

const size_t TEST_FUSION_TX_COUNT_PER_BLOCK = 3;
const size_t TEST_TX_COUNT_UP_TO_MEDIAN = 10;
const size_t TEST_MAX_TX_COUNT_PER_BLOCK = TEST_TX_COUNT_UP_TO_MEDIAN * 125 / 100;
const size_t TEST_TRANSACTION_SIZE = 2000;
const size_t TEST_FUSION_TX_MAX_SIZE = TEST_FUSION_TX_COUNT_PER_BLOCK * TEST_TRANSACTION_SIZE;
const size_t TEST_MINER_TX_BLOB_RESERVED_SIZE = 600;
const size_t TEST_MEDIAN_SIZE = TEST_TX_COUNT_UP_TO_MEDIAN * TEST_TRANSACTION_SIZE + TEST_MINER_TX_BLOB_RESERVED_SIZE;

Transaction createTestFusionTransaction(const Currency& currency) {
  FusionTransactionBuilder builder(currency, 30 * currency.defaultDustThreshold());
  return builder.createFusionTransactionBySize(TEST_TRANSACTION_SIZE);
}

Transaction createTestOrdinaryTransactionWithExtra(const Currency& currency, size_t extraSize) {
  TestTransactionBuilder builder;
  if (extraSize != 0) {
    builder.appendExtra(BinaryArray(extraSize, 0));
  }

  builder.addTestInput(100 * currency.minimumFee());
  builder.addTestKeyOutput(99 * currency.minimumFee(), 0);
  return convertTx(*builder.build());
}

Transaction createTestOrdinaryTransaction(const Currency& currency) {
  auto tx = createTestOrdinaryTransactionWithExtra(currency, 0);
  size_t realSize = getObjectBinarySize(tx);
  if (realSize < TEST_TRANSACTION_SIZE) {
    size_t extraSize = TEST_TRANSACTION_SIZE - realSize;
    tx = createTestOrdinaryTransactionWithExtra(currency, extraSize);

    realSize = getObjectBinarySize(tx);
    if (realSize > TEST_TRANSACTION_SIZE) {
      extraSize -= realSize - TEST_TRANSACTION_SIZE;
      tx = createTestOrdinaryTransactionWithExtra(currency, extraSize);
    }
  }

  return tx;
}

class TxPool_FillBlockTemplate : public tx_pool {
public:
  TxPool_FillBlockTemplate() :
    tx_pool() {
    // TODO fix it
    //currency = CryptoNote::CurrencyBuilder(logger).fusionTxMaxSize(TEST_FUSION_TX_MAX_SIZE).blockGrantedFullRewardZone(TEST_MEDIAN_SIZE).currency();
  }

  void doTest(size_t poolOrdinaryTxCount, size_t poolFusionTxCount, size_t expectedBlockOrdinaryTxCount, size_t expectedBlockFusionTxCount) {
    // TODO fix tests
    //TransactionValidator validator;
    //FakeTimeProvider timeProvider;
    //std::unique_ptr<tx_memory_pool> pool(new tx_memory_pool(currency, validator, timeProvider, logger));
    //ASSERT_TRUE(pool->init(m_configDir.string()));

    //std::unordered_map<Crypto::Hash, Transaction> ordinaryTxs;
    //for (size_t i = 0; i < poolOrdinaryTxCount; ++i) {
    //  auto tx = createTestOrdinaryTransaction(currency);
    //  ordinaryTxs.emplace(getObjectHash(tx), std::move(tx));
    //}

    //std::unordered_map<Crypto::Hash, Transaction> fusionTxs;
    //for (size_t i = 0; i < poolFusionTxCount; ++i) {
    //  auto tx = createTestFusionTransaction(currency);
    //  fusionTxs.emplace(getObjectHash(tx), std::move(tx));
    //}

    //for (auto pair : ordinaryTxs) {
    //  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    //  ASSERT_TRUE(pool->add_tx(pair.second, tvc, false));
    //}

    //for (auto pair : fusionTxs) {
    //  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    //  ASSERT_TRUE(pool->add_tx(pair.second, tvc, false));
    //}

    //Block block;
    //size_t totalSize;
    //uint64_t totalFee;
    //ASSERT_TRUE(pool->fill_block_template(block, currency.blockGrantedFullRewardZone(), std::numeric_limits<size_t>::max(), 0, totalSize, totalFee));

    //size_t fusionTxCount = 0;
    //size_t ordinaryTxCount = 0;
    //for (auto txHash : block.transactionHashes) {
    //  if (fusionTxs.count(txHash) > 0) {
    //    ++fusionTxCount;
    //  } else {
    //    ++ordinaryTxCount;
    //  }
    //}

    //ASSERT_EQ(expectedBlockOrdinaryTxCount, ordinaryTxCount);
    //ASSERT_EQ(expectedBlockFusionTxCount, fusionTxCount);
  }
};

}

TEST_F(TxPool_FillBlockTemplate, TxPoolAddsFusionTransactionsToBlockTemplateNoMoreThanLimit) {
  // TODO fix this test
  ASSERT_FALSE(true);
  ASSERT_NO_FATAL_FAILURE(doTest(TEST_MAX_TX_COUNT_PER_BLOCK,
    TEST_MAX_TX_COUNT_PER_BLOCK,
    TEST_MAX_TX_COUNT_PER_BLOCK - TEST_FUSION_TX_COUNT_PER_BLOCK,
    TEST_FUSION_TX_COUNT_PER_BLOCK));
}

TEST_F(TxPool_FillBlockTemplate, TxPoolAddsFusionTransactionsUpToMedianAfterOrdinaryTransactions) {
  // TODO fix this test
  ASSERT_FALSE(true);
  static_assert(TEST_MAX_TX_COUNT_PER_BLOCK > 2, "TEST_MAX_TX_COUNT_PER_BLOCK > 2");
  ASSERT_NO_FATAL_FAILURE(doTest(2, TEST_MAX_TX_COUNT_PER_BLOCK, 2, TEST_TX_COUNT_UP_TO_MEDIAN - 2));
}

TEST_F(TxPool_FillBlockTemplate, TxPoolAddsFusionTransactionsUpToMedianIfThereAreNoOrdinaryTransactions) {
  // TODO fix this test
  ASSERT_FALSE(true);
  ASSERT_NO_FATAL_FAILURE(doTest(0, TEST_MAX_TX_COUNT_PER_BLOCK, 0, TEST_TX_COUNT_UP_TO_MEDIAN));
}

TEST_F(TxPool_FillBlockTemplate, TxPoolContinuesToAddOrdinaryTransactionsUpTo125PerCentOfMedianAfterAddingFusionTransactions) {
  // TODO fix this test
  ASSERT_FALSE(true);
  size_t fusionTxCount = TEST_FUSION_TX_COUNT_PER_BLOCK - 1;
  ASSERT_NO_FATAL_FAILURE(doTest(TEST_MAX_TX_COUNT_PER_BLOCK,
    fusionTxCount,
    TEST_MAX_TX_COUNT_PER_BLOCK - fusionTxCount,
    fusionTxCount));
}
*/
