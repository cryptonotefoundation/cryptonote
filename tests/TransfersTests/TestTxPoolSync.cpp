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

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Currency.h"
#include "Logging/LoggerManager.h"
#include "P2p/NetNodeConfig.h"
#include "System/Dispatcher.h"
#include "System/InterruptedException.h"
#include "WalletLegacy/WalletLegacy.h"

#include "../IntegrationTestLib/BaseFunctionalTests.h"
#include "../IntegrationTestLib/TestWalletLegacy.h"


using namespace CryptoNote;
using namespace Tests::Common;
using namespace Crypto;

extern System::Dispatcher globalSystem;
extern Tests::Common::BaseFunctionalTestsConfig config;

namespace {
  class NodeTxPoolSyncTest : public Tests::Common::BaseFunctionalTests, public ::testing::Test {
  public:
    NodeTxPoolSyncTest() :
        BaseFunctionalTests(m_currency, globalSystem, config),
        m_dispatcher(globalSystem),
        m_currency(CurrencyBuilder(m_logManager).testnet(true).currency()) {
    }

  protected:
    Logging::LoggerManager m_logManager;
    System::Dispatcher& m_dispatcher;
    CryptoNote::Currency m_currency;
  };

  TEST_F(NodeTxPoolSyncTest, TxPoolsAreRequestedRightAfterANodeIsConnectedToAnotherIfTheirBlockchainsAreSynchronized) {
    //System::Timer timer(m_dispatcher);
    //m_dispatcher.spawn([&m_dispatcher, &timer] {
    //  try {
    //    timer.sleep(std::chrono::minutes(5));
    //    m_dispatcher.
    //  } catch (System::InterruptedException&) {
    //  }
    //});

    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;
    const size_t NODE_2 = 2;
    const size_t NODE_3 = 3;

    launchTestnet(4, Tests::Common::BaseFunctionalTests::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;
    std::unique_ptr<CryptoNote::INode> node3;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);
    nodeDaemons[NODE_2]->makeINode(node2);
    nodeDaemons[NODE_3]->makeINode(node3);

    CryptoNote::AccountBase minerAccount;
    minerAccount.generate();

    TestWalletLegacy wallet1(m_dispatcher, m_currency, *node1);
    TestWalletLegacy wallet2(m_dispatcher, m_currency, *node2);

    ASSERT_FALSE(static_cast<bool>(wallet1.init()));
    ASSERT_FALSE(static_cast<bool>(wallet2.init()));

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet2.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], minerAccount.getAccountKeys().address, m_currency.minedMoneyUnlockWindow()));

    wallet1.waitForSynchronizationToHeight(m_currency.minedMoneyUnlockWindow() + 3);
    wallet2.waitForSynchronizationToHeight(m_currency.minedMoneyUnlockWindow() + 3);

    stopNode(NODE_2);
    // To make sure new transaction won't be received by NODE_2 and NODE_3
    ASSERT_TRUE(waitForPeerCount(*node1, 1));

    Hash txHash1;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash1)));

    stopNode(NODE_1);
    // Don't start NODE_2, while NODE_1 doesn't close its connections
    ASSERT_TRUE(waitForPeerCount(*node0, 0));

    startNode(NODE_2);
    ASSERT_TRUE(waitDaemonReady(NODE_2));
    ASSERT_TRUE(waitForPeerCount(*node3, 1));

    Hash txHash2;
    ASSERT_FALSE(static_cast<bool>(wallet2.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash2)));

    startNode(NODE_1);
    ASSERT_TRUE(waitDaemonReady(NODE_1));

    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs1;
    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs2;
    ASSERT_TRUE(waitForPoolSize(NODE_1, *node1, 2, poolTxs1));
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 2, poolTxs2));

    //timer.stop();

    std::vector<Hash> poolTxsIds1;
    std::vector<Hash> poolTxsIds2;

    for (auto& tx : poolTxs1) {
      Hash txHash = tx->getTransactionHash();
      poolTxsIds1.emplace_back(std::move(txHash));
    }
    for (auto& tx : poolTxs2) {
      Hash txHash = tx->getTransactionHash();
      poolTxsIds2.emplace_back(std::move(txHash));
    }

    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash1) != poolTxsIds1.end());
    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash2) != poolTxsIds1.end());

    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash1) != poolTxsIds2.end());
    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash2) != poolTxsIds2.end());
  }

  TEST_F(NodeTxPoolSyncTest, TxPoolsAreRequestedRightAfterInitialBlockchainsSynchronization) {
    //System::Timer timer(m_dispatcher);
    //m_dispatcher.spawn([&m_dispatcher, &timer] {
    //  try {
    //    timer.sleep(std::chrono::minutes(5));
    //    m_dispatcher.
    //  } catch (System::InterruptedException&) {
    //  }
    //});

    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;
    const size_t NODE_2 = 2;
    const size_t NODE_3 = 3;

    launchTestnet(4, Tests::Common::BaseFunctionalTests::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;
    std::unique_ptr<CryptoNote::INode> node3;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);
    nodeDaemons[NODE_2]->makeINode(node2);
    nodeDaemons[NODE_3]->makeINode(node3);

    CryptoNote::AccountBase minerAccount;
    minerAccount.generate();

    TestWalletLegacy wallet1(m_dispatcher, m_currency, *node1);
    TestWalletLegacy wallet2(m_dispatcher, m_currency, *node2);

    ASSERT_FALSE(static_cast<bool>(wallet1.init()));
    ASSERT_FALSE(static_cast<bool>(wallet2.init()));

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet2.address(), 1));

    wallet1.waitForSynchronizationToHeight(static_cast<uint32_t>(3));
    wallet2.waitForSynchronizationToHeight(static_cast<uint32_t>(3));

    stopNode(NODE_2);
    // To make sure new transaction won't be received by NODE_2 and NODE_3
    ASSERT_TRUE(waitForPeerCount(*node1, 1));

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], minerAccount.getAccountKeys().address, m_currency.minedMoneyUnlockWindow()));
    wallet1.waitForSynchronizationToHeight(m_currency.minedMoneyUnlockWindow() + 3);

    Hash txHash1;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash1)));

    stopNode(NODE_1);
    // Don't start NODE_2, while NODE_1 doesn't close its connections
    ASSERT_TRUE(waitForPeerCount(*node0, 0));

    startNode(NODE_2);
    ASSERT_TRUE(waitDaemonReady(NODE_2));
    ASSERT_TRUE(waitForPeerCount(*node3, 1));

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_3], minerAccount.getAccountKeys().address, m_currency.minedMoneyUnlockWindow()));
    wallet2.waitForSynchronizationToHeight(m_currency.minedMoneyUnlockWindow() + 3);

    Hash txHash2;
    ASSERT_FALSE(static_cast<bool>(wallet2.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash2)));

    startNode(NODE_1);
    ASSERT_TRUE(waitDaemonReady(NODE_1));

    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs1;
    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs2;
    ASSERT_TRUE(waitForPoolSize(NODE_1, *node1, 2, poolTxs1));
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 2, poolTxs2));

    //timer.stop();

    std::vector<Hash> poolTxsIds1;
    std::vector<Hash> poolTxsIds2;

    for (auto& tx : poolTxs1) {
      Hash txHash = tx->getTransactionHash();
      poolTxsIds1.emplace_back(std::move(txHash));
    }
    for (auto& tx : poolTxs2) {
      Hash txHash = tx->getTransactionHash();
      poolTxsIds2.emplace_back(std::move(txHash));
    }

    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash1) != poolTxsIds1.end());
    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash2) != poolTxsIds1.end());

    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash1) != poolTxsIds2.end());
    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash2) != poolTxsIds2.end());
  }

  TEST_F(NodeTxPoolSyncTest, TxPoolsAreRequestedRightAfterTimedBlockchainsSynchronization) {
    //System::Timer timer(m_dispatcher);
    //m_dispatcher.spawn([&m_dispatcher, &timer] {
    //  try {
    //    timer.sleep(std::chrono::minutes(5));
    //    m_dispatcher.
    //  } catch (System::InterruptedException&) {
    //  }
    //});

    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;
    const size_t NODE_2 = 2;
    const size_t NODE_3 = 3;
    const size_t NODE_4 = 4;

    launchTestnet(5, Tests::Common::BaseFunctionalTests::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;
    std::unique_ptr<CryptoNote::INode> node3;
    std::unique_ptr<CryptoNote::INode> node4;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);
    nodeDaemons[NODE_2]->makeINode(node2);
    nodeDaemons[NODE_3]->makeINode(node3);
    nodeDaemons[NODE_4]->makeINode(node4);

    CryptoNote::AccountBase minerAccount;
    minerAccount.generate();

    TestWalletLegacy wallet1(m_dispatcher, m_currency, *node1);
    ASSERT_FALSE(static_cast<bool>(wallet1.init()));

    stopNode(NODE_4);
    ASSERT_TRUE(waitForPeerCount(*node3, 1));

    stopNode(NODE_3);
    ASSERT_TRUE(waitForPeerCount(*node2, 1));

    stopNode(NODE_2);
    ASSERT_TRUE(waitForPeerCount(*node1, 1));

    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], minerAccount.getAccountKeys().address, m_currency.minedMoneyUnlockWindow()));
    wallet1.waitForSynchronizationToHeight(m_currency.minedMoneyUnlockWindow() + 2);

    Hash txHash1;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(m_currency.accountAddressAsString(minerAccount), m_currency.coin(), txHash1)));

    // Start nodes simultaneously due to them connect each other and decided that they are connected to network
    startNode(NODE_4);
    startNode(NODE_3);
    ASSERT_TRUE(waitDaemonReady(NODE_4));
    ASSERT_TRUE(waitDaemonReady(NODE_3));
    ASSERT_TRUE(waitForPeerCount(*node4, 1));
    ASSERT_TRUE(waitForPeerCount(*node3, 1));

    //std::this_thread::sleep_for(std::chrono::seconds(5));

    startNode(NODE_2);
    ASSERT_TRUE(waitDaemonReady(NODE_2));

    // NODE_3 and NODE_4 are synchronized by timer
    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs2;
    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs3;
    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs4;
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 1, poolTxs2));
    ASSERT_TRUE(waitForPoolSize(NODE_3, *node3, 1, poolTxs3));
    ASSERT_TRUE(waitForPoolSize(NODE_4, *node4, 1, poolTxs4));

    //timer.stop();

    Hash poolTxId2 = poolTxs2.front()->getTransactionHash();
    Hash poolTxId3 = poolTxs3.front()->getTransactionHash();
    Hash poolTxId4 = poolTxs4.front()->getTransactionHash();

    ASSERT_EQ(txHash1, poolTxId2);
    ASSERT_EQ(txHash1, poolTxId3);
    ASSERT_EQ(txHash1, poolTxId4);
  }

  TEST_F(NodeTxPoolSyncTest, TxPoolsAreRequestedRightAfterSwitchingToAlternativeChain) {
    // If this condition isn't true, then test must be rewritten a bit
    ASSERT_GT(m_currency.difficultyLag() + m_currency.difficultyCut(), m_currency.minedMoneyUnlockWindow());

    //System::Timer timer(m_dispatcher);
    //m_dispatcher.spawn([&m_dispatcher, &timer] {
    //  try {
    //    timer.sleep(std::chrono::minutes(5));
    //    m_dispatcher.
    //  } catch (System::InterruptedException&) {
    //  }
    //});

    const size_t NODE_0 = 0;
    const size_t NODE_1 = 1;
    const size_t NODE_2 = 2;
    const size_t NODE_3 = 3;

    launchTestnet(4, Tests::Common::BaseFunctionalTests::Line);

    std::unique_ptr<CryptoNote::INode> node0;
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;
    std::unique_ptr<CryptoNote::INode> node3;

    nodeDaemons[NODE_0]->makeINode(node0);
    nodeDaemons[NODE_1]->makeINode(node1);
    nodeDaemons[NODE_2]->makeINode(node2);
    nodeDaemons[NODE_3]->makeINode(node3);

    TestWalletLegacy wallet0(m_dispatcher, m_currency, *node1);
    TestWalletLegacy wallet1(m_dispatcher, m_currency, *node1);
    TestWalletLegacy wallet2(m_dispatcher, m_currency, *node2);

    ASSERT_FALSE(static_cast<bool>(wallet0.init()));
    ASSERT_FALSE(static_cast<bool>(wallet1.init()));
    ASSERT_FALSE(static_cast<bool>(wallet2.init()));

    uint32_t blockchainLenght = 1;
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet0.address(), m_currency.difficultyBlocksCount()));
    blockchainLenght += static_cast<uint32_t>(m_currency.difficultyBlocksCount());

    wallet1.waitForSynchronizationToHeight(blockchainLenght);
    wallet2.waitForSynchronizationToHeight(blockchainLenght);

    stopNode(NODE_2);
    // To make sure new blocks won't be received by NODE_2
    ASSERT_TRUE(waitForPeerCount(*node1, 1));

    // Generate alternative chain for NODE_1
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet1.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_0], wallet2.address(), m_currency.minedMoneyUnlockWindow()));
    blockchainLenght += 1 + m_currency.minedMoneyUnlockWindow();

    wallet1.waitForSynchronizationToHeight(blockchainLenght);

    // This transaction is valid in both alternative chains, it is just an indicator, that shows when NODE_1 and NODE_2 are synchronized
    Hash txHash0;
    ASSERT_FALSE(static_cast<bool>(wallet0.sendTransaction(wallet0.wallet()->getAddress(), m_currency.coin(), txHash0)));

    // This transaction is valid only in alternative chain 1
    Hash txHash1;
    ASSERT_FALSE(static_cast<bool>(wallet1.sendTransaction(wallet0.wallet()->getAddress(), m_currency.coin(), txHash1)));

    stopNode(NODE_1);
    // Don't start NODE_2, while NODE_1 doesn't close its connections
    ASSERT_TRUE(waitForPeerCount(*node0, 0));

    startNode(NODE_2);
    ASSERT_TRUE(waitDaemonReady(NODE_2));
    ASSERT_TRUE(waitForPeerCount(*node3, 1));

    // Generate alternative chain for NODE_2.
    // After that it is expected that alternative chains 1 and 2 have the same difficulty, because
    // m_currency.minedMoneyUnlockWindow() < m_currency.difficultyLag() + m_currency.difficultyCut()
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_2], wallet2.address(), 1));
    ASSERT_TRUE(mineBlocks(*nodeDaemons[NODE_2], wallet1.address(), m_currency.minedMoneyUnlockWindow()));

    wallet2.waitForSynchronizationToHeight(blockchainLenght);

    // This block template doesn't contain txHash2, as it is not created yet
    CryptoNote::BlockTemplate blockTemplate2;
    uint64_t difficulty2;
    ASSERT_TRUE(nodeDaemons[NODE_2]->getBlockTemplate(wallet1.wallet()->getAddress(), blockTemplate2, difficulty2));
    ASSERT_EQ(1, difficulty2);
    ASSERT_TRUE(blockTemplate2.transactionHashes.empty());

    // This transaction is valid only in alternative chain 2
    Hash txHash2;
    ASSERT_FALSE(static_cast<bool>(wallet2.sendTransaction(wallet0.wallet()->getAddress(), m_currency.coin(), txHash2)));

    startNode(NODE_1);
    ASSERT_TRUE(waitDaemonReady(NODE_1));
    ASSERT_TRUE(waitForPeerCount(*node2, 2));

    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs2;
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 2, poolTxs2));

    // Now NODE_1 and NODE_2 are synchronized, but both are on its own alternative chains
    Hash tailId1;
    Hash tailId2;
    ASSERT_TRUE(nodeDaemons[NODE_1]->getTailBlockId(tailId1));
    ASSERT_TRUE(nodeDaemons[NODE_2]->getTailBlockId(tailId2));
    ASSERT_NE(tailId1, tailId2);

    // Add block to alternative chain 2, and wait for when NODE_1 switches to alternative chain 2.
    ASSERT_TRUE(prepareAndSubmitBlock(*nodeDaemons[NODE_2], std::move(blockTemplate2)));
    blockchainLenght += 1;

    wallet1.waitForSynchronizationToHeight(blockchainLenght);
    wallet2.waitForSynchronizationToHeight(blockchainLenght);

    std::vector<std::unique_ptr<CryptoNote::ITransactionReader>> poolTxs1;
    ASSERT_TRUE(waitForPoolSize(NODE_1, *node1, 2, poolTxs1));
    ASSERT_TRUE(waitForPoolSize(NODE_2, *node2, 2, poolTxs2));

    // Now NODE_1 and NODE_2 are on the same chain
    ASSERT_TRUE(nodeDaemons[NODE_1]->getTailBlockId(tailId1));
    ASSERT_TRUE(nodeDaemons[NODE_2]->getTailBlockId(tailId2));
    ASSERT_EQ(tailId1, tailId2);

    //timer.stop();

    std::vector<Hash> poolTxsIds1;
    std::vector<Hash> poolTxsIds2;

    for (auto& tx : poolTxs1) {
      Hash txHash = tx->getTransactionHash();
      poolTxsIds1.emplace_back(std::move(txHash));
    }
    for (auto& tx : poolTxs2) {
      Hash txHash = tx->getTransactionHash();
      poolTxsIds2.emplace_back(std::move(txHash));
    }

    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash0) != poolTxsIds1.end());
    ASSERT_TRUE(std::find(poolTxsIds1.begin(), poolTxsIds1.end(), txHash2) != poolTxsIds1.end());

    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash0) != poolTxsIds2.end());
    ASSERT_TRUE(std::find(poolTxsIds2.begin(), poolTxsIds2.end(), txHash2) != poolTxsIds2.end());
  }
}
