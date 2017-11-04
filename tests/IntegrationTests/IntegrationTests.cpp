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
#include <Logging/LoggerRef.h>

#include "../IntegrationTestLib/BaseFunctionalTests.h"
#include "../IntegrationTestLib/NodeObserver.h"

#include "WalletLegacy/WalletLegacy.h"
#include "WalletLegacyObserver.h"

using namespace CryptoNote;
using namespace Logging;

extern Tests::Common::BaseFunctionalTestsConfig baseCfg;
// extern System::Dispatcher globalDispatcher;

struct TotalWalletBalance {

  TotalWalletBalance(uint64_t actual_ = 0, uint64_t pending_ = 0) 
    : actual(actual_), pending(pending_) {}

  TotalWalletBalance(IWalletLegacy& wallet) 
    : TotalWalletBalance(wallet.actualBalance(), wallet.pendingBalance()) {}

  uint64_t actual = 0;
  uint64_t pending = 0;

  uint64_t total() const {
    return actual + pending;
  }
};

class IntegrationTest : public Tests::Common::BaseFunctionalTests, public ::testing::Test {
public:

  IntegrationTest() : 
    currency(CryptoNote::CurrencyBuilder(log).testnet(true).currency()), 
    BaseFunctionalTests(currency, dispatcher, baseCfg),
    logger(log, "IntegrationTest") {
  }

  ~IntegrationTest() {
    wallets.clear();
    inodes.clear();

    stopTestnet();
  }

  void makeINodes() {
    for (auto& n : nodeDaemons) {
      std::unique_ptr<INode> node;
      n->makeINode(node);
      inodes.push_back(std::move(node));
    }
  }

  void makeWallets() {
    for (auto& n: inodes) {
      std::unique_ptr<CryptoNote::IWalletLegacy> wallet(new CryptoNote::WalletLegacy(m_currency, *n));
      std::unique_ptr<WalletLegacyObserver> observer(new WalletLegacyObserver());

      wallet->initAndGenerate(walletPassword);
      wallet->addObserver(observer.get());

      wallets.push_back(std::move(wallet));
      walletObservers.push_back(std::move(observer));
    }
  }

  void mineBlocksFor(size_t node, const std::string& address, size_t blockCount) {
    auto prevHeight = nodeDaemons[node]->getLocalHeight();
    nodeDaemons[node]->startMining(1, address);

    do {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (prevHeight + blockCount < nodeDaemons[node]->getLocalHeight());

    nodeDaemons[node]->stopMining();
  }

  void printWalletBalances() {
    for (auto& w: wallets) {
      logger(INFO) << "Wallet " << w->getAddress().substr(0, 6);
      logger(INFO) << "  " << currency.formatAmount(w->actualBalance()) << " actual / " << currency.formatAmount(w->pendingBalance()) << " pending";
    }
  }

  void mineEmptyBlocks(size_t nodeNum, size_t blocksCount) {
  }

  void mineMoneyForWallet(size_t nodeNum, size_t walletNum) {
    auto& wallet = *wallets[walletNum];
    auto& node = *nodeDaemons[nodeNum];

    node.startMining(1, wallet.getAddress());
    walletObservers[walletNum]->waitActualBalanceChange();
    node.stopMining();

    while (node.getLocalHeight() > walletObservers[walletNum]->getCurrentHeight()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  std::error_code transferMoney(size_t srcWallet, size_t dstWallet, uint64_t amount, uint64_t fee) {
    logger(INFO) 
      << "Transferring from " << wallets[srcWallet]->getAddress().substr(0, 6) 
      << " to " << wallets[dstWallet]->getAddress().substr(0, 6) << " " << currency.formatAmount(amount);

    CryptoNote::WalletLegacyTransfer tr;
    tr.address = wallets[dstWallet]->getAddress();
    tr.amount = amount;
    std::error_code result;

    auto txId = wallets[srcWallet]->sendTransaction(tr, fee);

    logger(DEBUGGING) << "Transaction id = " << txId;

    return walletObservers[srcWallet]->waitSendResult(txId);
  }

  void checkIncomingTransfer(size_t dstWallet, uint64_t amount) {
    startMining(1);

    auto txId = walletObservers[dstWallet]->waitExternalTransaction();

    stopMining();

    WalletLegacyTransaction txInfo;

    ASSERT_TRUE(wallets[dstWallet]->getTransaction(txId, txInfo));
    ASSERT_EQ(txInfo.totalAmount, amount);
  }

  System::Dispatcher dispatcher;
  std::string walletPassword = "pass";
  CryptoNote::Currency currency;
  Logging::ConsoleLogger log;
  Logging::LoggerRef logger;

  std::vector<std::unique_ptr<INode>> inodes;
  std::vector<std::unique_ptr<IWalletLegacy>> wallets;
  std::vector<std::unique_ptr<WalletLegacyObserver>> walletObservers;
};



TEST_F(IntegrationTest, Wallet2Wallet) {
  //const uint64_t FEE = 1000000;

  launchTestnet(2);

  logger(INFO) << "Testnet launched";

  makeINodes();
  makeWallets();

  logger(INFO) << "Created wallets";

  mineMoneyForWallet(0, 0);

  logger(INFO) << "Mined money";

  printWalletBalances();

  TotalWalletBalance w0pre(*wallets[0]);
  TotalWalletBalance w1pre(*wallets[1]);

  auto sendAmount = w0pre.actual / 2;

  ASSERT_TRUE(!transferMoney(0, 1, sendAmount, currency.minimumFee()));
  ASSERT_NO_FATAL_FAILURE(checkIncomingTransfer(1, sendAmount));

  printWalletBalances();

  TotalWalletBalance w0after(*wallets[0]);
  TotalWalletBalance w1after(*wallets[1]);

  // check total 
  ASSERT_EQ(w0pre.total() + w1pre.total() - currency.minimumFee(), w0after.total() + w1after.total());

  // check diff
  ASSERT_EQ(sendAmount, w1after.total() - w1pre.total());
}

TEST_F(IntegrationTest, BlockPropagationSpeed) {

  launchTestnet(3, Line);
  makeINodes();

  {
    std::unique_ptr<CryptoNote::INode>& localNode = inodes.front();
    std::unique_ptr<CryptoNote::INode>& remoteNode = inodes.back();

    std::unique_ptr<CryptoNote::IWalletLegacy> wallet;
    makeWallet(wallet, localNode);

    NodeObserver localObserver(*localNode);
    NodeObserver remoteObserver(*remoteNode);

    const size_t BLOCKS_COUNT = 10;

    nodeDaemons.front()->startMining(1, wallet->getAddress());

    for (size_t blockNumber = 0; blockNumber < BLOCKS_COUNT; ++blockNumber) {
      uint32_t localHeight = localObserver.waitLastKnownBlockHeightUpdated();
      uint32_t remoteHeight = 0;

      while (remoteHeight != localHeight) {
        ASSERT_TRUE(remoteObserver.waitLastKnownBlockHeightUpdated(std::chrono::milliseconds(5000), remoteHeight));
      }

      logger(INFO) << "Iteration " << blockNumber + 1 << ": " << "height = " << localHeight;
    }

    nodeDaemons.front()->stopMining();
  }
}
