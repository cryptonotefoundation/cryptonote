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

#include "EventWaiter.h"
#include "Logging/ConsoleLogger.h"
#include "Transfers/BlockchainSynchronizer.h"
#include "Transfers/TransfersSynchronizer.h"

#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"
#include "TransactionApiHelpers.h"
#include "CryptoNoteCore/TransactionApi.h"

#include "WalletLegacy/WalletLegacy.h"

#include <boost/scoped_array.hpp>

#include <future>
#include <algorithm>

using namespace CryptoNote;

class INodeStubWithPoolTx : public INodeTrivialRefreshStub {
public:
  INodeStubWithPoolTx(TestBlockchainGenerator& generator) : INodeTrivialRefreshStub(generator), detached(false) {}

  void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) override {
    std::unique_lock<std::mutex> lk(mutex);
    relayedTxs.push_back(std::make_pair(this->getLastLocalBlockHeight(), transaction));
    lk.unlock();
    INodeTrivialRefreshStub::relayTransaction(transaction, callback);
  }

  void startAlternativeChain(uint32_t height) override {
    std::unique_lock<std::mutex> lk(mutex);
    INodeTrivialRefreshStub::startAlternativeChain(height);
    detachHeight = height;
    detached = true;
  }


  void getPoolSymmetricDifference(std::vector<Crypto::Hash>&& known_pool_tx_ids, Crypto::Hash known_block_id, bool& is_bc_actual, std::vector<std::unique_ptr<CryptoNote::ITransactionReader>>& new_txs, std::vector<Crypto::Hash>& deleted_tx_ids, const Callback& callback) override
  {
    std::unique_lock<std::mutex> lk(mutex);
    std::sort(relayedTxs.begin(), relayedTxs.end(), [](const std::pair<uint32_t, CryptoNote::Transaction>& val1, const std::pair<uint32_t, CryptoNote::Transaction>& val2)->bool {return val1.first < val2.first; });
    is_bc_actual = true;
    
    if (detached) {
      size_t i = 0;
      for (; i < relayedTxs.size(); ++i) {
        if (relayedTxs[i].first >= detachHeight) {
          break;
        }
      }

      for (; i < relayedTxs.size(); ++i) {
        new_txs.push_back(CryptoNote::createTransactionPrefix(relayedTxs[i].second));
      }
    }

    lk.unlock();
    callback(std::error_code()); 
  };

  
  std::vector<std::pair<uint32_t, CryptoNote::Transaction>> relayedTxs;
  uint32_t detachHeight;
  bool detached;
  std::mutex mutex;
};

class WalletSendObserver : public CryptoNote::IWalletLegacyObserver
{
public:
  WalletSendObserver() {}

  bool waitForSendEnd(std::error_code& ec) {
    if (!sent.wait_for(std::chrono::milliseconds(5000))) return false;
    ec = sendResult;
    return true;
  }

  virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) override {
    sendResult = result;
    sent.notify();
  }

  std::error_code sendResult;
  EventWaiter sent;
};

class DetachTest : public ::testing::Test, public IBlockchainSynchronizerObserver {
public:

  DetachTest() :
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

   // m_transferObservers.reset(new TransfersObserver[m_accounts.size()]);

    for (size_t i = 0; i < m_accounts.size(); ++i) {
      m_subscriptions.push_back(&m_transfersSync.addSubscription(createSubscription(i)));
      //m_subscriptions.back()->addObserver(&m_transferObservers[i]);
    }
  }

  void generateMoneyForAccount(size_t idx) {
    generator.getBlockRewardForAddress(
      reinterpret_cast<const CryptoNote::AccountPublicAddress&>(m_accounts[idx].address));
  }

  std::error_code submitTransaction(ITransactionReader& tx) {
    auto data = tx.getTransactionData();

    CryptoNote::BinaryArray txblob(data.data(), data.data() + data.size());
    CryptoNote::Transaction outTx;
    fromBinaryArray(outTx, data);
    std::promise<std::error_code> result;
    std::future<std::error_code> future = result.get_future();

    m_node.relayTransaction(outTx, [&result](std::error_code ec) {
      std::promise<std::error_code> promise = std::move(result);
      promise.set_value(ec);
    });

    return future.get();
  }

  void synchronizationCompleted(std::error_code result) override {
    decltype(syncCompleted) detachedPromise = std::move(syncCompleted);
    detachedPromise.set_value(result);
  }


protected:
  std::vector<AccountKeys> m_accounts;
  std::vector<ITransfersSubscription*> m_subscriptions;

  Logging::ConsoleLogger m_logger;
  CryptoNote::Currency m_currency;
  TestBlockchainGenerator generator;
  INodeStubWithPoolTx m_node;
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


TEST_F(DetachTest, testBlockchainDetach) {
  uint64_t sendAmount = 70000000000000;
  auto fee = m_currency.minimumFee();

  addMinerAccount();
  addAccounts(2);
  subscribeAccounts();

  generator.generateEmptyBlocks(20);
  
  syncCompleted = std::promise<std::error_code>();
  syncCompletedFuture = syncCompleted.get_future();
  m_sync.addObserver(this);
  m_sync.start();
  syncCompletedFuture.get();
  m_sync.removeObserver(this);

  auto& tc = m_subscriptions[0]->getContainer();
  tc.balance();

  ASSERT_LE(sendAmount, tc.balance(ITransfersContainer::IncludeAllUnlocked));

  auto tx = createMoneyTransfer(sendAmount, fee, m_accounts[0], m_accounts[1].address, tc);
  submitTransaction(*tx);

  syncCompleted = std::promise<std::error_code>();
  syncCompletedFuture = syncCompleted.get_future();
  m_sync.addObserver(this);
  m_node.updateObservers();
  syncCompletedFuture.get();
  m_sync.removeObserver(this);
  auto& tc2 = m_subscriptions[1]->getContainer();

  ASSERT_EQ(sendAmount, tc2.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(0, tc2.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(1, tc2.transactionsCount());

  std::vector<Crypto::Hash> unconfirmed;
  tc2.getUnconfirmedTransactions(unconfirmed);  
  ASSERT_EQ(0, unconfirmed.size());

  m_node.startAlternativeChain(m_node.getLastLocalBlockHeight() - 1);
  generator.generateEmptyBlocks(2);

  syncCompleted = std::promise<std::error_code>();
  syncCompletedFuture = syncCompleted.get_future();
  m_sync.addObserver(this);
  m_node.updateObservers();
  syncCompletedFuture.get();
  m_sync.removeObserver(this);
  auto& tc3 = m_subscriptions[1]->getContainer();

  ASSERT_EQ(sendAmount, tc3.balance(ITransfersContainer::IncludeAll));
  ASSERT_EQ(0, tc3.balance(ITransfersContainer::IncludeAllUnlocked));
  ASSERT_EQ(1, tc3.transactionsCount());

  tc3.getUnconfirmedTransactions(unconfirmed);
  ASSERT_EQ(1, unconfirmed.size());
  ASSERT_EQ(reinterpret_cast<const Hash&>(unconfirmed[0]), tx->getTransactionHash());
  m_sync.stop();
}


struct CompletionWalletObserver : public IWalletLegacyObserver {
  virtual void synchronizationCompleted(std::error_code result) override {
    decltype(syncCompleted) detachedPromise = std::move(syncCompleted);
    detachedPromise.set_value(result);
  }

  std::promise<std::error_code> syncCompleted;
  std::future<std::error_code> syncCompletedFuture;
};


struct WaitForExternalTransactionObserver : public CryptoNote::IWalletLegacyObserver {
public:
  WaitForExternalTransactionObserver() {}
  std::promise<CryptoNote::TransactionId> promise;

  virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId) override {
    decltype(promise) detachedPromise = std::move(promise);
    detachedPromise.set_value(transactionId);
  }

};

TEST_F(DetachTest, testDetachWithWallet) {
  auto fee = m_currency.minimumFee();

  generator.generateEmptyBlocks(5);
  WalletLegacy Alice(m_currency, m_node);
  WalletLegacy Bob(m_currency, m_node);

  CompletionWalletObserver AliceCompleted, BobCompleted;
  AliceCompleted.syncCompleted = std::promise<std::error_code>();
  AliceCompleted.syncCompletedFuture = AliceCompleted.syncCompleted.get_future();
  BobCompleted.syncCompleted = std::promise<std::error_code>();
  BobCompleted.syncCompletedFuture = BobCompleted.syncCompleted.get_future();
  Alice.addObserver(&AliceCompleted);
  Bob.addObserver(&BobCompleted);
  Alice.initAndGenerate("pass");
  Bob.initAndGenerate("pass");
  AliceCompleted.syncCompletedFuture.get();
  BobCompleted.syncCompletedFuture.get();
  Alice.removeObserver(&AliceCompleted);
  Bob.removeObserver(&BobCompleted);


  AccountKeys AliceKeys;
  Alice.getAccountKeys(AliceKeys);

  generator.getBlockRewardForAddress(AliceKeys.address);


  generator.generateEmptyBlocks(10);

  AliceCompleted.syncCompleted = std::promise<std::error_code>();
  AliceCompleted.syncCompletedFuture = AliceCompleted.syncCompleted.get_future();
  BobCompleted.syncCompleted = std::promise<std::error_code>();
  BobCompleted.syncCompletedFuture = BobCompleted.syncCompleted.get_future();
  Alice.addObserver(&AliceCompleted);
  Bob.addObserver(&BobCompleted);

  m_node.updateObservers();

  AliceCompleted.syncCompletedFuture.get();
  BobCompleted.syncCompletedFuture.get();
  Alice.removeObserver(&AliceCompleted);
  Bob.removeObserver(&BobCompleted);


  ASSERT_EQ(0, Alice.pendingBalance());
  ASSERT_NE(0, Alice.actualBalance());

  CryptoNote::WalletLegacyTransfer tr;

  tr.amount = Alice.actualBalance() / 2;
  tr.address = Bob.getAddress();

  WalletSendObserver wso;
  Alice.addObserver(&wso);
  Alice.sendTransaction(tr, fee);
  std::error_code sendError;
  wso.waitForSendEnd(sendError);
  Alice.removeObserver(&wso);
  ASSERT_FALSE(sendError);

  WaitForExternalTransactionObserver etxo;
  auto externalTxFuture = etxo.promise.get_future();
  Bob.addObserver(&etxo);
  AliceCompleted.syncCompleted = std::promise<std::error_code>();
  AliceCompleted.syncCompletedFuture = AliceCompleted.syncCompleted.get_future();
  BobCompleted.syncCompleted = std::promise<std::error_code>();
  BobCompleted.syncCompletedFuture = BobCompleted.syncCompleted.get_future();
  Alice.addObserver(&AliceCompleted);
  Bob.addObserver(&BobCompleted);

  auto expectedTransactionBlockHeight = m_node.getLastLocalBlockHeight();
  generator.generateEmptyBlocks(1); //unlock bob's pending money

  m_node.updateObservers();

  AliceCompleted.syncCompletedFuture.get();
  BobCompleted.syncCompletedFuture.get();
  Alice.removeObserver(&AliceCompleted);
  Bob.removeObserver(&BobCompleted);

  auto txId = externalTxFuture.get();
  Bob.removeObserver(&etxo);

  WalletLegacyTransaction txInfo;

  Bob.getTransaction(txId, txInfo);

  ASSERT_EQ(txInfo.blockHeight, expectedTransactionBlockHeight);
  ASSERT_EQ(txInfo.totalAmount, tr.amount);

  ASSERT_EQ(Bob.pendingBalance(), 0);
  ASSERT_EQ(Bob.actualBalance(), tr.amount);

  m_node.startAlternativeChain(txInfo.blockHeight - 1);
  generator.generateEmptyBlocks(2);

  //sync Bob
  AliceCompleted.syncCompleted = std::promise<std::error_code>();
  AliceCompleted.syncCompletedFuture = AliceCompleted.syncCompleted.get_future();
  BobCompleted.syncCompleted = std::promise<std::error_code>();
  BobCompleted.syncCompletedFuture = BobCompleted.syncCompleted.get_future();
  Alice.addObserver(&AliceCompleted);
  Bob.addObserver(&BobCompleted);

  m_node.updateObservers();

  AliceCompleted.syncCompletedFuture.get();
  BobCompleted.syncCompletedFuture.get();
  Alice.removeObserver(&AliceCompleted);
  Bob.removeObserver(&BobCompleted);

  Bob.getTransaction(txId, txInfo);
  ASSERT_EQ(txInfo.blockHeight, WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
  ASSERT_EQ(txInfo.totalAmount, tr.amount);

  ASSERT_EQ(Bob.pendingBalance(), tr.amount);
  ASSERT_EQ(Bob.actualBalance(), 0);
}
