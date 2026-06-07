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

#include "Globals.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionApi.h"

#include "Transfers/TransfersSynchronizer.h"
#include "Transfers/BlockchainSynchronizer.h"

#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

#include "../IntegrationTestLib/TestWalletLegacy.h"

using namespace CryptoNote;
using namespace Crypto;
using namespace Tests::Common;


class IInterruptable {
public:
  virtual void interrupt() = 0;
};

class WalletLegacyObserver : public IWalletLegacyObserver {
public:
  virtual void actualBalanceUpdated(uint64_t actualBalance) override {
    std::cout << "Actual balance updated = " << currency.formatAmount(actualBalance) << std::endl;
    m_actualBalance = actualBalance;
    m_sem.notify();
  }

  virtual void sendTransactionCompleted(TransactionId transactionId, std::error_code result) override {
    std::cout << "Transaction sent, result = " << result << std::endl;
  }

  std::atomic<uint64_t> m_actualBalance;
  Tests::Common::Semaphore m_sem;
};

class TransactionConsumer : public IBlockchainConsumer {
public:

  TransactionConsumer() {
    syncStart.timestamp = time(nullptr);
    syncStart.height = 0;
  }

  virtual SynchronizationStart getSyncStart() override {
    return syncStart;
  }

  virtual void onBlockchainDetach(uint32_t height) override {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_transactions.lower_bound(height);
    m_transactions.erase(it, m_transactions.end());
  }

  virtual uint32_t onNewBlocks(const CompleteBlock* blocks, uint32_t startHeight, uint32_t count) override {
    std::lock_guard<std::mutex> lk(m_mutex);
    for(size_t i = 0; i < count; ++i) {
      for (const auto& tx : blocks[i].transactions) {
        m_transactions[startHeight + i].insert(tx->getTransactionHash());
      }
    }
    m_cv.notify_all();
    return count;
  }

  bool waitForTransaction(const Hash& txHash) {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (!hasTransaction(txHash)) {
      m_cv.wait_for(lk, std::chrono::seconds(1));
    }
    return true;
  }

  std::error_code onPoolUpdated(const std::vector<std::unique_ptr<ITransactionReader>>& addedTransactions, const std::vector<Crypto::Hash>& deletedTransactions) override {
    //stub
    return std::error_code();
  }

  const std::unordered_set<Crypto::Hash>& getKnownPoolTxIds() const override {
    //stub
    static std::unordered_set<Crypto::Hash> empty;
    return empty;
  }

  std::error_code addUnconfirmedTransaction(const ITransactionReader& /*transaction*/) override {
    throw std::runtime_error("Not implemented");
  }

  void removeUnconfirmedTransaction(const Crypto::Hash& /*transactionHash*/) override {
    throw std::runtime_error("Not implemented");
  }

  virtual void addObserver(IBlockchainConsumerObserver* observer) override {
    //stub
  }

  virtual void removeObserver(IBlockchainConsumerObserver* observer) override {
    //stub
  }

private:

  bool hasTransaction(const Hash& txHash) {
    for (const auto& kv : m_transactions) {
      if (kv.second.count(txHash) > 0)
        return true;
    }
    return false;
  }

  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::map<uint64_t, std::unordered_set<Hash>> m_transactions;
  SynchronizationStart syncStart;
};

class TransfersObserver : public ITransfersObserver, public IInterruptable {
public:
  virtual void onTransactionUpdated(ITransfersSubscription* object, const Hash& transactionHash) override {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      m_transfers.push_back(transactionHash);

      auto key = object->getAddress().spendPublicKey;
      std::string address = Common::toHex(&key, sizeof(key));
      LOG_DEBUG("Transfer to " + address);
    }
    m_cv.notify_all();
  }

  bool waitTransfer() {
    std::unique_lock<std::mutex> lk(m_mutex);
    size_t prevSize = m_transfers.size();

    while (!m_interrupted && m_transfers.size() == prevSize) {
      m_cv.wait_for(lk, std::chrono::seconds(10));
    }

    return true;
  }

  bool waitTransactionTransfer(const Hash& transactionHash) {
    std::unique_lock<std::mutex> lk(m_mutex);

    while (!m_interrupted) {
      auto it = std::find(m_transfers.begin(), m_transfers.end(), transactionHash);
      if (it == m_transfers.end()) {
        m_cv.wait_for(lk, std::chrono::seconds(10));
      } else {
        m_transfers.erase(it);
        break;
      }
    }

    return true;
  }

private:
  bool hasTransaction(const Hash& transactionHash) {
    return std::find(m_transfers.begin(), m_transfers.end(), transactionHash) != m_transfers.end();
  }

  void interrupt() override {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interrupted = true;
    m_cv.notify_all();
  }

private:
  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::vector<Hash> m_transfers;
  bool m_interrupted = false;
};


class AccountGroup {
public:
  enum {
    TRANSACTION_SPENDABLE_AGE = 5
  };

  AccountGroup(ITransfersSynchronizer& sync) :
    m_sync(sync) {}

  void generateAccounts(size_t count) {
    CryptoNote::AccountBase acc;

    while (count--) {
      acc.generate();

      AccountSubscription sub;
      sub.keys = reinterpret_cast<const AccountKeys&>(acc.getAccountKeys());
      sub.syncStart.timestamp = 0;
      sub.syncStart.height = 0;
      sub.transactionSpendableAge = TRANSACTION_SPENDABLE_AGE;

      m_accounts.push_back(sub);
      m_addresses.push_back(currency.accountAddressAsString(acc));
    }
  }

  void subscribeAll() {
    m_observers.reset(new TransfersObserver[m_accounts.size()]);
    for (size_t i = 0; i < m_accounts.size(); ++i) {
      m_sync.addSubscription(m_accounts[i]).addObserver(&m_observers[i]);
    }
  }

  std::vector<AccountPublicAddress> getAddresses() {
    std::vector<AccountPublicAddress> addr;
    for (const auto& acc : m_accounts) {
      addr.push_back(acc.keys.address);
    }
    return addr;
  }

  ITransfersContainer& getTransfers(size_t idx) {
    return m_sync.getSubscription(m_accounts[idx].keys.address)->getContainer();
  }

  std::vector<AccountSubscription> m_accounts;
  std::vector<std::string> m_addresses;
  ITransfersSynchronizer& m_sync;
  std::unique_ptr<TransfersObserver[]> m_observers;
};

template <typename R>
class FutureGuard {
public:
  FutureGuard(std::future<R>&& f) : m_future(std::move(f)) {
  }

  ~FutureGuard() {
    if (m_future.valid()) {
      try {
        m_future.get();
      } catch (...) {
      }
    }
  }

  R get() {
    return m_future.get();
  }

private:
  std::future<R> m_future;
};

class Interrupter {
public:
  Interrupter(IInterruptable& interrpuptable) : m_interrpuptable(interrpuptable) {
  }

  ~Interrupter() {
    if (!m_cancelled) {
      m_interrpuptable.interrupt();
    }
  }

  void cancel() {
    m_cancelled = true;
  }

private:
  IInterruptable& m_interrpuptable;
  bool m_cancelled = false;
};

TEST_F(TransfersTest, base) {
  uint64_t TRANSFER_AMOUNT;
  currency.parseAmount("500000.5", TRANSFER_AMOUNT);

  launchTestnet(2);

  std::unique_ptr<CryptoNote::INode> node1;
  std::unique_ptr<CryptoNote::INode> node2;

  nodeDaemons[0]->makeINode(node1);
  nodeDaemons[1]->makeINode(node2);

  CryptoNote::AccountBase dstAcc;
  dstAcc.generate();

  AccountKeys dstKeys = reinterpret_cast<const AccountKeys&>(dstAcc.getAccountKeys());

  BlockchainSynchronizer blockSync(*node2.get(), logger, currency.genesisBlockHash());
  TransfersSyncronizer transferSync(currency, logger, blockSync, *node2.get());
  TransfersObserver transferObserver;
  WalletLegacyObserver walletObserver;

  AccountSubscription sub;
  sub.syncStart.timestamp = 0;
  sub.syncStart.height = 0;
  sub.keys = dstKeys;
  sub.transactionSpendableAge = 5;

  ITransfersSubscription& transferSub = transferSync.addSubscription(sub);
  ITransfersContainer& transferContainer = transferSub.getContainer();
  transferSub.addObserver(&transferObserver);

  Tests::Common::TestWalletLegacy wallet1(m_dispatcher, m_currency, *node1);
  ASSERT_FALSE(static_cast<bool>(wallet1.init()));
  wallet1.wallet()->addObserver(&walletObserver);
  ASSERT_TRUE(mineBlocks(*nodeDaemons[0], wallet1.accountKeys(), 1));
  ASSERT_TRUE(mineBlocks(*nodeDaemons[0], wallet1.accountKeys(), currency.minedMoneyUnlockWindow()));
  wallet1.waitForSynchronizationToHeight(static_cast<uint32_t>(2 + currency.minedMoneyUnlockWindow()));

  // start syncing and wait for a transfer
  FutureGuard<bool> waitFuture(std::async(std::launch::async, [&transferObserver] { return transferObserver.waitTransfer(); }));
  Interrupter transferObserverInterrupter(transferObserver);
  blockSync.start();

  Hash txId;
  ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(currency.accountAddressAsString(dstAcc), TRANSFER_AMOUNT, txId)));
  ASSERT_TRUE(mineBlocks(*nodeDaemons[0], wallet1.accountKeys(), 1));

  ASSERT_TRUE(waitFuture.get());
  transferObserverInterrupter.cancel();
  std::cout << "Received transfer: " << currency.formatAmount(transferContainer.balance(ITransfersContainer::IncludeAll)) << std::endl;

  ASSERT_EQ(TRANSFER_AMOUNT, transferContainer.balance(ITransfersContainer::IncludeAll));
  ASSERT_GT(transferContainer.getTransactionOutputs(txId, ITransfersContainer::IncludeAll).size(), 0);

  blockSync.stop();
}


std::error_code submitTransaction(INode& node, ITransactionReader& tx) {
  auto data = tx.getTransactionData();

  CryptoNote::Transaction outTx;
  fromBinaryArray(outTx, data);


  LOG_DEBUG("Submitting transaction " + Common::toHex(tx.getTransactionHash().data, 32));

  std::promise<std::error_code> result;
  node.relayTransaction(outTx, [&result](std::error_code ec) { result.set_value(ec); });
  auto err = result.get_future().get();

  if (err) {
    LOG_DEBUG("Error: " + err.message());
  } else {
    LOG_DEBUG("Submitted successfully");
  }

  return err;
}


