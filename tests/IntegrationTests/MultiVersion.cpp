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

#include "../IntegrationTestLib/BaseFunctionalTests.h"

#include <thread>
#include <Logging/ConsoleLogger.h>

#include "CryptoNoteCore/Account.h"
#include "WalletLegacyObserver.h"

#undef ERROR

using namespace CryptoNote;
using namespace Logging;

inline std::string shortAddress(const std::string& addr) {
  return addr.substr(0, 6);
}

class MultiVersionTest : Tests::Common::BaseFunctionalTests {
public:

  MultiVersionTest(const CryptoNote::Currency& currency, System::Dispatcher& d, const Tests::Common::BaseFunctionalTestsConfig& config, Logging::ILogger& log) :
    BaseFunctionalTests(currency, d, config), m_config(config), m_nodeCount(config.daemons.size()), logger(log, "MultiVersion") {}


  void run() {
    if (m_config.daemons.empty()) {
      logger(ERROR, BRIGHT_RED) << "No daemons configured, exiting";
      return;
    }

    launchTestnet(m_nodeCount, Tests::Common::BaseFunctionalTests::Line);
    
    createWallets();
    
    miningTest();

    // create some address for mining
    CryptoNote::AccountBase stashAddress;
    stashAddress.generate();
    auto stashAddressStr = m_currency.accountAddressAsString(stashAddress);

    unlockMoney(stashAddressStr);

    std::vector<uint64_t> balances;
    for (auto& o : m_observers) {
      balances.push_back(o->totalBalance());
    }

    printBalances();

    // transfer money from each wallet to each other
    for (size_t i = 0; i < m_nodeCount; ++i) {
      auto& srcWallet = *m_wallets[i];
      for (size_t wi = 0; wi < m_nodeCount; ++wi) {
        if (i != wi) {
          CryptoNote::WalletLegacyTransfer transfer;
          transfer.address = m_wallets[wi]->getAddress();
          transfer.amount = (i * 1000 + wi * 100) * m_currency.coin();
          logger(INFO, BRIGHT_YELLOW) << "Sending from " << shortAddress(srcWallet.getAddress()) << " to " << shortAddress(transfer.address) << " amount = " << m_currency.formatAmount(transfer.amount);
          auto txid = srcWallet.sendTransaction(transfer, m_currency.minimumFee());

          balances[i] -= transfer.amount + m_currency.minimumFee();
          balances[wi] += transfer.amount;

          auto res = m_observers[i]->waitSendResult(txid);

          if (res) {
            logger(ERROR, BRIGHT_RED) << "Failed to send transaction: " << res.message();
            throw std::runtime_error("Failed to send transaction: " + res.message());
          }

          logger(INFO) << "Sent successfully";
        }
      }
    }

    nodeDaemons[0]->startMining(1, stashAddressStr);

    for (size_t i = 0; i < m_nodeCount; ++i) {
      uint64_t total;
      logger(INFO) << i << " Expected target balance: " << m_currency.formatAmount(balances[i]);

      while ((total = m_wallets[i]->pendingBalance() + m_wallets[i]->actualBalance()) != balances[i]) {
        logger(INFO) << i << " - total: " << m_currency.formatAmount(total) << ", waiting";
        m_observers[i]->waitTotalBalanceChange();
      }
    }

    nodeDaemons[0]->stopMining();

    printBalances();
  }

  void miningTest() {
    auto prevHeight = nodeDaemons[0]->getLocalHeight();

    // mine block from each node to each wallet
    for (size_t i = 0; i < m_nodeCount; ++i) {
      for (size_t shift = 0; shift < m_nodeCount; ++shift) {
        logger(INFO, BRIGHT_YELLOW) << "Starting mining from node " << i << " -> wallet at node " << shift;

        while (nodeDaemons[i]->getLocalHeight() != prevHeight) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        logger(INFO) << "Starting mining at height " << prevHeight;
        nodeDaemons[i]->startMining(1, m_wallets[shift]->getAddress());

        uint64_t newHeight = 0;

        while ((newHeight = nodeDaemons[i]->getLocalHeight()) == prevHeight) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        nodeDaemons[i]->stopMining();
        logger(INFO) << "Block mined, new height = " << newHeight;

        prevHeight = nodeDaemons[i]->getLocalHeight();

        logger(INFO, BRIGHT_YELLOW) << "Waiting for balance to change";
        auto res = m_observers[shift]->waitPendingBalanceChangeFor(std::chrono::seconds(m_currency.difficultyTarget() * 5));

        if (!res.first) {
          logger(ERROR, BRIGHT_RED) << "Timeout waiting for balance to change!";
          throw std::runtime_error("Timeout");
        }
      }
    }
  }

  void unlockMoney(const std::string& miningAddress) {
    logger(INFO, BRIGHT_YELLOW) << "Starting to mine blocks to unlock money";

    // unlock money
    nodeDaemons[0]->startMining(1, miningAddress);
    for (auto& o : m_observers) {
      o->waitActualBalanceChange();
    }
    nodeDaemons[0]->stopMining();
    logger(INFO, BRIGHT_YELLOW) << "Unlocked all, waiting for all daemons to sync blockchain";

    auto minerHeight = nodeDaemons[0]->getLocalHeight();
    logger(INFO) << "Miner height: " << minerHeight;

    bool unsynced = true;

    while (unsynced) {
      unsynced = false;
      for (auto& o : m_observers) {
        if (o->getCurrentHeight() < minerHeight) {
          unsynced = true;
          break;
        }
      }

      if (unsynced) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }

    logger(INFO) << "OK";
  }

  void printBalances() {
    for (auto& w : m_wallets) {
      auto pending = w->pendingBalance();
      auto actual = w->actualBalance();

      logger(INFO, BRIGHT_GREEN) << 
        "Wallet " << shortAddress(w->getAddress()) << 
        ": " << m_currency.formatAmount(actual) << 
        " / " << m_currency.formatAmount(pending) << 
        " total = " << m_currency.formatAmount(pending + actual);
    }
  }

  void createWallets() {
    for (auto& daemon : nodeDaemons) {
      std::unique_ptr<INode> node;
      std::unique_ptr<IWalletLegacy> wallet;
      
      daemon->makeINode(node);
      makeWallet(wallet, node);

      std::unique_ptr<WalletLegacyObserver> observer(new WalletLegacyObserver);

      wallet->addObserver(observer.get());

      m_nodes.push_back(std::move(node));
      m_wallets.push_back(std::move(wallet));
      m_observers.push_back(std::move(observer));
    }
  }

  void startShiftedMining(size_t shift) {
    for (size_t i = 0; i < m_nodeCount; ++i) {
      nodeDaemons[i]->startMining(1, m_wallets[(i + shift) % m_nodeCount]->getAddress());
    }
  }

  void waitAllPendingBalancesChange() {
    for (auto& o : m_observers) {
      o->waitPendingBalanceChange();
    }
  }

  void stopAllMining() {
    for (auto& n : nodeDaemons) {
      n->stopMining();
    }
  }

private:

  const size_t m_nodeCount;
  const Tests::Common::BaseFunctionalTestsConfig& m_config;

  std::vector<std::unique_ptr<INode>> m_nodes;
  std::vector<std::unique_ptr<IWalletLegacy>> m_wallets;
  std::vector<std::unique_ptr<WalletLegacyObserver>> m_observers;

  Logging::LoggerRef logger;
};


void testMultiVersion(const CryptoNote::Currency& currency, System::Dispatcher& d, const Tests::Common::BaseFunctionalTestsConfig& config) {
  Logging::ConsoleLogger log;
  MultiVersionTest test(currency, d, config, log);
  test.run();
}
