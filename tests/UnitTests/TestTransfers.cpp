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
#include "Transfers/TransfersSynchronizer.h"

#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"
#include "TransactionApiHelpers.h"
#include "CryptoNoteCore/TransactionApi.h"

#include <boost/scoped_array.hpp>

#include <future>
#include <algorithm>
#include <numeric>

#include <Logging/ConsoleLogger.h>

using namespace CryptoNote;

class TransfersObserver : public ITransfersObserver {
public:

  virtual void onTransactionUpdated(ITransfersSubscription* object, const Hash& transactionHash) override {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_transfers.emplace_back(transactionHash);
  }

  std::vector<Hash> m_transfers;
  std::mutex m_mutex;
};

class TransfersApi : public ::testing::Test, public IBlockchainSynchronizerObserver {
public:

  TransfersApi() :
    m_logger(Logging::ERROR),
    m_currency(CryptoNote::CurrencyBuilder(m_logger).currency()),
    generator(m_currency),
    m_node(generator),
    m_sync(m_node, m_logger, m_currency.genesisBlockHash()),
    m_transfersSync(m_currency, m_logger, m_sync, m_node) {
  }

  void addAccounts(size_t count) {
    while (count--) {
      m_accounts.push_back(generateAccountKeys());
    }
  }

  void addPaymentAccounts(size_t count) {
    KeyPair p1;
    Crypto::generate_keys(p1.publicKey, p1.secretKey);
    auto viewKeys = p1;
    while (count--) {
      Crypto::generate_keys(p1.publicKey, p1.secretKey);
      m_accounts.push_back(accountKeysFromKeypairs(viewKeys, p1));
    }
  }

  void addMinerAccount() {
    m_accounts.push_back(reinterpret_cast<const AccountKeys&>(generator.getMinerAccount()));
  }

  AccountSubscription createSubscription(size_t acc, uint64_t timestamp = 0) {
    const auto& keys = m_accounts[acc];
    AccountSubscription sub;
    sub.keys = keys;
    sub.syncStart.timestamp = timestamp;
    sub.syncStart.height = 0;
    sub.transactionSpendableAge = 5;
    return sub;
  }

  void subscribeAccounts() {

    m_transferObservers.reset(new TransfersObserver[m_accounts.size()]);
      
    for (size_t i = 0; i < m_accounts.size(); ++i) {
      m_subscriptions.push_back(&m_transfersSync.addSubscription(createSubscription(i)));
      m_subscriptions.back()->addObserver(&m_transferObservers[i]);
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
    m_sync.lastKnownBlockHeightUpdated(0);
    syncCompletedFuture.get();
    m_sync.removeObserver(this);
  }

  void synchronizationCompleted(std::error_code result) override {
    decltype(syncCompleted) detachedPromise = std::move(syncCompleted);
    detachedPromise.set_value(result);
  }

  void generateMoneyForAccount(size_t idx) {
    generator.getBlockRewardForAddress(
      reinterpret_cast<const CryptoNote::AccountPublicAddress&>(m_accounts[idx].address));
  }

  std::error_code submitTransaction(ITransactionReader& tx) {
    auto data = tx.getTransactionData();
    Transaction outTx;
    CryptoNote::fromBinaryArray(outTx, data);

    std::promise<std::error_code> result;
    m_node.relayTransaction(outTx, [&result](std::error_code ec) {
      std::promise<std::error_code> detachedPromise = std::move(result);
      detachedPromise.set_value(ec);
    });
    return result.get_future().get();
  }

protected:

  boost::scoped_array<TransfersObserver> m_transferObservers;
  std::vector<AccountKeys> m_accounts;
  std::vector<ITransfersSubscription*> m_subscriptions;

  Logging::ConsoleLogger m_logger;
  CryptoNote::Currency m_currency;
  TestBlockchainGenerator generator;
  INodeTrivialRefreshStub m_node;
  BlockchainSynchronizer m_sync;
  TransfersSyncronizer m_transfersSync;

  std::promise<std::error_code> syncCompleted;
  std::future<std::error_code> syncCompletedFuture;
};


namespace CryptoNote {
  inline bool operator == (const TransactionOutputInformation& t1, const TransactionOutputInformation& t2) {
    return 
      t1.type == t2.type &&
      t1.amount == t2.amount &&
      t1.outputInTransaction == t2.outputInTransaction &&
      t1.transactionPublicKey == t2.transactionPublicKey;
  }
}


TEST_F(TransfersApi, testSubscriptions) {
  addAccounts(1);

  m_transfersSync.addSubscription(createSubscription(0));

  std::vector<AccountPublicAddress> subscriptions;

  m_transfersSync.getSubscriptions(subscriptions);

  const auto& addr = m_accounts[0].address;

  ASSERT_EQ(1, subscriptions.size());
  ASSERT_EQ(addr, subscriptions[0]);
  ASSERT_TRUE(m_transfersSync.getSubscription(addr) != 0);
  ASSERT_TRUE(m_transfersSync.removeSubscription(addr));

  subscriptions.clear();
  m_transfersSync.getSubscriptions(subscriptions);
  ASSERT_EQ(0, subscriptions.size());
}

TEST_F(TransfersApi, syncOneBlock) { 
  addAccounts(2);
  subscribeAccounts();

  generator.getBlockRewardForAddress(m_accounts[0].address);
  generator.generateEmptyBlocks(15);

  startSync();

  auto& tc1 = m_transfersSync.getSubscription(m_accounts[0].address)->getContainer();
  auto& tc2 = m_transfersSync.getSubscription(m_accounts[1].address)->getContainer();
  
  ASSERT_NE(&tc1, &tc2);

  ASSERT_GT(tc1.balance(ITransfersContainer::IncludeAll), 0);
  ASSERT_GT(tc1.transfersCount(), 0);
  ASSERT_EQ(0, tc2.transfersCount());
}


TEST_F(TransfersApi, syncMinerAcc) {
  addMinerAccount();
  subscribeAccounts();
  
  generator.generateEmptyBlocks(10);

  startSync();

  ASSERT_NE(0, m_subscriptions[0]->getContainer().transfersCount());
}


namespace {
  std::unique_ptr<ITransaction> createMoneyTransfer(
    uint64_t amount,
    uint64_t fee,
    const AccountKeys& senderKeys,
  const AccountPublicAddress& reciever,
  ITransfersContainer& tc) {

  std::vector<TransactionOutputInformation> transfers;
  tc.getOutputs(transfers, ITransfersContainer::IncludeAllUnlocked);

  auto tx = createTransaction();

  std::vector<std::pair<TransactionTypes::InputKeyInfo, KeyPair>> inputs;

  uint64_t foundMoney = 0;

  for (const auto& t : transfers) {
    TransactionTypes::InputKeyInfo info;

    info.amount = t.amount;

    TransactionTypes::GlobalOutput globalOut;
    globalOut.outputIndex = t.globalOutputIndex;
    globalOut.targetKey = t.outputKey;
    info.outputs.push_back(globalOut);

    info.realOutput.outputInTransaction = t.outputInTransaction;
    info.realOutput.transactionIndex = 0;
    info.realOutput.transactionPublicKey = t.transactionPublicKey;

    KeyPair kp;
    tx->addInput(senderKeys, info, kp);

    inputs.push_back(std::make_pair(info, kp));

    foundMoney += info.amount;

    if (foundMoney >= amount + fee) {
      break;
  }
    }

  // output to reciever
  tx->addOutput(amount, reciever);
  // change
  uint64_t change = foundMoney - amount - fee;
  if (change) {
    tx->addOutput(change, senderKeys.address);
  }

  for (size_t inputIdx = 0; inputIdx < inputs.size(); ++inputIdx) {
    tx->signInputKey(inputIdx, inputs[inputIdx].first, inputs[inputIdx].second);
  }

  return tx;
  }
}

TEST_F(TransfersApi, moveMoney) {
  addMinerAccount();
  addAccounts(2);
  subscribeAccounts();

  generator.generateEmptyBlocks(2 * m_currency.minedMoneyUnlockWindow());

  // sendAmount is an even number
  auto& transaction = generator.getBlockchain()[1].baseTransaction;
  uint64_t sendAmount = std::accumulate(
      transaction.outputs.begin(), transaction.outputs.end(), UINT64_C(0),
      [](uint64_t sum, const decltype(transaction.outputs)::value_type& output) { return sum + output.amount; });
  sendAmount = (sendAmount / 4) * 2;
  auto fee = m_currency.minimumFee();

  startSync();

  auto& tc0 = m_subscriptions[0]->getContainer();

  ASSERT_LE(sendAmount, tc0.balance(ITransfersContainer::IncludeAllUnlocked));

  auto tx = createMoneyTransfer(sendAmount, fee, m_accounts[0], m_accounts[1].address, tc0);
  submitTransaction(*tx);

  refreshSync();

  ASSERT_EQ(1, m_transferObservers[1].m_transfers.size());
  ASSERT_EQ(tx->getTransactionHash(), m_transferObservers[1].m_transfers[0]);

  auto& tc1 = m_subscriptions[1]->getContainer();

  ASSERT_EQ(sendAmount, tc1.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(0, tc1.balance(ITransfersContainer::IncludeAllUnlocked));

  generator.generateEmptyBlocks(m_currency.minedMoneyUnlockWindow()); // unlock money

  refreshSync();

  ASSERT_EQ(sendAmount, tc1.balance(ITransfersContainer::IncludeAllUnlocked));

  auto tx2 = createMoneyTransfer(sendAmount / 2, fee, m_accounts[1], m_accounts[2].address, tc1);
  submitTransaction(*tx2);

  refreshSync();

  ASSERT_EQ(2, m_transferObservers[1].m_transfers.size());
  ASSERT_EQ(tx2->getTransactionHash(), m_transferObservers[1].m_transfers.back());

  ASSERT_EQ(sendAmount / 2 - fee, m_subscriptions[1]->getContainer().balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(sendAmount / 2, m_subscriptions[2]->getContainer().balance(ITransfersContainer::IncludeAll));
}


struct lessOutKey {
  bool operator()(const TransactionOutputInformation& t1, const TransactionOutputInformation& t2) {
    return std::hash<PublicKey>()(t1.outputKey) <  std::hash<PublicKey>()(t2.outputKey);
  }
};

bool compareStates(TransfersSyncronizer& sync1, TransfersSyncronizer& sync2) {

  std::vector<AccountPublicAddress> subs;
  sync1.getSubscriptions(subs);

  for (const auto& s : subs) {
    auto& tc1 = sync1.getSubscription(s)->getContainer();

    if (sync2.getSubscription(s) == nullptr)
      return false;

    auto& tc2 = sync2.getSubscription(s)->getContainer();

    std::vector<TransactionOutputInformation> out1;
    std::vector<TransactionOutputInformation> out2;

    tc1.getOutputs(out1);
    tc2.getOutputs(out2);

    std::sort(out1.begin(), out1.end(), lessOutKey());
    std::sort(out2.begin(), out2.end(), lessOutKey());

    if (out1 != out2)
      return false;

  }

  return true;
}

TEST_F(TransfersApi, state) {
  addMinerAccount();
  subscribeAccounts();

  generator.generateEmptyBlocks(20);

  startSync();

  m_sync.stop();
  std::stringstream memstm;
  m_transfersSync.save(memstm);
  m_sync.start();

  BlockchainSynchronizer bsync2(m_node, m_logger, m_currency.genesisBlockHash());
  TransfersSyncronizer sync2(m_currency, m_logger, bsync2, m_node);

  for (size_t i = 0; i < m_accounts.size(); ++i) {
    sync2.addSubscription(createSubscription(i));
  }

  sync2.load(memstm);

  // compare transfers
  ASSERT_TRUE(compareStates(m_transfersSync, sync2));

  // generate more blocks
  generator.generateEmptyBlocks(10);

  refreshSync();

  syncCompleted = std::promise<std::error_code>();
  syncCompletedFuture = syncCompleted.get_future();
  bsync2.addObserver(this);
  bsync2.start();
  syncCompletedFuture.get();
  bsync2.removeObserver(this);

  // check again
  ASSERT_TRUE(compareStates(m_transfersSync, sync2));
}

TEST_F(TransfersApi, sameTrackingKey) {

  size_t offset = 2; // miner account + ordinary account
  size_t paymentAddresses = 1000;
  size_t payments = 10;

  addMinerAccount();
  addAccounts(1);
  addPaymentAccounts(paymentAddresses);

  subscribeAccounts();

  for (size_t i = 0; i < payments; ++i) {
    generateMoneyForAccount(i + offset);
  }

  startSync();

  for (size_t i = 0; i < payments; ++i) {
    auto sub = m_subscriptions[offset + i];
    EXPECT_NE(0, sub->getContainer().balance(ITransfersContainer::IncludeAll));
  }

}
