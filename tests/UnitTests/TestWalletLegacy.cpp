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

#include <future>
#include <chrono>
#include <array>

#include "EventWaiter.h"
#include "INode.h"
#include "WalletLegacy/WalletLegacy.h"
#include "WalletLegacy/WalletHelper.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNote.h"

#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"
#include <Logging/ConsoleLogger.h>

class TrivialWalletObserver : public CryptoNote::IWalletLegacyObserver
{
public:
  TrivialWalletObserver() : actualBalance(0), pendingBalance(0) {}

  bool waitForSyncEnd() {
    return synced.wait_for(std::chrono::milliseconds(3000));
  }

  bool waitForSendEnd(std::error_code& ec) {
    if (!sent.wait_for(std::chrono::milliseconds(5000))) return false;
    ec = sendResult;
    return true;
  }

  bool waitForSaveEnd(std::error_code& ec) {
    if (!saved.wait_for(std::chrono::milliseconds(5000))) return false;
    ec = saveResult;
    return true;
  }

  bool waitForLoadEnd(std::error_code& ec) {
    if (!loaden.wait_for(std::chrono::milliseconds(5000))) return false;
    ec = loadResult;
    return true;
  }

  virtual void synchronizationCompleted(std::error_code result) override {
    synced.notify();
  }

  virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) override {
    sendResult = result;
    sent.notify();
  }

  virtual void saveCompleted(std::error_code result) override {
    saveResult = result;
    saved.notify();
  }

  virtual void initCompleted(std::error_code result) override {
    loadResult = result;
    loaden.notify();
  }

  virtual void actualBalanceUpdated(uint64_t actualBalance) override {
    //    std::cout << "actual balance: " << actualBalance << std::endl;
    this->actualBalance = actualBalance;
  }

  virtual void pendingBalanceUpdated(uint64_t pendingBalance) override {
//    std::cout << "pending balance: " << pendingBalance << std::endl;
    this->pendingBalance = pendingBalance;
  }

  std::error_code sendResult;
  std::error_code saveResult;
  std::error_code loadResult;

  std::atomic<uint64_t> actualBalance;
  std::atomic<uint64_t> pendingBalance;

  EventWaiter synced;
  EventWaiter saved;
  EventWaiter loaden;
  EventWaiter sent;
};

struct SaveOnInitWalletObserver: public CryptoNote::IWalletLegacyObserver {
  SaveOnInitWalletObserver(CryptoNote::WalletLegacy* wallet) : wallet(wallet) {
  };
  virtual ~SaveOnInitWalletObserver() {}

  virtual void initCompleted(std::error_code result) override {
    wallet->save(stream, true, true);
  }

  CryptoNote::WalletLegacy* wallet;
  std::stringstream stream;
};

static const uint64_t TEST_BLOCK_REWARD = 70368744177663;

CryptoNote::TransactionId TransferMoney(CryptoNote::WalletLegacy& from, CryptoNote::WalletLegacy& to, int64_t amount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "") {
  CryptoNote::WalletLegacyTransfer transfer;
  transfer.amount = amount;
  transfer.address = to.getAddress();

  return from.sendTransaction(transfer, fee, extra, mixIn);
}

void WaitWalletSync(TrivialWalletObserver* observer) {
  ASSERT_TRUE(observer->waitForSyncEnd());
}

void WaitWalletSend(TrivialWalletObserver* observer) {
  std::error_code ec;
  ASSERT_TRUE(observer->waitForSendEnd(ec));
}

void WaitWalletSend(TrivialWalletObserver* observer, std::error_code& ec) {
  ASSERT_TRUE(observer->waitForSendEnd(ec));
}

void WaitWalletSave(TrivialWalletObserver* observer) {
  std::error_code ec;

  ASSERT_TRUE(observer->waitForSaveEnd(ec));
  EXPECT_FALSE(ec);
}

void WaitWalletLoad(TrivialWalletObserver* observer) {
  std::error_code ec;
  
  ASSERT_TRUE(observer->waitForLoadEnd(ec));
  EXPECT_FALSE(ec);  
}

class WalletLegacyApi : public ::testing::Test
{
public:
  WalletLegacyApi() : m_currency(CryptoNote::CurrencyBuilder(m_logger).currency()), generator(m_currency) {
  }

  void SetUp() override;

protected:
  void prepareAliceWallet();
  void prepareBobWallet();
  void prepareCarolWallet();

  void GetOneBlockReward(CryptoNote::WalletLegacy& wallet);
  void GetOneBlockReward(CryptoNote::WalletLegacy& wallet, const CryptoNote::Currency& currency, TestBlockchainGenerator& blockchainGenerator);
  void GetOneBlockRewardAndUnlock(CryptoNote::WalletLegacy& wallet, TrivialWalletObserver& observer, INodeTrivialRefreshStub& node,
                                  const CryptoNote::Currency& currency, TestBlockchainGenerator& blockchainGenerator);

  void TestSendMoney(int64_t transferAmount, uint64_t fee, uint64_t mixIn = 0, const std::string& extra = "");
  void performTransferWithErrorTx(const std::array<int64_t, 5>& amounts, uint64_t fee);


  Logging::ConsoleLogger m_logger;
  CryptoNote::Currency m_currency;

  TestBlockchainGenerator generator;

  std::shared_ptr<TrivialWalletObserver> aliceWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> aliceNode;
  std::shared_ptr<CryptoNote::WalletLegacy> alice;

  std::shared_ptr<TrivialWalletObserver> bobWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> bobNode;
  std::shared_ptr<CryptoNote::WalletLegacy> bob;

  std::shared_ptr<TrivialWalletObserver> carolWalletObserver;
  std::shared_ptr<INodeTrivialRefreshStub> carolNode;
  std::shared_ptr<CryptoNote::WalletLegacy> carol;
};

void WalletLegacyApi::SetUp() {
  prepareAliceWallet();
  generator.generateEmptyBlocks(3);
}

void WalletLegacyApi::prepareAliceWallet() {
  decltype(aliceNode) newNode(new INodeTrivialRefreshStub(generator));

  alice.reset(new CryptoNote::WalletLegacy(m_currency, *newNode));
  aliceNode = newNode;

  aliceWalletObserver.reset(new TrivialWalletObserver());
  alice->addObserver(aliceWalletObserver.get());
}

void WalletLegacyApi::prepareBobWallet() {
  bobNode.reset(new INodeTrivialRefreshStub(generator));
  bobWalletObserver.reset(new TrivialWalletObserver());

  bob.reset(new CryptoNote::WalletLegacy(m_currency, *bobNode));
  bob->addObserver(bobWalletObserver.get());
}

void WalletLegacyApi::prepareCarolWallet() {
  carolNode.reset(new INodeTrivialRefreshStub(generator));
  carolWalletObserver.reset(new TrivialWalletObserver());

  carol.reset(new CryptoNote::WalletLegacy(m_currency, *carolNode));
  carol->addObserver(carolWalletObserver.get());
}

void WalletLegacyApi::GetOneBlockReward(CryptoNote::WalletLegacy& wallet) {
  GetOneBlockReward(wallet, m_currency, generator);
}

void WalletLegacyApi::GetOneBlockReward(CryptoNote::WalletLegacy& wallet, const CryptoNote::Currency& currency, TestBlockchainGenerator& blockchainGenerator) {
  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(currency.parseAccountAddressString(wallet.getAddress(), address));
  blockchainGenerator.getBlockRewardForAddress(address);
}

void WalletLegacyApi::GetOneBlockRewardAndUnlock(CryptoNote::WalletLegacy& wallet, TrivialWalletObserver& observer, INodeTrivialRefreshStub& node,
                                                 const CryptoNote::Currency& currency, TestBlockchainGenerator& blockchainGenerator) {
  GetOneBlockReward(wallet, currency, blockchainGenerator);
  blockchainGenerator.generateEmptyBlocks(10);
  node.updateObservers();
  WaitWalletSync(&observer);
}

void WalletLegacyApi::performTransferWithErrorTx(const std::array<int64_t, 5>& amounts, uint64_t fee) {
  std::vector<CryptoNote::WalletLegacyTransfer> trs;
  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = amounts[0];
  trs.push_back(tr);

  tr.address = bob->getAddress();
  tr.amount = amounts[1];
  trs.push_back(tr);

  tr.address = carol->getAddress();
  tr.amount = amounts[2];
  trs.push_back(tr);

  aliceNode->setNextTransactionError();
  alice->sendTransaction(trs, fee);

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), result));
  ASSERT_NE(result.value(), 0);

  trs.clear();

  tr.address = bob->getAddress();
  tr.amount = amounts[3];
  trs.push_back(tr);

  tr.address = carol->getAddress();
  tr.amount = amounts[4];
  trs.push_back(tr);

  alice->sendTransaction(trs, fee);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);
}

void WalletLegacyApi::TestSendMoney(int64_t transferAmount, uint64_t fee, uint64_t mixIn, const std::string& extra) {
  prepareBobWallet();

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  //unlock Alice's money
  generator.generateEmptyBlocks(10);
  uint64_t expectedBalance = TEST_BLOCK_REWARD;

  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(0, alice->pendingBalance());
  EXPECT_EQ(expectedBalance, alice->actualBalance());

  EXPECT_EQ(expectedBalance, aliceWalletObserver->actualBalance);
  EXPECT_EQ(0, aliceWalletObserver->pendingBalance);

  bob->initAndGenerate("pass2");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, transferAmount, fee, mixIn, ""));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  EXPECT_EQ(0, bob->pendingBalance());
  EXPECT_EQ(transferAmount, bob->actualBalance());

  EXPECT_EQ(0, alice->pendingBalance());
  EXPECT_EQ(expectedBalance - transferAmount - fee, alice->actualBalance());

  alice->shutdown();
  bob->shutdown();
}

void WaitWalletLoad(TrivialWalletObserver* observer, std::error_code& ec) {
  ASSERT_TRUE(observer->waitForLoadEnd(ec));
}

TEST_F(WalletLegacyApi, initAndSave) {
  SaveOnInitWalletObserver saveOnInit(alice.get());
  alice->addObserver(&saveOnInit);
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));
  alice->shutdown();
}

TEST_F(WalletLegacyApi, refreshWithMoney) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(alice->actualBalance(), 0);
  ASSERT_EQ(alice->pendingBalance(), 0);

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getBlockRewardForAddress(address);

  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->actualBalance(), 0);
  EXPECT_EQ(alice->pendingBalance(), TEST_BLOCK_REWARD);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, initWithMoney) {
  std::stringstream archive;

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  alice->save(archive, true, true);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  ASSERT_EQ(alice->actualBalance(), 0);
  ASSERT_EQ(alice->pendingBalance(), 0);

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));

  alice->shutdown();

  generator.getBlockRewardForAddress(address);

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->actualBalance(), 0);
  EXPECT_EQ(alice->pendingBalance(), TEST_BLOCK_REWARD);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, TransactionsAndTransfersAfterSend) {
  prepareBobWallet();
  prepareCarolWallet();

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->getTransactionCount(), 0);
  EXPECT_EQ(alice->getTransferCount(), 0);

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  //unblock Alice's money
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(alice->getTransactionCount(), 1);

  bob->initAndGenerate("pass2");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t fee = 100000;
  int64_t amount1 = 1230000;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, amount1, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  int64_t amount2 = 1234500;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, amount2, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  int64_t amount3 = 1234567;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *bob, amount3, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  carol->initAndGenerate("pass3");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  int64_t amount4 = 1020304;
  ASSERT_NO_FATAL_FAILURE(TransferMoney(*alice, *carol, amount4, fee, 0));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  EXPECT_EQ(alice->getTransactionCount(), 5);

  CryptoNote::WalletLegacyTransaction tx;

  //Transaction with id = 0 is tested in getTransactionSuccess
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount1 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(2, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount2 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 1);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(3, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount3 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 2);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransaction(4, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount4 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.isCoinbase, false);
  EXPECT_EQ(tx.firstTransferId, 3);
  EXPECT_EQ(tx.transferCount, 1);

  //Now checking transfers
  CryptoNote::WalletLegacyTransfer tr;
  ASSERT_TRUE(alice->getTransfer(0, tr));
  EXPECT_EQ(tr.amount, amount1);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(1, tr));
  EXPECT_EQ(tr.amount, amount2);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(2, tr));
  EXPECT_EQ(tr.amount, amount3);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(3, tr));
  EXPECT_EQ(tr.amount, amount4);
  EXPECT_EQ(tr.address, carol->getAddress());

  EXPECT_EQ(alice->findTransactionByTransferId(0), 1);
  EXPECT_EQ(alice->findTransactionByTransferId(1), 2);
  EXPECT_EQ(alice->findTransactionByTransferId(2), 3);
  EXPECT_EQ(alice->findTransactionByTransferId(3), 4);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, saveAndLoadCacheDetails) {
  prepareBobWallet();
  prepareCarolWallet();

  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  //unblock Alice's money
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bob->initAndGenerate("pass2");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carol->initAndGenerate("pass3");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  uint64_t fee = 1000000;
  int64_t amount1 = 1234567;
  int64_t amount2 = 1020304;
  int64_t amount3 = 2030405;

  std::vector<CryptoNote::WalletLegacyTransfer> trs;
  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = amount1;
  trs.push_back(tr);

  tr.address = bob->getAddress();
  tr.amount = amount2;
  trs.push_back(tr);

  alice->sendTransaction(trs, fee, "", 0, 0);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  trs.clear();
  tr.address = carol->getAddress();
  tr.amount = amount3;
  trs.push_back(tr);

  alice->sendTransaction(trs, fee, "", 0, 0);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  std::stringstream archive;
  alice->save(archive, true, true);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  auto prevActualBalance = alice->actualBalance();
  auto prevPendingBalance = alice->pendingBalance();

  alice->shutdown();

  prepareAliceWallet();

  alice->initAndLoad(archive, "pass");
  std::error_code ec;

  WaitWalletLoad(aliceWalletObserver.get(), ec);
  ASSERT_FALSE(ec);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(alice->getTransactionCount(), 3);
  ASSERT_EQ(alice->getTransferCount(), 3);

  EXPECT_EQ(prevActualBalance, alice->actualBalance());
  EXPECT_EQ(prevPendingBalance, alice->pendingBalance());

  CryptoNote::WalletLegacyTransaction tx;
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount1 + amount2 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 2);

  ASSERT_TRUE(alice->getTransaction(2, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amount3 + fee));
  EXPECT_EQ(tx.fee, fee);
  EXPECT_EQ(tx.firstTransferId, 2);
  EXPECT_EQ(tx.transferCount, 1);

  ASSERT_TRUE(alice->getTransfer(0, tr));
  EXPECT_EQ(tr.address, bob->getAddress());
  EXPECT_EQ(tr.amount, amount1);

  ASSERT_TRUE(alice->getTransfer(1, tr));
  EXPECT_EQ(tr.address, bob->getAddress());
  EXPECT_EQ(tr.amount, amount2);

  ASSERT_TRUE(alice->getTransfer(2, tr));
  EXPECT_EQ(tr.address, carol->getAddress());
  EXPECT_EQ(tr.amount, amount3);

  EXPECT_EQ(alice->findTransactionByTransferId(0), 1);
  EXPECT_EQ(alice->findTransactionByTransferId(1), 1);
  EXPECT_EQ(alice->findTransactionByTransferId(2), 2);

  alice->shutdown();
  carol->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, sendMoneySuccessNoMixin) {
  ASSERT_NO_FATAL_FAILURE(TestSendMoney(10000000, 1000000, 0));
}

TEST_F(WalletLegacyApi, sendMoneySuccessWithMixin) {
  ASSERT_NO_FATAL_FAILURE(TestSendMoney(10000000, 1000000, 3));
}

TEST_F(WalletLegacyApi, getTransactionSuccess) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransaction tx;

  ASSERT_EQ(alice->getTransactionCount(), 1);
  ASSERT_TRUE(alice->getTransaction(0, tx));

  EXPECT_EQ(tx.firstTransferId, CryptoNote::WALLET_LEGACY_INVALID_TRANSFER_ID);
  EXPECT_EQ(tx.transferCount, 0);
  EXPECT_EQ(tx.totalAmount, TEST_BLOCK_REWARD);
  EXPECT_EQ(tx.fee, 0);
  EXPECT_EQ(tx.isCoinbase, false);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, getTransactionFailure) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransaction tx;

  ASSERT_EQ(alice->getTransactionCount(), 0);
  ASSERT_FALSE(alice->getTransaction(0, tx));

  alice->shutdown();
}

TEST_F(WalletLegacyApi, useNotInitializedObject) {
  EXPECT_THROW(alice->pendingBalance(), std::system_error);
  EXPECT_THROW(alice->actualBalance(), std::system_error);
  EXPECT_THROW(alice->getTransactionCount(), std::system_error);
  EXPECT_THROW(alice->getTransferCount(), std::system_error);
  EXPECT_THROW(alice->getAddress(), std::system_error);

  std::stringstream archive;
  EXPECT_THROW(alice->save(archive, true, true), std::system_error);

  EXPECT_THROW(alice->findTransactionByTransferId(1), std::system_error);

  CryptoNote::WalletLegacyTransaction tx;
  CryptoNote::WalletLegacyTransfer tr;
  EXPECT_THROW(alice->getTransaction(1, tx), std::system_error);
  EXPECT_THROW(alice->getTransfer(2, tr), std::system_error);

  tr.address = "lslslslslslsls";
  tr.amount = 1000000;
  EXPECT_THROW(alice->sendTransaction(tr, 300201), std::system_error);

  std::vector<CryptoNote::WalletLegacyTransfer> trs;
  trs.push_back(tr);
  EXPECT_THROW(alice->sendTransaction(trs, 329293), std::system_error);
}

TEST_F(WalletLegacyApi, sendWrongAmount) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransfer tr;
  tr.address = "1234567890qwertasdfgzxcvbyuiophjklnm";
  tr.amount = 1;

  EXPECT_THROW(alice->sendTransaction(tr, 1), std::system_error);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, wrongPassword) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  std::stringstream archive;
  alice->save(archive, true, false);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  alice->shutdown();

  prepareAliceWallet();
  alice->initAndLoad(archive, "wrongpass");

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  EXPECT_EQ(result.value(), CryptoNote::error::WRONG_PASSWORD);
}

TEST_F(WalletLegacyApi, detachBlockchain) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));

  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  aliceNode->startAlternativeChain(3);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(0, alice->actualBalance());
  EXPECT_EQ(0, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, saveAndLoad) {
  alice->initAndGenerate("pass");

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);

  std::stringstream archive;
  ASSERT_NO_FATAL_FAILURE(alice->save(archive));

  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);
}

TEST_F(WalletLegacyApi, saveAndLoadErroneousTxsCacheDetails) {
  prepareBobWallet();
  prepareCarolWallet();

  std::error_code result;

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carol->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  std::array<int64_t, 5> amounts;
  amounts[0] = 1234567;
  amounts[1] = 1345678;
  amounts[2] = 1456789;
  amounts[3] = 1567890;
  amounts[4] = 1678901;
  uint64_t fee = 10000;

  ASSERT_NO_FATAL_FAILURE(performTransferWithErrorTx(amounts, fee));

  std::stringstream archive;
  alice->save(archive);

  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);

  EXPECT_EQ(alice->getTransactionCount(), 2);
  EXPECT_EQ(alice->getTransferCount(), 2);

  CryptoNote::WalletLegacyTransaction tx;
  ASSERT_TRUE(alice->getTransaction(1, tx));
  EXPECT_EQ(tx.totalAmount, -static_cast<int64_t>(amounts[3] + amounts[4] + fee));
  EXPECT_EQ(tx.firstTransferId, 0);
  EXPECT_EQ(tx.transferCount, 2);

  CryptoNote::WalletLegacyTransfer tr;
  ASSERT_TRUE(alice->getTransfer(0, tr));
  EXPECT_EQ(tr.amount, amounts[3]);
  EXPECT_EQ(tr.address, bob->getAddress());

  ASSERT_TRUE(alice->getTransfer(1, tr));
  EXPECT_EQ(tr.amount, amounts[4]);
  EXPECT_EQ(tr.address, carol->getAddress());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, saveAndLoadErroneousTxsCacheNoDetails) {
  prepareBobWallet();
  prepareCarolWallet();

  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_NO_FATAL_FAILURE(GetOneBlockReward(*alice));
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  carol->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(carolWalletObserver.get()));

  std::array<int64_t, 5> amounts;
  amounts[0] = 1234567;
  amounts[1] = 1345678;
  amounts[2] = 1456789;
  amounts[3] = 1567890;
  amounts[4] = 1678901;
  uint64_t fee = 10000;

  ASSERT_NO_FATAL_FAILURE(performTransferWithErrorTx(amounts, fee));

  std::stringstream archive;
  alice->save(archive, false, true);

  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);

  EXPECT_EQ(0, alice->getTransactionCount());
  EXPECT_EQ(0, alice->getTransferCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, mineSaveNoCacheNoDetailsRefresh) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getBlockRewardForAddress(address);
  generator.getBlockRewardForAddress(address);
  generator.getBlockRewardForAddress(address);

  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  std::stringstream archive;
  alice->save(archive, false, false);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  alice->shutdown();

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD * 3, alice->pendingBalance());
  alice->shutdown();
}


TEST_F(WalletLegacyApi, sendMoneyToMyself) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getBlockRewardForAddress(address);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::TransactionId txId = TransferMoney(*alice, *alice, 100000000, 100);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD - 100, alice->actualBalance());
  ASSERT_EQ(0, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, sendSeveralTransactions) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  for (int i = 0; i < 5; ++i) {
    GetOneBlockReward(*alice);
  }

  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  auto aliceBalance = alice->actualBalance();

  uint64_t sendAmount = 100000;
  uint64_t totalSentAmount = 0;
  size_t transactionCount = 0;
  
  for (int i = 0; i < 10 && alice->actualBalance() > sendAmount; ++i) {
    CryptoNote::WalletLegacyTransfer tr;
    tr.address = bob->getAddress();
    tr.amount = sendAmount;

    auto txId = alice->sendTransaction(tr, m_currency.minimumFee(), "", 1, 0);  
    ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);

    std::error_code sendResult;
    ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), sendResult));
    ASSERT_EQ(std::error_code(), sendResult);

    ++transactionCount;
    totalSentAmount += sendAmount;
  }

  generator.generateEmptyBlocks(10);

  bobNode->updateObservers();

  while (totalSentAmount != bob->actualBalance()) {
    ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));
  }

  EXPECT_EQ(transactionCount, bob->getTransactionCount());
  EXPECT_EQ(0, bob->pendingBalance());
  EXPECT_EQ(totalSentAmount, bob->actualBalance());

  uint64_t aliceTotalBalance = alice->actualBalance() + alice->pendingBalance();
  EXPECT_EQ(aliceBalance - transactionCount * (sendAmount + m_currency.minimumFee()), aliceTotalBalance);
}

TEST_F(WalletLegacyApi, balanceAfterFailedTransaction) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  auto actualBalance = alice->actualBalance();
  auto pendingBalance = alice->pendingBalance();

  uint64_t send = 11000000;
  uint64_t fee = m_currency.minimumFee();

  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = send;

  aliceNode->setNextTransactionError();

  alice->sendTransaction(tr, fee, "", 1, 0);
  generator.generateEmptyBlocks(1);

  ASSERT_EQ(actualBalance, alice->actualBalance());
  ASSERT_EQ(pendingBalance, alice->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, checkPendingBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t startActualBalance = alice->actualBalance();
  int64_t sendAmount = 304050;
  uint64_t fee = m_currency.minimumFee();

  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = sendAmount;

  auto txId = alice->sendTransaction(tr, fee, "", 1, 0);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);

  std::error_code sendResult;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), sendResult));
  ASSERT_EQ(std::error_code(), sendResult);

  uint64_t totalBalance = alice->actualBalance() + alice->pendingBalance();
  ASSERT_EQ(startActualBalance - sendAmount - fee, totalBalance);

  generator.generateEmptyBlocks(1);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  ASSERT_EQ(sendAmount, bob->actualBalance());
  ASSERT_EQ(0, bob->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, checkChange) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;
  uint64_t sendAmount = 50000;
  uint64_t fee = m_currency.minimumFee();

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransfer tr;
  tr.address = bob->getAddress();
  tr.amount = sendAmount;

  auto txId = alice->sendTransaction(tr, fee, "", 1, 0);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);

  std::error_code sendResult;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get(), sendResult));
  ASSERT_EQ(std::error_code(), sendResult);

  EXPECT_EQ(0, alice->actualBalance());
  EXPECT_EQ(banknote - sendAmount - fee, alice->pendingBalance());
}

TEST_F(WalletLegacyApi, checkBalanceAfterSend) {
  alice->initAndGenerate("pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t banknote = 1000000000;

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));

  //Once wallet takes outputs in random fashion we don't know for sure which outputs will be taken.
  //In this case we generate controllable set of outs.
  generator.getSingleOutputTransaction(address, banknote);
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  const uint64_t sendAmount = 10000000;
  const uint64_t fee = 100;
  CryptoNote::TransactionId txId = TransferMoney(*alice, *alice, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  ASSERT_EQ(banknote, alice->actualBalance());
  ASSERT_EQ(banknote - sendAmount - fee, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, moneyInPoolDontAffectActualBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  const uint64_t sendAmount = 10000000;
  const uint64_t fee = 100;
  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(0, alice->actualBalance());
  EXPECT_EQ(banknote - sendAmount - fee, alice->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, balanceAfterTransactionsPlacedInBlockchain) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  uint64_t banknote = 1000000000;

  CryptoNote::AccountPublicAddress address;
  ASSERT_TRUE(m_currency.parseAccountAddressString(alice->getAddress(), address));
  generator.getSingleOutputTransaction(address, banknote);
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  const uint64_t sendAmount = 10000000;
  const uint64_t fee = 100;
  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  generator.generateEmptyBlocks(10);

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  aliceNode->includeTransactionsFromPoolToBlock();
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  EXPECT_EQ(banknote - sendAmount - fee, alice->actualBalance());
  EXPECT_EQ(0, alice->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, checkMyMoneyInTxPool) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t sendAmount = 8821902;
  uint64_t fee = 10000;

  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  EXPECT_EQ(0, bob->actualBalance());
  EXPECT_EQ(sendAmount, bob->pendingBalance());

  alice->shutdown();
  bob->shutdown();
}

TEST_F(WalletLegacyApi, initWithKeys) {
  CryptoNote::AccountKeys accountKeys;

  Crypto::generate_keys(accountKeys.address.spendPublicKey, accountKeys.spendSecretKey);
  Crypto::generate_keys(accountKeys.address.viewPublicKey, accountKeys.viewSecretKey);

  alice->initWithKeys(accountKeys, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get()));

  CryptoNote::AccountKeys keys;
  alice->getAccountKeys(keys);
  
  EXPECT_EQ(accountKeys.address.spendPublicKey, keys.address.spendPublicKey);
  EXPECT_EQ(accountKeys.spendSecretKey, keys.spendSecretKey);
  EXPECT_EQ(accountKeys.address.viewPublicKey, keys.address.viewPublicKey);
  EXPECT_EQ(accountKeys.viewSecretKey, keys.viewSecretKey);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, deleteTxFromPool) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  uint64_t sendAmount = 9748291;
  uint64_t fee = 10000;

  aliceNode->setNextTransactionToPool();
  CryptoNote::TransactionId txId = TransferMoney(*alice, *bob, sendAmount, fee);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  alice->shutdown();

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  generator.clearTxPool();

  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  EXPECT_EQ(0, bob->actualBalance());
  EXPECT_EQ(0, bob->pendingBalance());

  bob->shutdown();
}

TEST_F(WalletLegacyApi, sendAfterFailedTransaction) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::WalletLegacyTransfer tr;
  tr.amount = 100000;
  tr.address = "wrong_address";

  EXPECT_THROW(alice->sendTransaction(tr, 1000, "", 2, 0), std::system_error);
  CryptoNote::TransactionId txId = TransferMoney(*alice, *alice, 100000, 100);
  ASSERT_NE(txId, CryptoNote::WALLET_LEGACY_INVALID_TRANSACTION_ID);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));
  alice->shutdown();
}

TEST_F(WalletLegacyApi, DISABLED_loadingBrokenCache) {
  alice->initAndGenerate("pass");

  std::error_code result;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  ASSERT_EQ(result.value(), 0);

  std::stringstream archive;
  ASSERT_NO_FATAL_FAILURE(alice->save(archive, false, true));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));
  size_t sizeWithEmptyCache = archive.str().size();

  for (size_t i = 0; i < 3; ++i) {
    GetOneBlockReward(*alice);
  }
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();

  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  archive.str("");
  archive.clear();

  ASSERT_NO_FATAL_FAILURE(alice->save(archive, false, true));
  ASSERT_NO_FATAL_FAILURE(WaitWalletSave(aliceWalletObserver.get()));

  std::string state = archive.str();
  for (size_t i = sizeWithEmptyCache; i < state.size(); ++i) {
    state[i] = '\xff';
  }
  archive.str(state);

  prepareAliceWallet();
  alice->initAndLoad(archive, "pass");

  ASSERT_NO_FATAL_FAILURE(WaitWalletLoad(aliceWalletObserver.get(), result));
  ASSERT_EQ(result.value(), 0);
}

TEST_F(WalletLegacyApi, outcommingExternalTransactionTotalAmount) {
  class ExternalTxChecker : public CryptoNote::IWalletLegacyObserver {
  public:
    ExternalTxChecker(CryptoNote::WalletLegacy& wallet) : wallet(wallet), totalAmount(std::numeric_limits<int64_t>::max()) {
    }

    virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId) override {
      CryptoNote::WalletLegacyTransaction txInfo;
      ASSERT_TRUE(wallet.getTransaction(transactionId, txInfo));
      totalAmount = txInfo.totalAmount;
    }

    CryptoNote::WalletLegacy& wallet;
    int64_t totalAmount;
  };

  std::stringstream walletData;

  alice->initAndGenerate("pass");
  WaitWalletSync(aliceWalletObserver.get());

  alice->save(walletData, false, false);
  WaitWalletSave(aliceWalletObserver.get());

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  WaitWalletSync(aliceWalletObserver.get());

  prepareBobWallet();
  bob->initAndGenerate("pass2");
  WaitWalletSync(bobWalletObserver.get());

  uint64_t sent = 10000000;
  uint64_t fee = 1000;

  CryptoNote::WalletLegacyTransfer tr;
  tr.amount = sent;
  tr.address = bob->getAddress();

  alice->sendTransaction(tr, fee);
  WaitWalletSend(aliceWalletObserver.get());

  bob->shutdown();
  alice->shutdown();

  CryptoNote::WalletLegacy wallet(m_currency, *aliceNode);

  ExternalTxChecker externalTransactionObserver(wallet);
  TrivialWalletObserver walletObserver;

  wallet.addObserver(&externalTransactionObserver);
  wallet.addObserver(&walletObserver);

  wallet.initAndLoad(walletData, "pass");
  WaitWalletSync(&walletObserver);

  ASSERT_EQ(-static_cast<int64_t>(sent + fee), externalTransactionObserver.totalAmount);
  wallet.shutdown();
}

TEST_F(WalletLegacyApi, shutdownAllowsInitializeWalletWithTheSameKeys) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::AccountKeys accountKeys;
  alice->getAccountKeys(accountKeys);

  alice->shutdown();
  alice->initWithKeys(accountKeys, "pass");

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(1, alice->getTransactionCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, shutdownAllowsInitializeWalletWithDifferentKeys) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->shutdown();
  alice->initAndGenerate("pass");

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(0, alice->getTransactionCount());

  alice->shutdown();
}

namespace {
class WalletSynchronizationProgressUpdatedObserver : public CryptoNote::IWalletLegacyObserver {
public:
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t /*total*/) override {
    m_current = current;
  }

  uint64_t m_current = 0;
};
}

TEST_F(WalletLegacyApi, shutdownDoesNotRemoveObservers) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  WalletSynchronizationProgressUpdatedObserver observer;
  CryptoNote::WalletHelper::IWalletRemoveObserverGuard observerGuard(*alice, observer);

  alice->shutdown();
  observer.m_current = 0;
  alice->initAndGenerate("pass");

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(5, observer.m_current);

  observerGuard.removeObserver();
  alice->shutdown();
}

namespace {
class WalletTransactionEventCounter : public CryptoNote::IWalletLegacyObserver {
public:
  virtual void externalTransactionCreated(CryptoNote::TransactionId /*transactionId*/) override {
    ++m_count;
  }

  virtual void transactionUpdated(CryptoNote::TransactionId /*transactionId*/) override {
    ++m_count;
  }

  size_t m_count = 0;
};
}

TEST_F(WalletLegacyApi, afterShutdownAndInitWalletDoesNotSendNotificationsRelatedToOldAddress) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  std::string aliceAddress1 = alice->getAddress();
  CryptoNote::AccountKeys accountKeys1;
  alice->getAccountKeys(accountKeys1);

  alice->shutdown();
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  std::string aliceAddress2 = alice->getAddress();

  alice->shutdown();
  alice->initWithKeys(accountKeys1, "pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  WalletTransactionEventCounter observer;
  CryptoNote::WalletHelper::IWalletRemoveObserverGuard observerGuard(*alice, observer);

  prepareBobWallet();
  bob->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));
  GetOneBlockReward(*bob);
  generator.generateEmptyBlocks(10);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  std::vector<CryptoNote::WalletLegacyTransfer> transfers;
  transfers.push_back({ aliceAddress1, TEST_BLOCK_REWARD / 10 });
  transfers.push_back({ aliceAddress2, TEST_BLOCK_REWARD / 5 });
  bob->sendTransaction(transfers, m_currency.minimumFee());
  std::error_code sendResult;
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(bobWalletObserver.get(), sendResult));

  generator.generateEmptyBlocks(1);
  bobNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(bobWalletObserver.get()));

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(1, observer.m_count);

  observerGuard.removeObserver();
  bob->shutdown();
  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetDoesNotChangeAddress) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  auto expectedAddress = alice->getAddress();
  alice->reset();
  ASSERT_EQ(expectedAddress, alice->getAddress());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetDoesNotChangeAccountKeys) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  CryptoNote::AccountKeys expectedAccountKeys;
  alice->getAccountKeys(expectedAccountKeys);

  alice->reset();

  CryptoNote::AccountKeys actualAccountKeys;
  alice->getAccountKeys(actualAccountKeys);

  ASSERT_EQ(expectedAccountKeys.address, actualAccountKeys.address);
  ASSERT_EQ(expectedAccountKeys.spendSecretKey, actualAccountKeys.spendSecretKey);
  ASSERT_EQ(expectedAccountKeys.viewSecretKey, actualAccountKeys.viewSecretKey);

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetDoesNotRemoveObservers) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  WalletSynchronizationProgressUpdatedObserver observer;
  CryptoNote::WalletHelper::IWalletRemoveObserverGuard observerGuard(*alice, observer);

  alice->reset();
  observer.m_current = 0;

  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(5, observer.m_current);

  observerGuard.removeObserver();
  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetDoesNotChangePassword) {
  std::string password = "password";
  std::string newPassword = "new_password";

  alice->initAndGenerate(password);
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->reset();
  ASSERT_TRUE(static_cast<bool>(alice->changePassword(newPassword, password)));
  ASSERT_FALSE(static_cast<bool>(alice->changePassword(password, newPassword)));

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetClearsPendingBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice->pendingBalance());
  alice->reset();
  ASSERT_EQ(0, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetClearsActualBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice->actualBalance());
  alice->reset();
  ASSERT_EQ(0, alice->actualBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetClearsTransactionHistory) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(1, alice->getTransactionCount());
  alice->reset();
  ASSERT_EQ(0, alice->getTransactionCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetClearsTransfersHistory) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->sendTransaction({ alice->getAddress(), 100 }, m_currency.minimumFee());
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  ASSERT_EQ(1, alice->getTransferCount());
  alice->reset();
  ASSERT_EQ(0, alice->getTransferCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetAndSyncRestorePendingBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->reset();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice->pendingBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetAndSyncRestoreActualBalance) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->reset();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice->actualBalance());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetAndSyncRestoreTransactions) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->reset();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(1, alice->getTransactionCount());

  alice->shutdown();
}

TEST_F(WalletLegacyApi, resetAndSyncDoNotRestoreTransfers) {
  alice->initAndGenerate("pass");
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  GetOneBlockReward(*alice);
  generator.generateEmptyBlocks(10);
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  alice->sendTransaction({ alice->getAddress(), 100 }, m_currency.minimumFee());
  ASSERT_NO_FATAL_FAILURE(WaitWalletSend(aliceWalletObserver.get()));

  alice->reset();
  aliceNode->updateObservers();
  ASSERT_NO_FATAL_FAILURE(WaitWalletSync(aliceWalletObserver.get()));

  ASSERT_EQ(0, alice->getTransferCount());

  alice->shutdown();
}

void generateWallet(CryptoNote::IWalletLegacy& wallet, TrivialWalletObserver& observer, const std::string& pass) {
  wallet.initAndGenerate(pass);
  WaitWalletSync(&observer);
}

TEST_F(WalletLegacyApi, outdatedUnconfirmedTransactionDeletedOnNewBlock) {
  const uint64_t TRANSACTION_MEMPOOL_TIME = 1;
  CryptoNote::Currency currency(CryptoNote::CurrencyBuilder(m_logger).mempoolTxLiveTime(TRANSACTION_MEMPOOL_TIME).currency());
  TestBlockchainGenerator blockchainGenerator(currency);
  INodeTrivialRefreshStub node(blockchainGenerator);
  CryptoNote::WalletLegacy wallet(currency, node);
  TrivialWalletObserver walletObserver;
  wallet.addObserver(&walletObserver);

  wallet.initAndGenerate("pass");
  WaitWalletSync(&walletObserver);

  GetOneBlockRewardAndUnlock(wallet, walletObserver, node, currency, blockchainGenerator);

  const std::string ADDRESS = "2634US2FAz86jZT73YmM8u5GPCknT2Wxj8bUCKivYKpThFhF2xsjygMGxbxZzM42zXhKUhym6Yy6qHHgkuWtruqiGkDpX6m";
  node.setNextTransactionToPool();
  auto id = wallet.sendTransaction({ADDRESS, static_cast<int64_t>(TEST_BLOCK_REWARD - m_currency.minimumFee())}, m_currency.minimumFee());
  WaitWalletSend(&walletObserver);

  node.cleanTransactionPool();
  std::this_thread::sleep_for(std::chrono::seconds(TRANSACTION_MEMPOOL_TIME));

  blockchainGenerator.generateEmptyBlocks(1);
  node.updateObservers();
  WaitWalletSync(&walletObserver);

  ASSERT_EQ(TEST_BLOCK_REWARD, wallet.actualBalance());

  CryptoNote::WalletLegacyTransaction transaction;
  ASSERT_TRUE(wallet.getTransaction(id, transaction));
  EXPECT_EQ(CryptoNote::WalletLegacyTransactionState::Deleted, transaction.state);

  wallet.removeObserver(&walletObserver);
  wallet.shutdown();
}

TEST_F(WalletLegacyApi, outdatedUnconfirmedTransactionDeletedOnLoad) {
  const uint64_t TRANSACTION_MEMPOOL_TIME = 1;
  CryptoNote::Currency currency(CryptoNote::CurrencyBuilder(m_logger).mempoolTxLiveTime(TRANSACTION_MEMPOOL_TIME).currency());
  TestBlockchainGenerator blockchainGenerator(currency);
  INodeTrivialRefreshStub node(blockchainGenerator);
  CryptoNote::WalletLegacy wallet(currency, node);
  TrivialWalletObserver walletObserver;
  wallet.addObserver(&walletObserver);

  wallet.initAndGenerate("pass");
  WaitWalletSync(&walletObserver);

  GetOneBlockRewardAndUnlock(wallet, walletObserver, node, currency, blockchainGenerator);

  const std::string ADDRESS = "2634US2FAz86jZT73YmM8u5GPCknT2Wxj8bUCKivYKpThFhF2xsjygMGxbxZzM42zXhKUhym6Yy6qHHgkuWtruqiGkDpX6m";
  node.setNextTransactionToPool();
  auto id = wallet.sendTransaction({ADDRESS, static_cast<int64_t>(TEST_BLOCK_REWARD - m_currency.minimumFee())}, m_currency.minimumFee());
  WaitWalletSend(&walletObserver);

  node.cleanTransactionPool();

  std::stringstream data;
  wallet.save(data);
  WaitWalletSave(&walletObserver);

  wallet.shutdown();

  std::this_thread::sleep_for(std::chrono::seconds(TRANSACTION_MEMPOOL_TIME));

  wallet.initAndLoad(data, "pass");
  WaitWalletSync(&walletObserver);

  ASSERT_EQ(TEST_BLOCK_REWARD, wallet.actualBalance());

  CryptoNote::WalletLegacyTransaction transaction;
  ASSERT_TRUE(wallet.getTransaction(id, transaction));
  EXPECT_EQ(CryptoNote::WalletLegacyTransactionState::Deleted, transaction.state);

  wallet.removeObserver(&walletObserver);
  wallet.shutdown();
}

TEST_F(WalletLegacyApi, walletLoadsNullSpendSecretKey) {
  CryptoNote::AccountKeys accountKeys;

  Crypto::generate_keys(accountKeys.address.spendPublicKey, accountKeys.spendSecretKey);
  Crypto::generate_keys(accountKeys.address.viewPublicKey, accountKeys.viewSecretKey);
  accountKeys.spendSecretKey = CryptoNote::NULL_SECRET_KEY;

  alice->initWithKeys(accountKeys, "pass");
  WaitWalletSync(aliceWalletObserver.get());

  std::stringstream data;
  alice->save(data);
  WaitWalletSave(aliceWalletObserver.get());

  alice->shutdown();

  alice->initAndLoad(data, "pass");
  WaitWalletSync(aliceWalletObserver.get());

  ASSERT_EQ(std::error_code(), aliceWalletObserver->loadResult);
  alice->shutdown();
}
