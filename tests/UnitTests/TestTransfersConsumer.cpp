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

#include "CryptoNoteCore/TransactionApi.h"
#include "Logging/ConsoleLogger.h"
#include "Transfers/TransfersConsumer.h"

#include <algorithm>
#include <limits>
#include <Transfers/CommonTypes.h>
#include <CryptoNoteCore/TransactionApi.h>

#include "INodeStubs.h"
#include "TransactionApiHelpers.h"
#include "TransfersObserver.h"
#include "TestBlockchainGenerator.h"

using namespace CryptoNote;

AccountSubscription getAccountSubscription(const AccountKeys& accountKeys) {
  AccountSubscription subscription;
  subscription.keys = accountKeys;

  return subscription;
}

AccountKeys getAccountKeysWithViewKey(const PublicKey& publicViewKey, const SecretKey& secretViewKey) {
  KeyPair viewKp;
  viewKp.publicKey = publicViewKey;
  viewKp.secretKey = secretViewKey;
  KeyPair p1;
  Crypto::generate_keys(p1.publicKey, p1.secretKey);
  AccountKeys accountKeys = accountKeysFromKeypairs(viewKp, p1);

  return accountKeys;
}

class TransfersConsumerTest : public ::testing::Test {
public:
  TransfersConsumerTest();

protected:

  ITransfersSubscription& addSubscription(TransfersConsumer& consumer, const AccountKeys& acc, uint64_t height = 0,
    uint64_t timestamp = 0, size_t age = 0)
  {
    AccountSubscription subscription = getAccountSubscription(acc);
    subscription.syncStart.height = height;
    subscription.syncStart.timestamp = timestamp;
    subscription.transactionSpendableAge = age;
    return consumer.addSubscription(subscription);
  }

  ITransfersSubscription& addSubscription(const AccountKeys& acc, uint64_t height = 0, uint64_t timestamp = 0, size_t age = 0) {
    return addSubscription(m_consumer, acc, height, timestamp, age);
  }

  ITransfersSubscription& addSubscription(uint64_t height = 0, uint64_t timestamp = 0, size_t age = 0) {
    return addSubscription(m_consumer, m_accountKeys, height, timestamp, age);
  }

  ITransfersSubscription& addSubscription(TransfersConsumer& consumer, uint64_t height = 0, uint64_t timestamp = 0, size_t age = 0) {
    return addSubscription(consumer, m_accountKeys, height, timestamp, age);
  }

  AccountKeys generateAccount() {
    return getAccountKeysWithViewKey(m_accountKeys.address.viewPublicKey, m_accountKeys.viewSecretKey);
  }

  Logging::ConsoleLogger m_logger;
  CryptoNote::Currency m_currency;
  TestBlockchainGenerator m_generator;
  INodeTrivialRefreshStub m_node;
  AccountKeys m_accountKeys;
  TransfersConsumer m_consumer;
};

TransfersConsumerTest::TransfersConsumerTest() :
  m_currency(CryptoNote::CurrencyBuilder(m_logger).currency()),
  m_generator(m_currency),
  m_node(m_generator, true),
  m_accountKeys(generateAccountKeys()),
  m_consumer(m_currency, m_node, m_logger, m_accountKeys.viewSecretKey)
{
}

bool amountFound(const std::vector<TransactionOutputInformation>& outs, uint64_t amount) {
  return std::find_if(outs.begin(), outs.end(), [amount] (const TransactionOutputInformation& inf) { return inf.amount == amount; }) != outs.end();
}

AccountSubscription getAccountSubscriptionWithSyncStart(const AccountKeys& keys, uint64_t timestamp, uint64_t height) {
  AccountSubscription subscription = getAccountSubscription(keys);
  subscription.syncStart.timestamp = timestamp;
  subscription.syncStart.height = height;

  return subscription;
}

TEST_F(TransfersConsumerTest, addSubscription_Success) {
  AccountSubscription subscription;
  subscription.keys = m_accountKeys;

  ITransfersSubscription& accountSubscription = m_consumer.addSubscription(subscription);
  ASSERT_EQ(subscription.keys.address, accountSubscription.getAddress());
}

TEST_F(TransfersConsumerTest, addSubscription_WrongViewKey) {
  AccountKeys accountKeys = generateAccountKeys();
  AccountSubscription subscription = getAccountSubscription(accountKeys);

  ASSERT_ANY_THROW(m_consumer.addSubscription(subscription));
}

TEST_F(TransfersConsumerTest, addSubscription_SameSubscription) {
  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  ITransfersSubscription* ts1 = &m_consumer.addSubscription(subscription);
  ITransfersSubscription* ts2 = &m_consumer.addSubscription(subscription);

  ASSERT_EQ(ts1, ts2);
}

TEST_F(TransfersConsumerTest, removeSubscription_Success) {
  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  m_consumer.addSubscription(subscription);

  ITransfersSubscription* ts = m_consumer.getSubscription(m_accountKeys.address);
  ASSERT_NE(nullptr, ts);

  m_consumer.removeSubscription(m_accountKeys.address);
  ts = m_consumer.getSubscription(m_accountKeys.address);
  ASSERT_EQ(nullptr, ts);
}

TEST_F(TransfersConsumerTest, removeSubscription_OneAddressLeft) {
  AccountSubscription subscription1 = getAccountSubscription(m_accountKeys);
  m_consumer.addSubscription(subscription1);

  AccountKeys accountKeys = getAccountKeysWithViewKey(m_accountKeys.address.viewPublicKey, m_accountKeys.viewSecretKey);
  AccountSubscription subscription2 = getAccountSubscription(accountKeys);

  m_consumer.addSubscription(subscription2);

  ASSERT_FALSE(m_consumer.removeSubscription(subscription1.keys.address));
}

TEST_F(TransfersConsumerTest, removeSubscription_RemoveAllAddresses) {
  AccountSubscription subscription1 = getAccountSubscription(m_accountKeys);
  m_consumer.addSubscription(subscription1);

  ASSERT_TRUE(m_consumer.removeSubscription(subscription1.keys.address));
}

TEST_F(TransfersConsumerTest, getSubscription_ReturnSameValueForSameAddress) {
  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  m_consumer.addSubscription(subscription);

  ITransfersSubscription* ts1 = m_consumer.getSubscription(m_accountKeys.address);
  ITransfersSubscription* ts2 = m_consumer.getSubscription(m_accountKeys.address);

  ASSERT_EQ(ts1, ts2);
}

TEST_F(TransfersConsumerTest, getSubscription_ReturnNullForNonExistentAddr) {
  AccountSubscription subscription1 = getAccountSubscription(m_accountKeys);
  m_consumer.addSubscription(subscription1);

  AccountKeys accountKeys = getAccountKeysWithViewKey(m_accountKeys.address.viewPublicKey, m_accountKeys.viewSecretKey);

  ASSERT_EQ(nullptr, m_consumer.getSubscription(accountKeys.address));
}

TEST_F(TransfersConsumerTest, getSubscriptions_Empty) {
  std::vector<AccountPublicAddress> subscriptions;
  m_consumer.getSubscriptions(subscriptions);

  ASSERT_TRUE(subscriptions.empty());
}

TEST_F(TransfersConsumerTest, getSubscriptions_TwoSubscriptions) {
  AccountSubscription subscription1 = getAccountSubscription(m_accountKeys);
  m_consumer.addSubscription(subscription1);

  AccountKeys accountKeys = getAccountKeysWithViewKey(m_accountKeys.address.viewPublicKey, m_accountKeys.viewSecretKey);
  AccountSubscription subscription2 = getAccountSubscription(accountKeys);
  m_consumer.addSubscription(subscription2);

  std::vector<AccountPublicAddress> subscriptions;
  m_consumer.getSubscriptions(subscriptions);

  ASSERT_EQ(2, subscriptions.size());
  ASSERT_NE(subscriptions.end(), std::find(subscriptions.begin(), subscriptions.end(), subscription1.keys.address));
  ASSERT_NE(subscriptions.end(), std::find(subscriptions.begin(), subscriptions.end(), subscription2.keys.address));
}

TEST_F(TransfersConsumerTest, getSyncStart_Empty) {
  auto syncStart = m_consumer.getSyncStart();

  EXPECT_EQ(std::numeric_limits<uint64_t>::max(), syncStart.height);
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(), syncStart.timestamp);
}

TEST_F(TransfersConsumerTest, getSyncStart_OneSubscription) {
  const uint64_t height = 1209384;
  const uint64_t timestamp = 99284512;

  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  subscription.syncStart.height = height;
  subscription.syncStart.timestamp = timestamp;

  m_consumer.addSubscription(subscription);

  auto sync = m_consumer.getSyncStart();
  ASSERT_EQ(height, sync.height);
  ASSERT_EQ(timestamp, sync.timestamp);
}

TEST_F(TransfersConsumerTest, getSyncStart_MinSyncSameSubscription) {
  const uint64_t height = 1209384;
  const uint64_t timestamp = 99284512;
  const uint64_t minHeight = 120984;
  const uint64_t minTimestamp = 9984512;

  AccountSubscription subscription1 = getAccountSubscription(m_accountKeys);
  subscription1.syncStart.height = height;
  subscription1.syncStart.timestamp = timestamp;

  AccountKeys accountKeys = getAccountKeysWithViewKey(m_accountKeys.address.viewPublicKey, m_accountKeys.viewSecretKey);
  AccountSubscription subscription2 = getAccountSubscription(accountKeys);

  subscription2.syncStart.height = minHeight;
  subscription2.syncStart.timestamp = minTimestamp;

  m_consumer.addSubscription(subscription1);
  m_consumer.addSubscription(subscription2);

  auto sync = m_consumer.getSyncStart();
  ASSERT_EQ(minHeight, sync.height);
  ASSERT_EQ(minTimestamp, sync.timestamp);
}

TEST_F(TransfersConsumerTest, getSyncStart_MinSyncDifferentSubscriptions) {
  const uint64_t height = 1209384;
  const uint64_t timestamp = 99284512;
  const uint64_t minHeight = 120984;
  const uint64_t minTimestamp = 9984512;

  AccountSubscription subscription1 = getAccountSubscription(m_accountKeys);
  subscription1.syncStart.height = minHeight;
  subscription1.syncStart.timestamp = timestamp;

  AccountKeys accountKeys = getAccountKeysWithViewKey(m_accountKeys.address.viewPublicKey, m_accountKeys.viewSecretKey);
  AccountSubscription subscription2 = getAccountSubscription(accountKeys);

  subscription2.syncStart.height = height;
  subscription2.syncStart.timestamp = minTimestamp;

  m_consumer.addSubscription(subscription1);
  m_consumer.addSubscription(subscription2);

  auto sync = m_consumer.getSyncStart();
  ASSERT_EQ(minHeight, sync.height);
  ASSERT_EQ(minTimestamp, sync.timestamp);
}

TEST_F(TransfersConsumerTest, getSyncStart_RemoveMinSyncSubscription) {
  const uint64_t height = 1209384;
  const uint64_t timestamp = 99284512;
  const uint64_t minHeight = 120984;
  const uint64_t minTimestamp = 9984512;

  AccountSubscription subscription1 = getAccountSubscription(m_accountKeys);
  subscription1.syncStart.height = height;
  subscription1.syncStart.timestamp = timestamp;

  AccountKeys accountKeys = getAccountKeysWithViewKey(m_accountKeys.address.viewPublicKey, m_accountKeys.viewSecretKey);
  AccountSubscription subscription2 = getAccountSubscription(accountKeys);

  subscription2.syncStart.height = minHeight;
  subscription2.syncStart.timestamp = minTimestamp;

  m_consumer.addSubscription(subscription1);
  m_consumer.addSubscription(subscription2);
  m_consumer.removeSubscription(subscription2.keys.address);

  auto sync = m_consumer.getSyncStart();
  ASSERT_EQ(height, sync.height);
  ASSERT_EQ(timestamp, sync.timestamp);
}

TEST_F(TransfersConsumerTest, onBlockchainDetach) {
  auto& container1 = addSubscription().getContainer();
  auto keys = generateAccount();
  auto& container2 = addSubscription(keys).getContainer();

  std::shared_ptr<ITransaction> tx1 = createTransaction();
  addTestInput(*tx1, 100);
  addTestKeyOutput(*tx1, 50, 1, m_accountKeys);

  std::shared_ptr<ITransaction> tx2 = createTransaction();
  addTestInput(*tx1, 100);
  addTestKeyOutput(*tx1, 50, 1, keys);

  CompleteBlock blocks[3];
  blocks[0].block = CryptoNote::BlockTemplate();
  blocks[0].block->timestamp = 1233;

  blocks[1].block = CryptoNote::BlockTemplate();
  blocks[1].block->timestamp = 1234;
  blocks[1].transactions.push_back(tx1);

  blocks[2].block = CryptoNote::BlockTemplate();
  blocks[2].block->timestamp = 1235;
  blocks[2].transactions.push_back(tx2);

  ASSERT_TRUE(m_consumer.onNewBlocks(&blocks[0], 0, 3));

  m_consumer.onBlockchainDetach(0);
  std::vector<TransactionOutputInformation> trs;
  container1.getOutputs(trs, ITransfersContainer::IncludeAll);
  ASSERT_EQ(0, trs.size());

  container2.getOutputs(trs, ITransfersContainer::IncludeAll);
  ASSERT_EQ(0, trs.size());
}

TEST_F(TransfersConsumerTest, onNewBlocks_OneEmptyBlockOneFilled) {
  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  subscription.syncStart.height = 1;
  subscription.syncStart.timestamp = 1234;

  TestTransactionBuilder b1;  
  auto unknownSender = generateAccountKeys();
  b1.addTestInput(1000, unknownSender);
  b1.addTestKeyOutput(123, 1, m_accountKeys);

  TestTransactionBuilder b2;
  b2.addTestInput(10000, unknownSender);
  b2.addTestKeyOutput(850, 2, m_accountKeys);
  b2.addTestKeyOutput(900, 3, m_accountKeys);

  auto tx1 = std::shared_ptr<ITransactionReader>(b1.build().release());
  auto tx2 = std::shared_ptr<ITransactionReader>(b2.build().release());

  CompleteBlock blocks[2];
  blocks[0].transactions.push_back(tx1);
  blocks[1].block = CryptoNote::BlockTemplate();
  blocks[1].block->timestamp = 1235;
  blocks[1].transactions.push_back(tx2);

  ITransfersContainer& container = m_consumer.addSubscription(subscription).getContainer();
  ASSERT_TRUE(m_consumer.onNewBlocks(&blocks[0], 1, 2));

  auto outs = container.getTransactionOutputs(tx2->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_TRUE(amountFound(outs, 850));
  ASSERT_TRUE(amountFound(outs, 900));

  auto ignoredOuts = container.getTransactionOutputs(tx1->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(0, ignoredOuts.size());
}

TEST_F(TransfersConsumerTest, onNewBlocks_DifferentTimestamps) {
  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  subscription.syncStart.timestamp = 12345;
  subscription.syncStart.height = 12;

  TestTransactionBuilder b1;
  auto unknownSender = generateAccountKeys();
  b1.addTestInput(1000, unknownSender);
  b1.addTestKeyOutput(123, 1, m_accountKeys);

  TestTransactionBuilder b2;
  b2.addTestInput(10000, unknownSender);
  b2.addTestKeyOutput(850, 2, m_accountKeys);
  b2.addTestKeyOutput(900, 3, m_accountKeys);

  auto tx1 = std::shared_ptr<ITransactionReader>(b1.build().release());
  auto tx2 = std::shared_ptr<ITransactionReader>(b2.build().release());

  CompleteBlock blocks[2];
  blocks[0].transactions.push_back(tx1);
  blocks[0].block = CryptoNote::BlockTemplate();
  blocks[0].block->timestamp = subscription.syncStart.timestamp - 1;

  blocks[1].block = CryptoNote::BlockTemplate();
  blocks[1].block->timestamp = subscription.syncStart.timestamp;
  blocks[1].transactions.push_back(tx2);

  ITransfersContainer& container = m_consumer.addSubscription(subscription).getContainer();
  ASSERT_TRUE(m_consumer.onNewBlocks(&blocks[0], 2, 2));

  auto ignoredOuts = container.getTransactionOutputs(tx1->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(0, ignoredOuts.size());

  auto outs = container.getTransactionOutputs(tx2->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_TRUE(amountFound(outs, 850));
  ASSERT_TRUE(amountFound(outs, 900));
}

TEST_F(TransfersConsumerTest, onNewBlocks_getTransactionOutsGlobalIndicesError) {
  class INodeGlobalIndicesStub: public INodeDummyStub {
  public:
    virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
      std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override {
      callback(std::make_error_code(std::errc::operation_canceled));
    };
  };

  INodeGlobalIndicesStub node;

  TransfersConsumer consumer(m_currency, node, m_logger, m_accountKeys.viewSecretKey);

  auto subscription = getAccountSubscriptionWithSyncStart(m_accountKeys, 1234, 10);

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  addTestKeyOutput(*tx, 900, 2, m_accountKeys);

  CompleteBlock block;
  block.block = CryptoNote::BlockTemplate();
  block.block->timestamp = subscription.syncStart.timestamp;
  block.transactions.push_back(tx);

  consumer.addSubscription(subscription);
  ASSERT_FALSE(consumer.onNewBlocks(&block, static_cast<uint32_t>(subscription.syncStart.height), 1));
}

TEST_F(TransfersConsumerTest, onNewBlocks_updateHeight) {
  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  subscription.syncStart.timestamp = 2131;
  subscription.syncStart.height = 32;
  subscription.transactionSpendableAge = 5;

  auto& container = m_consumer.addSubscription(subscription).getContainer();

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  addTestKeyOutput(*tx, 900, 0, m_accountKeys);

  CompleteBlock block;
  block.block = CryptoNote::BlockTemplate();
  block.block->timestamp = subscription.syncStart.timestamp;
  block.transactions.push_back(tx);

  ASSERT_TRUE(m_consumer.onNewBlocks(&block, static_cast<uint32_t>(subscription.syncStart.height), 1));
  ASSERT_EQ(900, container.balance(ITransfersContainer::IncludeAllLocked));

  std::unique_ptr<CompleteBlock[]> blocks(new CompleteBlock[subscription.transactionSpendableAge]);
  for (uint32_t i = 0; i < subscription.transactionSpendableAge; ++i) {
    blocks[i].block = CryptoNote::BlockTemplate();
    auto tr = createTransaction();
    addTestInput(*tr, 1000);
    addTestKeyOutput(*tr, 100, i + 1, generateAccountKeys());
  }

  ASSERT_TRUE(m_consumer.onNewBlocks(blocks.get(), static_cast<uint32_t>(subscription.syncStart.height + 1), static_cast<uint32_t>(subscription.transactionSpendableAge)));
  ASSERT_EQ(0, container.balance(ITransfersContainer::IncludeAllLocked));
  ASSERT_EQ(900, container.balance(ITransfersContainer::IncludeAllUnlocked));
}

TEST_F(TransfersConsumerTest, onNewBlocks_DifferentSubscribers) {
  auto& container1 = addSubscription().getContainer();

  auto keys = generateAccount();
  auto& container2 = addSubscription(keys).getContainer();

  uint64_t amount1 = 900;
  uint64_t amount2 = 850;

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  addTestKeyOutput(*tx, amount1, 0, m_accountKeys);
  addTestKeyOutput(*tx, amount2, 1, keys);

  CompleteBlock block;
  block.block = CryptoNote::BlockTemplate();
  block.block->timestamp = 0;
  block.transactions.push_back(tx);

  ASSERT_TRUE(m_consumer.onNewBlocks(&block, 0, 1));
  auto outs1 = container1.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(1, outs1.size());
  ASSERT_EQ(amount1, outs1[0].amount);

  auto outs2 = container2.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(1, outs2.size());
  ASSERT_EQ(amount2, outs2[0].amount);
}

TEST_F(TransfersConsumerTest, onNewBlocks_MultisignatureTransaction) {
  auto& container1 = addSubscription().getContainer();

  auto keys = generateAccount();

  auto keys2 = generateAccount();
  auto keys3 = generateAccount();

  uint64_t amount = 900;

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  tx->addOutput(amount, { m_accountKeys.address, keys.address, keys2.address } , 3);
  tx->addOutput(800, { keys.address, keys2.address, keys3.address }, 3);

  CompleteBlock block;
  block.block = CryptoNote::BlockTemplate();
  block.block->timestamp = 0;
  block.transactions.push_back(tx);

  ASSERT_TRUE(m_consumer.onNewBlocks(&block, 0, 1));
  auto outs1 = container1.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(1, outs1.size());
  ASSERT_EQ(amount, outs1[0].amount);
}

TEST_F(TransfersConsumerTest, onNewBlocks_getTransactionOutsGlobalIndicesIsProperlyCalled) {
  class INodeGlobalIndicesStub: public INodeDummyStub {
  public:
    virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
      std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override {
      outsGlobalIndices.push_back(3);
      hash = transactionHash;
      callback(std::error_code());
    };

    Crypto::Hash hash;
  };

  INodeGlobalIndicesStub node;
  TransfersConsumer consumer(m_currency, node, m_logger, m_accountKeys.viewSecretKey);

  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  subscription.syncStart.height = 0;
  subscription.syncStart.timestamp = 0;
  consumer.addSubscription(subscription);

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  addTestKeyOutput(*tx, 900, 2, m_accountKeys);

  CompleteBlock block;
  block.block = CryptoNote::BlockTemplate();
  block.block->timestamp = 0;
  block.transactions.push_back(tx);

  ASSERT_TRUE(consumer.onNewBlocks(&block, 1, 1));
  const Crypto::Hash &hash = tx->getTransactionHash();
  const Crypto::Hash expectedHash = *reinterpret_cast<const Crypto::Hash*>(&hash);
  ASSERT_EQ(expectedHash, node.hash);
}

TEST_F(TransfersConsumerTest, onNewBlocks_getTransactionOutsGlobalIndicesIsNotCalled) {
  class INodeGlobalIndicesStub: public INodeDummyStub {
  public:
    INodeGlobalIndicesStub() : called(false) {};

    virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
      std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override {
      outsGlobalIndices.push_back(3);
      called = true;
      callback(std::error_code());
    };

    bool called;
  };

  INodeGlobalIndicesStub node;
  TransfersConsumer consumer(m_currency, node, m_logger, m_accountKeys.viewSecretKey);

  AccountSubscription subscription = getAccountSubscription(m_accountKeys);
  subscription.syncStart.height = 0;
  subscription.syncStart.timestamp = 0;
  consumer.addSubscription(subscription);

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  addTestKeyOutput(*tx, 900, 2, generateAccount());

  CompleteBlock block;
  block.block = CryptoNote::BlockTemplate();
  block.block->timestamp = 0;
  block.transactions.push_back(tx);
  ASSERT_TRUE(consumer.onNewBlocks(&block, 1, 1));

  ASSERT_FALSE(node.called);
}

TEST_F(TransfersConsumerTest, onNewBlocks_markTransactionConfirmed) {
  auto& container = addSubscription().getContainer();
  
  TestTransactionBuilder b1;
  auto unknownSender = generateAccountKeys();
  b1.addTestInput(10000, unknownSender);
  b1.addTestKeyOutput(10000, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, m_accountKeys);

  auto tx = std::shared_ptr<ITransactionReader>(b1.build().release());

  std::unique_ptr<ITransactionReader> prefix = createTransactionPrefix(convertTx(*tx));
  std::vector<std::unique_ptr<ITransactionReader>> v;
  v.push_back(std::move(prefix));
  m_consumer.onPoolUpdated(v, {});

  auto lockedOuts = container.getTransactionOutputs(tx->getTransactionHash(),
    ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeTypeKey);
  ASSERT_EQ(1, lockedOuts.size());
  ASSERT_EQ(10000, lockedOuts[0].amount);

  CompleteBlock blocks[2];
  blocks[0].block = CryptoNote::BlockTemplate();
  blocks[0].block->timestamp = 0;
  blocks[0].transactions.push_back(tx);
  blocks[1].block = CryptoNote::BlockTemplate();
  blocks[1].block->timestamp = 0;
  blocks[1].transactions.push_back(createTransaction());
  ASSERT_TRUE(m_consumer.onNewBlocks(&blocks[0], 0, 2));

  auto softLockedOuts = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeKeyUnlocked);
  ASSERT_EQ(1, softLockedOuts.size());
  ASSERT_EQ(10000, softLockedOuts[0].amount);
}

class INodeGlobalIndexStub: public INodeDummyStub {
public:

  virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
    std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override {
    outsGlobalIndices.push_back(globalIndex);
    callback(std::error_code());
  };

  uint32_t globalIndex;
};

TEST_F(TransfersConsumerTest, onNewBlocks_checkTransactionOutputInformation) {
  const uint64_t index = 2;

  INodeGlobalIndexStub node;
  TransfersConsumer consumer(m_currency, node, m_logger, m_accountKeys.viewSecretKey);

  node.globalIndex = index;

  auto& container = addSubscription(consumer).getContainer();

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  auto out = addTestKeyOutput(*tx, 10000, index, m_accountKeys);

  CompleteBlock block;
  block.block = CryptoNote::BlockTemplate();
  block.block->timestamp = 0;
  block.transactions.push_back(tx);
  ASSERT_TRUE(consumer.onNewBlocks(&block, 0, 1));

  auto outs = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(1, outs.size());

  auto& o = outs[0];

  ASSERT_EQ(out.type, o.type);
  ASSERT_EQ(out.amount, o.amount);
  ASSERT_EQ(out.outputKey, o.outputKey);
  ASSERT_EQ(out.globalOutputIndex, o.globalOutputIndex);
  ASSERT_EQ(out.outputInTransaction, o.outputInTransaction);
  ASSERT_EQ(out.transactionPublicKey, o.transactionPublicKey);
}

TEST_F(TransfersConsumerTest, onNewBlocks_checkTransactionOutputInformationMultisignature) {
  const uint64_t index = 2;

  INodeGlobalIndexStub node;
  TransfersConsumer consumer(m_currency, node, m_logger, m_accountKeys.viewSecretKey);

  node.globalIndex = index;

  auto& container = addSubscription(consumer).getContainer();

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  size_t txIndex = tx->addOutput(300, { m_accountKeys.address, generateAccountKeys().address}, 2);

  TransactionOutputInformation expectedOut;
  expectedOut.type = TransactionTypes::OutputType::Multisignature;
  expectedOut.amount = 300;
  expectedOut.globalOutputIndex = index;
  expectedOut.outputInTransaction = static_cast<uint32_t>(txIndex);
  expectedOut.transactionPublicKey = tx->getTransactionPublicKey();
  expectedOut.requiredSignatures = 2;

  CompleteBlock block;
  block.block = CryptoNote::BlockTemplate();
  block.block->timestamp = 0;
  block.transactions.push_back(tx);
  ASSERT_TRUE(consumer.onNewBlocks(&block, 0, 1));

  auto outs = container.getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);
  ASSERT_EQ(1, outs.size());

  auto& o = outs[0];
  ASSERT_EQ(expectedOut.type, o.type);
  ASSERT_EQ(expectedOut.amount, o.amount);
  ASSERT_EQ(expectedOut.requiredSignatures, o.requiredSignatures);
  ASSERT_EQ(expectedOut.globalOutputIndex, o.globalOutputIndex);
  ASSERT_EQ(expectedOut.outputInTransaction, o.outputInTransaction);
  ASSERT_EQ(expectedOut.transactionPublicKey, o.transactionPublicKey);
}

TEST_F(TransfersConsumerTest, onNewBlocks_checkTransactionInformation) {
  auto& container = addSubscription().getContainer();

  std::shared_ptr<ITransaction> tx(createTransaction());
  addTestInput(*tx, 10000);
  addTestKeyOutput(*tx, 1000, 2, m_accountKeys);
  Hash paymentId = Crypto::rand<Hash>();
  uint64_t unlockTime = 10;
  tx->setPaymentId(paymentId);
  tx->setUnlockTime(unlockTime);

  CompleteBlock blocks[2];
  blocks[0].block = CryptoNote::BlockTemplate();
  blocks[0].block->timestamp = 0;
  blocks[0].transactions.push_back(createTransaction());

  blocks[1].block = CryptoNote::BlockTemplate();
  blocks[1].block->timestamp = 11;
  blocks[1].transactions.push_back(tx);

  ASSERT_TRUE(m_consumer.onNewBlocks(&blocks[0], 0, 2));

  TransactionInformation info;
  ASSERT_TRUE(container.getTransactionInformation(tx->getTransactionHash(), info));

  ASSERT_EQ(tx->getTransactionHash(), info.transactionHash);
  ASSERT_EQ(tx->getTransactionPublicKey(), info.publicKey);
  ASSERT_EQ(1, info.blockHeight);
  ASSERT_EQ(11, info.timestamp);
  ASSERT_EQ(unlockTime, info.unlockTime);
  ASSERT_EQ(10000, info.totalAmountIn);
  ASSERT_EQ(1000, info.totalAmountOut);
  ASSERT_EQ(paymentId, info.paymentId);
}

TEST_F(TransfersConsumerTest, onNewBlocks_manyBlocks) {
 const size_t blocksCount = 1000;
 const size_t txPerBlock = 10;

 auto& container = addSubscription().getContainer();

 std::vector<CompleteBlock> blocks(blocksCount);

 uint64_t timestamp = 10000;
 uint64_t expectedAmount = 0;
 size_t expectedTransactions = 0;
 uint32_t globalOut = 0;
 size_t blockIdx = 0;

 for (auto& b : blocks) {
   b.block = BlockTemplate();
   b.block->timestamp = timestamp++;
   
   if (++blockIdx % 10 == 0) {
     for (size_t i = 0; i < txPerBlock; ++i) {
       TestTransactionBuilder b1;
       auto unknownSender = generateAccountKeys();
       b1.addTestInput(10000, unknownSender);
       
       if ((i % 3) == 0) {
         b1.addTestKeyOutput(1000, ++globalOut, m_accountKeys);
         b1.addTestKeyOutput(2000, ++globalOut, m_accountKeys);
         expectedAmount += 3000;
         ++expectedTransactions;
       }

       auto tx = std::shared_ptr<ITransactionReader>(b1.build().release());
       b.transactions.push_back(tx);
     }
   }
 }

 ASSERT_TRUE(m_consumer.onNewBlocks(&blocks[0], 0, static_cast<uint32_t>(blocks.size())));

 ASSERT_EQ(expectedTransactions, container.transactionsCount());
 ASSERT_EQ(expectedAmount, container.balance(ITransfersContainer::IncludeAll));
}

TEST_F(TransfersConsumerTest, onPoolUpdated_addTransaction) {
  auto& sub = addSubscription();

  // construct tx
  TestTransactionBuilder b1;
  auto unknownSender = generateAccountKeys();
  b1.addTestInput(10000, unknownSender);
  auto out = b1.addTestKeyOutput(10000, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, m_accountKeys);

  auto tx = std::shared_ptr<ITransactionReader>(b1.build().release());

  std::unique_ptr<ITransactionReader> prefix = createTransactionPrefix(convertTx(*tx));
  std::vector<std::unique_ptr<ITransactionReader>> v;
  v.push_back(std::move(prefix));
  m_consumer.onPoolUpdated(v, {});

  auto outputs = sub.getContainer().getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);

  ASSERT_EQ(1, outputs.size());

  auto& o = outputs[0];

  ASSERT_EQ(out.type, o.type);
  ASSERT_EQ(out.amount, o.amount);
  ASSERT_EQ(out.outputKey, o.outputKey);
  ASSERT_EQ(UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, o.globalOutputIndex);
}

TEST_F(TransfersConsumerTest, onPoolUpdated_addTransactionMultisignature) {
  auto& sub = addSubscription();

  // construct tx with multisignature output
  TestTransactionBuilder b1;
  auto unknownSender = generateAccountKeys();
  b1.addTestInput(10000, unknownSender);
  auto addresses = std::vector<AccountPublicAddress>{ m_accountKeys.address, generateAccountKeys().address };
  b1.addTestMultisignatureOutput(10000, addresses, 1);

  auto tx = std::shared_ptr<ITransactionReader>(b1.build().release());

  std::unique_ptr<ITransactionReader> prefix = createTransactionPrefix(convertTx(*tx));
  std::vector<std::unique_ptr<ITransactionReader>> v;
  v.push_back(std::move(prefix));
  m_consumer.onPoolUpdated(v, {});

  auto outputs = sub.getContainer().getTransactionOutputs(tx->getTransactionHash(), ITransfersContainer::IncludeAll);

  ASSERT_EQ(1, outputs.size());

  auto& o = outputs[0];

  uint64_t amount_;
  MultisignatureOutput out;
  tx->getOutput(0, out, amount_);

  ASSERT_EQ(TransactionTypes::OutputType::Multisignature, o.type);
  ASSERT_EQ(amount_, o.amount);
  ASSERT_EQ(out.requiredSignatureCount, o.requiredSignatures);
  ASSERT_EQ(UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, o.globalOutputIndex);
}


TEST_F(TransfersConsumerTest, onPoolUpdated_addTransactionDoesNotGetsGlobalIndices) {
  addSubscription();
  // construct tx
  auto tx = createTransaction();

  KeyInput input;
  input.amount = 10000;
  input.keyImage = generateKeyImage();
  tx->addInput(input);

  addTestKeyOutput(*tx, 10000, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, m_accountKeys);

  std::unique_ptr<ITransactionReader> prefix = createTransactionPrefix(convertTx(*tx));
  std::vector<std::unique_ptr<ITransactionReader>> v;
  v.push_back(std::move(prefix));
  m_consumer.onPoolUpdated(v, {});

  ASSERT_TRUE(m_node.calls_getTransactionOutsGlobalIndices.empty());
}

TEST_F(TransfersConsumerTest, onPoolUpdated_deleteTransactionNotDeleted) {
  auto& sub = addSubscription();
  TransfersObserver observer;
  sub.addObserver(&observer);

  std::vector<Crypto::Hash> deleted = { 
    Crypto::rand<Crypto::Hash>(), 
    Crypto::rand<Crypto::Hash>() 
  };

  m_consumer.onPoolUpdated({}, deleted);

  ASSERT_EQ(0, observer.deleted.size());
}

TEST_F(TransfersConsumerTest, onPoolUpdated_deleteTransaction) {
  const uint8_t TX_COUNT = 2;
  auto& sub = addSubscription();
  TransfersObserver observer;
  sub.addObserver(&observer);

  std::vector<std::unique_ptr<ITransactionReader>> added;
  std::vector<Crypto::Hash> deleted;

  for (uint8_t i = 0; i < TX_COUNT; ++i) {
    // construct tx
    TestTransactionBuilder b1;
    auto unknownSender = generateAccountKeys();
    b1.addTestInput(10000, unknownSender);
    auto out = b1.addTestKeyOutput(10000, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, m_accountKeys);

    auto tx = std::shared_ptr<ITransactionReader>(b1.build().release());

    std::unique_ptr<ITransactionReader> prefix = createTransactionPrefix(convertTx(*tx));
    added.push_back(std::move(prefix));
    deleted.push_back(added.back()->getTransactionHash());
  }
  
  m_consumer.onPoolUpdated(added, {});
  m_consumer.onPoolUpdated({}, deleted);

  ASSERT_EQ(deleted.size(), observer.deleted.size());
  ASSERT_EQ(deleted, observer.deleted);
}

TEST_F(TransfersConsumerTest, getKnownPoolTxIds_empty) {
  addSubscription();
  const std::unordered_set<Crypto::Hash>& ids = m_consumer.getKnownPoolTxIds();
  ASSERT_TRUE(ids.empty());
}

std::shared_ptr<ITransactionReader> createTransactionTo(const AccountKeys& to, uint64_t amountIn, uint64_t amountOut) {
  TestTransactionBuilder b1;
  auto unknownSender = generateAccountKeys();
  b1.addTestInput(amountIn, unknownSender);
  b1.addTestKeyOutput(amountOut, UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX, to);
  auto tx = std::shared_ptr<ITransactionReader>(b1.build().release());
  return tx;
}

TEST_F(TransfersConsumerTest, getKnownPoolTxIds_returnsUnconfirmed) {
  auto acc1 = generateAccount();
  auto acc2 = generateAccount();

  addSubscription(acc1);
  addSubscription(acc2);

  std::vector<std::shared_ptr<ITransactionReader>> txs;
  txs.push_back(createTransactionTo(acc1, 10000, 10000));
  txs.push_back(createTransactionTo(acc1, 20000, 20000));
  txs.push_back(createTransactionTo(acc2, 30000, 30000));

  std::vector<std::unique_ptr<ITransactionReader>> v;
  v.push_back(createTransactionPrefix(convertTx(*txs[0])));
  v.push_back(createTransactionPrefix(convertTx(*txs[1])));
  v.push_back(createTransactionPrefix(convertTx(*txs[2])));
  m_consumer.onPoolUpdated(v, {});

  const std::unordered_set<Crypto::Hash>& ids = m_consumer.getKnownPoolTxIds();

  ASSERT_EQ(3, ids.size());

  for (int i = 0; i < 3; ++i) {
    auto txhash = txs[i]->getTransactionHash();
    ASSERT_EQ(1, ids.count(txhash));
  }
}


class AutoTimer {
public:

  AutoTimer(bool startNow = true) {
    if (startNow) {
      start();
    }
  }

  void start() {
    startTime = std::chrono::steady_clock::now();
  }

  std::chrono::duration<double> getSeconds() {
    return std::chrono::steady_clock::now() - startTime;
  }

private:

  std::chrono::steady_clock::time_point startTime;

};

class AutoPrintTimer : AutoTimer {
public:
  ~AutoPrintTimer() {
    std::cout << "Running time: " << getSeconds().count() << "s" << std::endl;
  }
};


class TransfersConsumerPerformanceTest : public TransfersConsumerTest {
public:

  void addAndSubscribeAccounts(size_t count) {
    std::cout << "Creating " << count << " accounts" << std::endl;
    for (size_t i = 0; i < count; ++i) {
      recipients.push_back(generateAccount());
      addSubscription(recipients.back());
    }
  }

  size_t generateBlocks(size_t blocksCount, size_t txPerBlock, size_t eachNTx = 3) {
    std::cout << "Generating " << blocksCount << " blocks, " << blocksCount*txPerBlock << " transactions" << std::endl;

    blocks.resize(blocksCount);

    uint64_t timestamp = 10000;
    uint64_t expectedAmount = 0;
    size_t totalTransactions = 0;
    size_t expectedTransactions = 0;
    uint32_t globalOut = 0;

    for (auto& b : blocks) {
      b.transactions.clear();
      b.block = BlockTemplate();
      b.block->timestamp = timestamp++;

      for (size_t i = 0; i < txPerBlock; ++i) {
        auto tx = createTransaction();
        addTestInput(*tx, 10000);
        if ((totalTransactions % eachNTx) == 0) {

          auto& account = recipients[rand() % recipients.size()];

          addTestKeyOutput(*tx, 1000, ++globalOut, account);
          addTestKeyOutput(*tx, 2000, ++globalOut, account);
          addTestKeyOutput(*tx, 3000, ++globalOut, account);
          expectedAmount += 6000;
          ++expectedTransactions;
        }
        tx->getTransactionHash();
        b.transactions.push_back(std::move(tx));
        ++totalTransactions;
      }
    }

    return expectedTransactions;
  }

  std::vector<AccountKeys> recipients;
  std::vector<CompleteBlock> blocks;
};

TEST_F(TransfersConsumerPerformanceTest, DISABLED_memory) {

  addAndSubscribeAccounts(10000);
  size_t txcount = generateBlocks(1000, 50, 1);

  std::cout << "Blocks generated, calling onNewBlocks" << std::endl;

  {
    AutoPrintTimer t;
    ASSERT_TRUE(m_consumer.onNewBlocks(&blocks[0], 0, static_cast<uint32_t>(blocks.size())));
  }

  blocks.clear();
  blocks.shrink_to_fit();

  std::cout << "Transactions to accounts: " << txcount << std::endl;

  char c;
  std::cin >> c;
}


TEST_F(TransfersConsumerPerformanceTest, DISABLED_performanceTest) {

  const size_t blocksCount = 1000;
  const size_t txPerBlock = 10;

  addAndSubscribeAccounts(1000);

  auto expectedTransactions = generateBlocks(blocksCount, txPerBlock, 3);
  auto start = std::chrono::steady_clock::now();
  
  std::cout << "Calling onNewBlocks" << std::endl;

  ASSERT_TRUE(m_consumer.onNewBlocks(&blocks[0], 0, static_cast<uint32_t>(blocks.size())));

  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> dur = end - start;

  std::cout << "Total transactions sent: " << blocksCount * txPerBlock << std::endl;
  std::cout << "Transactions sent to accounts: " << expectedTransactions << std::endl;
  std::cout << "Running time: " << dur.count() << "s" << std::endl;
  std::cout << "Finish" << std::endl;
}
