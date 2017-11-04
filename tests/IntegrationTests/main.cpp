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

#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <mutex>  
#include <functional>
#include <future>

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

#include "../IntegrationTestLib/BaseFunctionalTests.h"
#include "../IntegrationTestLib/Logger.h"

#include "Logging/ConsoleLogger.h"

#ifndef CHECK_AND_ASSERT_MES
#define CHECK_AND_ASSERT_MES(expr, fail_ret_val, message)   do{if(!(expr)) {LOG_ERROR(message); return fail_ret_val;};}while(0)
#endif

#ifndef CHECK_AND_ASSERT_MES_NON_FATAL
#define CHECK_AND_ASSERT_MES_NON_FATAL(expr, fail_ret_val, message)   do{if(!(expr)) {LOG_WARNING(message); };}while(0)
#endif

Tests::Common::BaseFunctionalTestsConfig baseCfg;
// System::Dispatcher globalDispatcher;

namespace po = boost::program_options;
namespace {
class ConfigurationError : public std::runtime_error {
public:
  ConfigurationError(const char* desc) : std::runtime_error(desc) {}
};

struct Configuration : public Tests::Common::BaseFunctionalTestsConfig {
  Configuration() : desc("Allowed options") {
    init();
  }

  bool handleCommandLine(int argc, char **argv) {
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
    po::notify(vm);
    BaseFunctionalTestsConfig::handleCommandLine(vm);
    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return false;
    }

    if (vm.count("test-type")) {
      auto testType = vm["test-type"].as<uint16_t>();
      if (testType < 1 || testType >= TESTLAST)
        throw ConfigurationError("Incorrect test type.");
      _testType = (TestType)testType;
    } else
      throw ConfigurationError("Missing test type.");
    return true;
  }

  enum TestType {
    WALLET2WALLET = 1,
    BLOCKTHRUDAEMONS = 3,
    RELAYBLOCKTHRUDAEMONS = 4,
    TESTPOOLANDINPROCNODE = 5,
    TESTPOOLDELETION = 6,
    TESTMULTIVERSION = 7,
    TESTLAST
  } _testType;

  po::options_description desc;

protected:
  void init() {
    desc.add_options()
      ("help,h", "produce this help message and exit")
      ("test-type,t", po::value<uint16_t>()->default_value(1), 
        "test type:\r\n"
        "1 - wallet to wallet test,\r\n"
        "3 - block thru daemons test\r\n"
        "4 - relay block thru daemons\r\n"
        "5 - test tx pool and inproc node\r\n"
        "6 - deleting tx from pool due to timeout\r\n"
        "7 - multiple daemons interoperability test (use -a option to specify daemons)\r\n");
    BaseFunctionalTestsConfig::init(desc);
  }
};
}


class SimpleTest : public Tests::Common::BaseFunctionalTests {
public:

  SimpleTest(const CryptoNote::Currency& currency, System::Dispatcher& system, const Tests::Common::BaseFunctionalTestsConfig& config) : 
    BaseFunctionalTests(currency, system, config) {}

  class WaitForActualGrowObserver : public CryptoNote::IWalletLegacyObserver {
    Tests::Common::Semaphore& m_GotActual;

    uint64_t m_lastFunds;

  public:
    WaitForActualGrowObserver(Tests::Common::Semaphore& GotActual, uint64_t lastFunds) : m_GotActual(GotActual), m_lastFunds(lastFunds) { }

    virtual void actualBalanceUpdated(uint64_t actualBalance) override {
      if (m_lastFunds < actualBalance) {
        m_GotActual.notify();
      }
      m_lastFunds = actualBalance;
    }
  };

  class WaitForActualDwindleObserver : public CryptoNote::IWalletLegacyObserver {
    Tests::Common::Semaphore& m_GotActual;

    uint64_t m_lastFunds;

  public:
    WaitForActualDwindleObserver(Tests::Common::Semaphore& GotActual, uint64_t lastFunds) : m_GotActual(GotActual), m_lastFunds(lastFunds) { }

    virtual void actualBalanceUpdated(uint64_t actualBalance) override {
      if (m_lastFunds > actualBalance) {
        m_GotActual.notify();
      }
      m_lastFunds = actualBalance;
    }
  };

  class WaitForPendingGrowObserver : public CryptoNote::IWalletLegacyObserver {
    Tests::Common::Semaphore& m_GotActual;

    uint64_t m_lastFunds;

  public:
    WaitForPendingGrowObserver(Tests::Common::Semaphore& GotActual, uint64_t lastFunds) : m_GotActual(GotActual), m_lastFunds(lastFunds) { }

    virtual void pendingBalanceUpdated(uint64_t pendingBalance) override {
      if (m_lastFunds < pendingBalance) {
        m_GotActual.notify();
      }
      m_lastFunds = pendingBalance;
    }
  };

  class WaitForConfirmationObserver : public CryptoNote::IWalletLegacyObserver {
    Tests::Common::Semaphore& m_confirmed;

    std::function<bool(uint64_t)> m_pred;
  public:
    WaitForConfirmationObserver(Tests::Common::Semaphore& confirmed, std::function<bool(uint64_t)> pred) : m_confirmed(confirmed), m_pred(pred) { }

    virtual void pendingBalanceUpdated(uint64_t pendingBalance) override {
      if (m_pred(pendingBalance)) m_confirmed.notify();
    }
  };

  class WaitForSendCompletedObserver : public CryptoNote::IWalletLegacyObserver {
    Tests::Common::Semaphore& m_Sent;
    std::error_code& m_error;
    CryptoNote::TransactionId& m_transactionId;

  public:
    WaitForSendCompletedObserver(Tests::Common::Semaphore& Sent, CryptoNote::TransactionId& transactionId, std::error_code& error) : m_Sent(Sent), m_transactionId(transactionId), m_error(error) { }
    virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) override {
      m_error = result;
      m_transactionId = transactionId;
      m_Sent.notify();
    }
  };

  class WaitForExternalTransactionObserver : public CryptoNote::IWalletLegacyObserver {
  public:
    WaitForExternalTransactionObserver() { }
    std::promise<CryptoNote::TransactionId> promise;

    virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId) override {
      promise.set_value(transactionId);
    }

  };


  class WaitForTransactionUpdated : public CryptoNote::IWalletLegacyObserver {
  public:
    WaitForTransactionUpdated() {}
    std::promise<void> promise;

    virtual void transactionUpdated(CryptoNote::TransactionId transactionId) override {
      if (expectindTxId == transactionId) {
        promise.set_value();
      }
    }

    CryptoNote::TransactionId expectindTxId;
  };


  bool perform1() {
    using namespace Tests::Common;
    using namespace CryptoNote;
    const uint64_t FEE = 1000000;
    launchTestnet(2);
    LOG_TRACE("STEP 1 PASSED");

    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> node2;

    nodeDaemons.front()->makeINode(node1);
    nodeDaemons.front()->makeINode(node2);

    std::unique_ptr<CryptoNote::IWalletLegacy> wallet1;
    std::unique_ptr<CryptoNote::IWalletLegacy> wallet2;

    makeWallet(wallet1, node1);
    makeWallet(wallet2, node2);

    CHECK_AND_ASSERT_MES(mineBlock(), false, "can't mine block");
    CHECK_AND_ASSERT_MES(mineBlock(), false, "can't mine block");
    LOG_TRACE("STEP 2 PASSED");
    LOG_DEBUG("Wallet1 pending: " +  m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " +  m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " +  m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " +  m_currency.formatAmount(wallet2->actualBalance()));
    CHECK_AND_ASSERT_MES(mineBlock(wallet1), false, "can't mine block on wallet 1");
    CHECK_AND_ASSERT_MES(mineBlock(), false, "can't mine block");
    LOG_TRACE("STEP 3 PASSED");
    LOG_DEBUG("Wallet1 pending: " +  m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " +  m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " +  m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " +  m_currency.formatAmount(wallet2->actualBalance()));
    Semaphore wallet1GotActual;
    WaitForConfirmationObserver wallet1ActualGrown(wallet1GotActual, [](uint64_t pending)->bool {return pending == 0; });
    wallet1->addObserver(&wallet1ActualGrown);
    CHECK_AND_ASSERT_MES(startMining(1) , false, "startMining(1) failed");
    wallet1GotActual.wait();
    LOG_TRACE("STEP 4 PASSED");
    LOG_DEBUG("Wallet1 pending: " +  m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " +  m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " +  m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " +  m_currency.formatAmount(wallet2->actualBalance()));
    CHECK_AND_ASSERT_MES(stopMining() , false, "stopMining() failed");
    auto wallet1ActualBeforeTransaction = wallet1->actualBalance();
    auto wallet2ActualBeforeTransaction = wallet2->actualBalance();
    auto wallet2PendingBeforeTransaction = wallet2->pendingBalance();
    CryptoNote::WalletLegacyTransfer tr;
    tr.address = wallet2->getAddress();
    tr.amount = wallet1ActualBeforeTransaction / 2;
    TransactionId sendTransaction;
    std::error_code result;
    Semaphore moneySent;
    WaitForSendCompletedObserver sco1(moneySent, sendTransaction, result);    
    Semaphore w2GotPending;
    WaitForPendingGrowObserver pgo1(w2GotPending, wallet2PendingBeforeTransaction);
    wallet2->addObserver(&pgo1);
    wallet1->addObserver(&sco1);
    wallet1->sendTransaction(tr, FEE);
    CHECK_AND_ASSERT_MES(startMining(1), false, "startMining(1) failed");
    moneySent.wait();
    w2GotPending.wait();
    CHECK_AND_ASSERT_MES(stopMining(), false, "stopMining() failed");
    auto wallet2PendingAfterTransaction = wallet2->pendingBalance();
    auto wallet1PendingAfterTransaction = wallet1->pendingBalance();
    auto w2PendingDiff = wallet2PendingAfterTransaction - wallet2PendingBeforeTransaction;
    auto wallet1ActualAfterTransaction = wallet1->actualBalance();

    LOG_TRACE("STEP 5 PASSED");
    LOG_DEBUG("Wallet1 pending: " +  m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " +  m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " +  m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " +  m_currency.formatAmount(wallet2->actualBalance()));
    CHECK_AND_ASSERT_MES((tr.amount == w2PendingDiff), false, "STEP 6 ASSERTION 1 FAILED\r\n Transfered amount " +  m_currency.formatAmount(tr.amount) + " doesn't match recieved amount " +  m_currency.formatAmount(w2PendingDiff));
    CHECK_AND_ASSERT_MES((wallet1ActualBeforeTransaction - wallet1PendingAfterTransaction - wallet1ActualAfterTransaction - tr.amount - FEE == 0), false,
      "STEP 6 ASSERTION 2 FAILED\r\n wallet1 Actual Before Transaction doesn't match wallet1 total After Transaction + Transfered amount + Fee "  
      + m_currency.formatAmount(wallet1ActualBeforeTransaction) + " <> " + m_currency.formatAmount(wallet1PendingAfterTransaction) + " + " + m_currency.formatAmount(wallet1ActualAfterTransaction) + " + " + m_currency.formatAmount(tr.amount) + " + " + m_currency.formatAmount(FEE));
    LOG_TRACE("STEP 6 PASSED");
    LOG_DEBUG("Wallet1 pending: " +  m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " +  m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " +  m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " +  m_currency.formatAmount(wallet2->actualBalance()));
    CHECK_AND_ASSERT_MES(startMining(1), false, "startMining(1) failed");
    Semaphore confirmed2;
    Semaphore confirmed1;
    WaitForConfirmationObserver confirmationObserver2(confirmed2, [](uint64_t pending)->bool {return pending == 0; });
    WaitForConfirmationObserver confirmationObserver1(confirmed1, [](uint64_t pending)->bool {return pending == 0; });
    wallet2->addObserver(&confirmationObserver2);
    wallet1->addObserver(&confirmationObserver1);
    if (wallet2->pendingBalance() != 0) confirmed2.wait();
    if (wallet1->pendingBalance() != 0) confirmed1.wait();
    CHECK_AND_ASSERT_MES(stopMining(), false, "stopMining() failed");
    auto wallet1ActualAfterTransactionAndConfirmation = wallet1->actualBalance();
    auto wallet2ActualAfterTransactionAndConfirmation = wallet2->actualBalance();
    auto w2ActualDiff = wallet2ActualAfterTransactionAndConfirmation - wallet2ActualBeforeTransaction;
    auto w1ActualDiff = wallet1ActualBeforeTransaction - wallet1ActualAfterTransactionAndConfirmation;
    CHECK_AND_ASSERT_MES((tr.amount == w2ActualDiff), false, "STEP 7 FAILED\r\n Transfered amount " +  m_currency.formatAmount(tr.amount) + " doesn't match confirmed recieved amount " +  m_currency.formatAmount(w2ActualDiff));
    CHECK_AND_ASSERT_MES((w1ActualDiff - tr.amount - FEE == 0), false,
      "STEP 7 FAILED\r\n wallet1 Actual Before Transaction doesn't match wallet1 Actual After Transaction + Transfered amount + Fee "
      +  m_currency.formatAmount(wallet1ActualBeforeTransaction) + " <> " +  m_currency.formatAmount(wallet1ActualAfterTransactionAndConfirmation) + "+" +  m_currency.formatAmount(tr.amount) + "+" +  m_currency.formatAmount(FEE));
    LOG_TRACE("STEP 7 PASSED");
    LOG_DEBUG("Wallet1 pending: " +  m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " +  m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " +  m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " +  m_currency.formatAmount(wallet2->actualBalance()));
    wallet1->removeObserver(&wallet1ActualGrown);
    wallet2->removeObserver(&pgo1);
    wallet1->removeObserver(&sco1);
    wallet2->removeObserver(&confirmationObserver2);
    wallet1->removeObserver(&confirmationObserver1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    return true;
  }

  class WaitForBlockchainHeightChangeObserver : public CryptoNote::INodeObserver {
    Tests::Common::Semaphore& m_changed;
  public:
    WaitForBlockchainHeightChangeObserver(Tests::Common::Semaphore& changed) : m_changed(changed) { }
    virtual void lastKnownBlockHeightUpdated(uint32_t height) override {
      m_changed.notify();
    }
  };

  class CallbackHeightChangeObserver : public CryptoNote::INodeObserver {
    std::function<void(uint32_t)> m_callback;
  public:
    CallbackHeightChangeObserver(std::function<void(uint32_t)> callback) : m_callback(callback) {}
    virtual void lastKnownBlockHeightUpdated(uint32_t height) override {
      m_callback(height);
    }
  };

  bool perform2(size_t blocksCount = 10)
  {
    using namespace Tests::Common;
    launchTestnet(3, Line);
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    LOG_TRACE("STEP 1 PASSED");
    mineBlock();
    mineBlock();
    LOG_TRACE("STEP 2 PASSED");
    std::unique_ptr<CryptoNote::INode> localNode;
    std::unique_ptr<CryptoNote::INode> remoteNode;

    nodeDaemons.front()->makeINode(localNode);
    nodeDaemons.back()->makeINode(remoteNode);
    
    std::unique_ptr<CryptoNote::IWalletLegacy> wallet;
    makeWallet(wallet, localNode);

    LOG_TRACE("STEP 3 PASSED");
    Semaphore blockMined;
    Semaphore blockArrivedToRemote;

    WaitForBlockchainHeightChangeObserver localHCO(blockMined);
    WaitForBlockchainHeightChangeObserver remoteHCO(blockArrivedToRemote);

    localNode->addObserver(&localHCO);
    remoteNode->addObserver(&remoteHCO);
    for (size_t blockNumber = 0; blockNumber < blocksCount; ++blockNumber) {
      nodeDaemons.front()->startMining(1, wallet->getAddress());
      blockMined.wait();
      CHECK_AND_ASSERT_MES(blockArrivedToRemote.wait_for(std::chrono::milliseconds(5000)), false, "block propagation too slow >5000ms.");
      nodeDaemons.front()->stopMining();
      LOG_TRACE("STEP 4 STAGE " + TO_STRING(blockNumber+1) + " of " + TO_STRING(blocksCount)+" PASSED");
    }
    
    return true;
  }

  bool perform4() {
    using namespace CryptoNote;
    using namespace Tests::Common;
    launchTestnet(3, Star);
    LOG_TRACE("STEP 1 PASSED");
    
    std::unique_ptr<CryptoNote::INode> hopNode;
    std::unique_ptr<CryptoNote::INode> localNode;
    std::unique_ptr<CryptoNote::INode> remoteNode;

    nodeDaemons[0]->makeINode(hopNode);
    nodeDaemons[1]->makeINode(localNode);
    nodeDaemons[2]->makeINode(remoteNode);

    LOG_TRACE("STEP 2 PASSED");


    std::string test_block1_hex =
      "0101b392d79f05a742885cb01d11b7b36fb8bf14616d42cd3d8c1429a224df41afa81b86b8a3a84e"
      "d8c33f010b01ff0108c0a62d02cc353782cbe4c6067bd30510f11d1f2993f2c7fed37239f299ffe3"
      "f96f135675c096b102023e8d4b2c22d73f91d0d9f8e0e12c8df24e5917f00d0b2dd99786c5bb0e5b"
      "300580bbb021022764ae61c084db07e7cd83c55e9c833f42b1d422e1008220fdb4acc726b94ea980"
      "88debe01023330c2b7dc4840f478066370ae48b148ce8dd010c59f6ecc08598682d32f07d080a0d9"
      "e61d02bcf35dc40ead54a614174774e60d8f5d0e46272c70bc7e70f205f7ccef25c34980b09dc2df"
      "01026ddcf1aed901f018453fd9352a01d5a44067d271ca403b4cd799d9832076daa280f092cbdd08"
      "021f613eab32b76ed03f6a796de7a5c92009ea9f9b9e3299ec91df7657cd694e5580c089a9a2f50f"
      "02f545046885a297ba63a2c7b305a74fdb741129cc367330661c1363e0bb0f0d0b2101acf052dcbe"
      "407bc34df1b7fffc17f0bfb0ffc23002e2b6de48a210df6f78bf1400";
    std::string test_block2_hex =
      "0101b492d79f05456231a956ed3a8c1ac0bfe8efc1bb5d522d8474e566b051919ddea0ceab478a74"
      "e35210010c01ff0207c0e41202b2d7e697c6e2e894f9e98262c278235720b39f3a149774cb58cb52"
      "e5dde21601c09fab0302abffefad3afab42ca1ce2f7dccfa6942256f31387b307becd43571cfe22a"
      "10688084af5f02e2ab32d9b8fb8ced4bf4a81de0f48c23dc575076e8d233a3532d28f36e79035380"
      "a0d9e61d020c10664fe1ca35418733fa32ae2deadd4bf7ed982bb5d11ba98a7940a73e161580b09d"
      "c2df010266c3bfa27436b480a217a2fe06df714f4d2094ec1a0ced3bac2d96881972e28a80f092cb"
      "dd0802f31e9ac25fb8afd1d9d964331242a94f023c3188db5e532b5a9c800a843a3ebc80c089a9a2"
      "f50f0206244fcc73941c3da62ea6d62d679bedb311fc530d149099bdfd04c59cd507a121019d4b74"
      "f09454ccfdd6ca44b8c5f73c6805ea08dbe6a71769b058e158b2d4df5100";
    std::string test_block3_hex =
      "0101b492d79f051f6fe6d9f7c14c0d5e16ba82d9ea68e4e6d6f30726854d45330aeb2fae5c1cd3fb"
      "7f4352010d01ff0308ffae350220f4c1c7631ecf4247688c376665df2b9dd935af6e4c027c9cddcb"
      "400fefec7380a4e803029e05ef9b3295e178d0f3199fca420f909f04fdab09b97c14290c8a913e42"
      "19c68087a70e02693641fefb1a6da81c2308370f349ef5e4adab792ae06b5da989ae3f1b7a13ca80"
      "d293ad0302c14d721ed8da5c98f108ef17c326737765857ddfa0b705fd4483cfa7ffeaad51808cee"
      "891a02f5a8e2ac24d6a9f789e5514de520c3ac28387788e130e22c4250b7d1be47460380b09dc2df"
      "0102cc3b2f894b416f3e09afae0395fc01cc2ec9763dff72839944e60055049ea37d80f092cbdd08"
      "02772df06a2cd92c174815ae1572799430ea01e903796f6a763648c7b350151ce580c089a9a2f50f"
      "0211c7bea98edba4fad6d3f19b330a676b8fb0391f7a99f45542e7cf52d39d6c632101e0370c5c79"
      "e99d772b41e0569bc41e1ebde2e563cdb7f5bdd23984899fad103200";
    std::string test_block4_hex =
      "0101b492d79f0537da79424e1cc69d16aadf174dcf443947f8027695a5d1e30b2be4f5aa71904194"
      "47fe54010e01ff0407fffc1a0278eb82c9ea2f1e998906cec55caf26e347224c3391fb0aa2213bc1"
      "5eec4dacc580bbb021024ce32a63614269f43f698644c98fd9b7a11694dc69fd5126f6f6735ba6c5"
      "98dd808c8d9e0202d539ead46faf6d786964dd5106004612eb8d64778ad4fe8befa8c63e4d666f92"
      "808cee891a021a6c6669298dcc1c86af887804f128123d95a6d96b5884db97cfc96fa9ad018e80b0"
      "9dc2df0102dd3b9bbfef1eddeef8c406de9c0c4fc469c8069c910541252491df5a482fd5e380f092"
      "cbdd0802176a4cb411309761b7f50b0f495e99cc55cbaae70011d3c901e409a8a938f1b680c089a9"
      "a2f50f02bb232a77911350a1315de0b3de447142390f97e5ef25ecc1bf5837a8972b4b5e2101ef54"
      "5c318e38cfdd92362340fab6ec6630e4134b93cfd01db4d9a42fa945fdef00";

    Semaphore blockArrivedToRemote;

    WaitForBlockchainHeightChangeObserver remoteHCO(blockArrivedToRemote);

    std::chrono::steady_clock::time_point localAdded;
    std::chrono::steady_clock::time_point hoplAdded;
    std::chrono::steady_clock::time_point remoteAdded;
    std::chrono::steady_clock::time_point submitInvokingStart;
    std::chrono::steady_clock::time_point submitInvoked;

    //auto height = localNode->getLastKnownBlockHeight();

    CallbackHeightChangeObserver CHCOLocal([&localAdded](uint64_t new_height){localAdded = std::chrono::steady_clock::now(); });
    CallbackHeightChangeObserver CHCOHop([&hoplAdded](uint64_t new_height){hoplAdded = std::chrono::steady_clock::now(); });
    CallbackHeightChangeObserver CHCORemote([&remoteAdded](uint64_t new_height){remoteAdded = std::chrono::steady_clock::now(); });

    localNode->addObserver(&CHCOLocal);
    hopNode->addObserver(&CHCOHop);
    remoteNode->addObserver(&CHCORemote);
    remoteNode->addObserver(&remoteHCO);

    LOG_TRACE("test_block1");
    submitInvokingStart = std::chrono::steady_clock::now();
    if (!nodeDaemons[1]->submitBlock(test_block1_hex)) return false;
    submitInvoked = std::chrono::steady_clock::now();
    CHECK_AND_ASSERT_MES(blockArrivedToRemote.wait_for(std::chrono::milliseconds(10000)), false, "block 1 propagation too slow >10000ms.");
    LOG_TRACE("submitBlock() invocation takes:        " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(submitInvoked - submitInvokingStart).count()) + " ms");
    LOG_TRACE("HeightChangedCallback() since submit : " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(localAdded - submitInvoked).count()) + " ms");
    LOG_TRACE("Local   -> HopNode: " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(hoplAdded - localAdded).count()) + " ms");
    LOG_TRACE("HopNode -> Remote:  " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(remoteAdded - hoplAdded).count()) + " ms");
    LOG_TRACE("Local   -> Remote:  " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(remoteAdded - localAdded).count()) + " ms");

    LOG_TRACE("test_block2");
    submitInvokingStart = std::chrono::steady_clock::now();
    if (!nodeDaemons[1]->submitBlock(test_block2_hex)) return false;
    submitInvoked = std::chrono::steady_clock::now();
    CHECK_AND_ASSERT_MES(blockArrivedToRemote.wait_for(std::chrono::milliseconds(10000)), false, "block 2 propagation too slow >10000ms.");
    LOG_TRACE("submitBlock() invocation takes:        " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(submitInvoked - submitInvokingStart).count()) + " ms");
    LOG_TRACE("HeightChangedCallback() since submit : " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(localAdded - submitInvoked).count()) + " ms");
    LOG_TRACE("Local   -> HopNode: " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(hoplAdded - localAdded).count()) + " ms");
    LOG_TRACE("HopNode -> Remote:  " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(remoteAdded - hoplAdded).count()) + " ms");
    LOG_TRACE("Local   -> Remote:  " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(remoteAdded - localAdded).count()) + " ms");

    LOG_TRACE("test_block3");
    submitInvokingStart = std::chrono::steady_clock::now();
    if (!nodeDaemons[1]->submitBlock(test_block3_hex)) return false;
    submitInvoked = std::chrono::steady_clock::now();
    CHECK_AND_ASSERT_MES(blockArrivedToRemote.wait_for(std::chrono::milliseconds(10000)), false, "block 3 propagation too slow >10000ms.");
    LOG_TRACE("submitBlock() invocation takes:        " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(submitInvoked - submitInvokingStart).count()) + " ms");
    LOG_TRACE("HeightChangedCallback() since submit : " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(localAdded - submitInvoked).count()) + " ms");
    LOG_TRACE("Local   -> HopNode: " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(hoplAdded - localAdded).count()) + " ms");
    LOG_TRACE("HopNode -> Remote:  " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(remoteAdded - hoplAdded).count()) + " ms");
    LOG_TRACE("Local   -> Remote:  " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(remoteAdded - localAdded).count()) + " ms");
    
    LOG_TRACE("test_block4");
    submitInvokingStart = std::chrono::steady_clock::now();
    if (!nodeDaemons[1]->submitBlock(test_block4_hex)) return false;
    submitInvoked = std::chrono::steady_clock::now();
    CHECK_AND_ASSERT_MES(blockArrivedToRemote.wait_for(std::chrono::milliseconds(10000)), false, "block 4 propagation too slow >10000ms.");
    LOG_TRACE("submitBlock() invocation takes:        " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(submitInvoked - submitInvokingStart).count()) + " ms");
    LOG_TRACE("HeightChangedCallback() since submit : " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(localAdded - submitInvoked).count()) + " ms");
    LOG_TRACE("Local   -> HopNode: " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(hoplAdded - localAdded).count()) + " ms");
    LOG_TRACE("HopNode -> Remote:  " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(remoteAdded - hoplAdded).count()) + " ms");
    LOG_TRACE("Local   -> Remote:  " + TO_STRING(std::chrono::duration_cast<std::chrono::milliseconds>(remoteAdded - localAdded).count()) + " ms");

    localNode.release();
    remoteNode.release();
    hopNode.release();
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    return true;
  }


  bool perform5() {
    using namespace Tests::Common;
    using namespace CryptoNote;
    const uint64_t FEE = 1000000;
    launchTestnetWithInprocNode(2);
    
    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> inprocNode;

    nodeDaemons.front()->makeINode(node1);
    nodeDaemons.back()->makeINode(inprocNode);

    while (node1->getLastLocalBlockHeight() != inprocNode->getLastLocalBlockHeight()) {
      LOG_TRACE("Syncing...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    LOG_TRACE("STEP 1 PASSED");

    std::unique_ptr<CryptoNote::IWalletLegacy> wallet1;
    std::unique_ptr<CryptoNote::IWalletLegacy> wallet2;

    makeWallet(wallet1, node1);
    makeWallet(wallet2, inprocNode);

    CHECK_AND_ASSERT_MES(mineBlock(), false, "can't mine block");
    CHECK_AND_ASSERT_MES(mineBlock(), false, "can't mine block");
    LOG_TRACE("STEP 2 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));

    CHECK_AND_ASSERT_MES(mineBlock(wallet1), false, "can't mine block on wallet 1");

    LOG_TRACE("STEP 3 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));

    Semaphore wallet1GotActual;
    WaitForConfirmationObserver wallet1ActualGrown(wallet1GotActual, [&wallet1](uint64_t actual)->bool {return wallet1->pendingBalance() == actual; });
    wallet1->addObserver(&wallet1ActualGrown);
    CHECK_AND_ASSERT_MES(startMining(1), false, "startMining(1) failed");
    wallet1GotActual.wait();

    LOG_TRACE("STEP 4 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));
    
    CHECK_AND_ASSERT_MES(stopMining(), false, "stopMining() failed");

    auto wallet1ActualBeforeTransaction = wallet1->actualBalance();
    auto wallet1PendingBeforeTransaction = wallet1->pendingBalance();
    auto wallet2ActualBeforeTransaction = wallet2->actualBalance();
    auto wallet2PendingBeforeTransaction = wallet2->pendingBalance();
    CryptoNote::WalletLegacyTransfer tr;
    tr.address = wallet2->getAddress();
    tr.amount = wallet1ActualBeforeTransaction / 2;
    std::error_code result;
    Semaphore w2GotPending;
    WaitForPendingGrowObserver pgo1(w2GotPending, wallet2PendingBeforeTransaction);
    wallet2->addObserver(&pgo1);
    
    WaitForExternalTransactionObserver poolTxWaiter;
    auto future = poolTxWaiter.promise.get_future();
    wallet2->addObserver(&poolTxWaiter);

    wallet1->sendTransaction(tr, FEE);

    auto txId = future.get();
    w2GotPending.wait();

    wallet2->removeObserver(&poolTxWaiter);
    CryptoNote::WalletLegacyTransaction txInfo;
    wallet2->getTransaction(txId, txInfo);

    auto wallet2PendingAfterTransaction = wallet2->pendingBalance();
    auto wallet1PendingAfterTransaction = wallet1->pendingBalance();
    auto w2PendingDiff = wallet2PendingAfterTransaction - wallet2PendingBeforeTransaction;
    auto w1PendingDiff = wallet1PendingBeforeTransaction - wallet1PendingAfterTransaction;
    CHECK_AND_ASSERT_MES((txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), false, "STEP 5 ASSERTION 1 FAILED\r\n Transaction blockHeight differs unconfirmed_tx_height");
    CHECK_AND_ASSERT_MES((tr.amount == txInfo.totalAmount), false, "STEP 5 ASSERTION 2 FAILED\r\n Transfered amount " + m_currency.formatAmount(tr.amount) + " doesn't match recieved amount from pool transaction " + m_currency.formatAmount(txInfo.totalAmount));
    CHECK_AND_ASSERT_MES((tr.amount == w2PendingDiff), false, "STEP 5 ASSERTION 3 FAILED\r\n Transfered amount " + m_currency.formatAmount(tr.amount) + " doesn't match recieved amount " + m_currency.formatAmount(w2PendingDiff));
    CHECK_AND_ASSERT_MES((w1PendingDiff - tr.amount - FEE == 0), false,
      "STEP 5 ASSERTION 4 FAILED\r\n wallet1 Pending Before Transaction doesn't match wallet1 Pending After Transaction + Transfered amount + Fee "
      + m_currency.formatAmount(wallet1PendingBeforeTransaction) + " <> " + m_currency.formatAmount(wallet1PendingAfterTransaction) + "+" + m_currency.formatAmount(tr.amount) + "+" + m_currency.formatAmount(FEE));

    LOG_TRACE("STEP 5 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));

    WaitForTransactionUpdated trasactionConfirmationObserver;
    trasactionConfirmationObserver.expectindTxId = txId;

    wallet2->addObserver(&trasactionConfirmationObserver);
    auto txUpdated = trasactionConfirmationObserver.promise.get_future();

    CHECK_AND_ASSERT_MES(mineBlock(), false, "mineBlock() failed");
    CHECK_AND_ASSERT_MES(mineBlock(), false, "mineBlock() failed");
    txUpdated.get();
    wallet2->getTransaction(txId, txInfo);
    wallet2->removeObserver(&trasactionConfirmationObserver);

    CHECK_AND_ASSERT_MES(txInfo.blockHeight <= inprocNode->getLastLocalBlockHeight(), false, "STEP 6 ASSERTION FAILED tx height confirmation failed");
    LOG_TRACE("STEP 6 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));


    CHECK_AND_ASSERT_MES(startMining(1), false, "startMining(1) failed");
    Semaphore confirmed2;
    Semaphore confirmed1;
    WaitForConfirmationObserver confirmationObserver2(confirmed2, [&wallet2](uint64_t actual)->bool {return wallet2->pendingBalance() == actual; });
    WaitForConfirmationObserver confirmationObserver1(confirmed1, [&wallet1](uint64_t actual)->bool {return wallet1->pendingBalance() == actual; });
    wallet2->addObserver(&confirmationObserver2);
    wallet1->addObserver(&confirmationObserver1);
    if (wallet2->pendingBalance() != wallet2->actualBalance()) confirmed2.wait();
    if (wallet1->pendingBalance() != wallet1->actualBalance()) confirmed1.wait();
    CHECK_AND_ASSERT_MES(stopMining(), false, "stopMining() failed");
    auto wallet1ActualAfterTransactionAndConfirmation = wallet1->actualBalance();
    auto wallet2ActualAfterTransactionAndConfirmation = wallet2->actualBalance();
    auto w2ActualDiff = wallet2ActualAfterTransactionAndConfirmation - wallet2ActualBeforeTransaction;
    auto w1ActualDiff = wallet1ActualBeforeTransaction - wallet1ActualAfterTransactionAndConfirmation;
    CHECK_AND_ASSERT_MES((tr.amount == w2ActualDiff), false, "STEP 7 FAILED\r\n Transfered amount " + m_currency.formatAmount(tr.amount) + " doesn't match confirmed recieved amount " + m_currency.formatAmount(w2ActualDiff));
    CHECK_AND_ASSERT_MES((w1ActualDiff - tr.amount - FEE == 0), false,
      "STEP 7 FAILED\r\n wallet1 Actual Before Transaction doesn't match wallet1 Actual After Transaction + Transfered amount + Fee "
      + m_currency.formatAmount(wallet1ActualBeforeTransaction) + " <> " + m_currency.formatAmount(wallet1ActualAfterTransactionAndConfirmation) + "+" + m_currency.formatAmount(tr.amount) + "+" + m_currency.formatAmount(FEE));
    LOG_TRACE("STEP 7 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));
    wallet1->removeObserver(&wallet1ActualGrown);
    wallet2->removeObserver(&pgo1);
    wallet2->removeObserver(&confirmationObserver2);
    wallet1->removeObserver(&confirmationObserver1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    return true;
  }


  bool perform6() {
    using namespace Tests::Common;
    using namespace CryptoNote;
    const uint64_t FEE = 1000000;
    launchTestnetWithInprocNode(2);

    std::unique_ptr<CryptoNote::INode> node1;
    std::unique_ptr<CryptoNote::INode> inprocNode;

    nodeDaemons.front()->makeINode(node1);
    nodeDaemons.back()->makeINode(inprocNode);

    while (node1->getLastLocalBlockHeight() != inprocNode->getLastLocalBlockHeight()) {
      LOG_TRACE("Syncing...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    LOG_TRACE("STEP 1 PASSED");

    std::unique_ptr<CryptoNote::IWalletLegacy> wallet1;
    std::unique_ptr<CryptoNote::IWalletLegacy> wallet2;

    makeWallet(wallet1, node1);
    makeWallet(wallet2, inprocNode);

    CHECK_AND_ASSERT_MES(mineBlock(), false, "can't mine block");
    CHECK_AND_ASSERT_MES(mineBlock(), false, "can't mine block");
    LOG_TRACE("STEP 2 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));

    CHECK_AND_ASSERT_MES(mineBlock(wallet1), false, "can't mine block on wallet 1");

    LOG_TRACE("STEP 3 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));

    Semaphore wallet1GotActual;
    WaitForConfirmationObserver wallet1ActualGrown(wallet1GotActual, [&wallet1](uint64_t actual)->bool {return wallet1->pendingBalance() == actual; });
    wallet1->addObserver(&wallet1ActualGrown);
    CHECK_AND_ASSERT_MES(startMining(1), false, "startMining(1) failed");
    wallet1GotActual.wait();

    LOG_TRACE("STEP 4 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));

    CHECK_AND_ASSERT_MES(stopMining(), false, "stopMining() failed");

    auto wallet1ActualBeforeTransaction = wallet1->actualBalance();
    auto wallet1PendingBeforeTransaction = wallet1->pendingBalance();
    auto wallet2PendingBeforeTransaction = wallet2->pendingBalance();
    CryptoNote::WalletLegacyTransfer tr;
    tr.address = wallet2->getAddress();
    tr.amount = wallet1ActualBeforeTransaction / 2;
    std::error_code result;
    Semaphore w2GotPending;
    WaitForPendingGrowObserver pgo1(w2GotPending, wallet2PendingBeforeTransaction);
    wallet2->addObserver(&pgo1);

    WaitForExternalTransactionObserver poolTxWaiter;
    auto future = poolTxWaiter.promise.get_future();
    wallet2->addObserver(&poolTxWaiter);

    wallet1->sendTransaction(tr, FEE);

    auto txId = future.get();
    w2GotPending.wait();

    wallet2->removeObserver(&poolTxWaiter);
    CryptoNote::WalletLegacyTransaction txInfo;
    wallet2->getTransaction(txId, txInfo);

    auto wallet2PendingAfterTransaction = wallet2->pendingBalance();
    auto wallet1PendingAfterTransaction = wallet1->pendingBalance();
    auto w2PendingDiff = wallet2PendingAfterTransaction - wallet2PendingBeforeTransaction;
    auto w1PendingDiff = wallet1PendingBeforeTransaction - wallet1PendingAfterTransaction;
    CHECK_AND_ASSERT_MES((txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT), false, "STEP 5 ASSERTION 1 FAILED\r\n Transaction blockHeight differs unconfirmed_tx_height");
    CHECK_AND_ASSERT_MES((tr.amount == txInfo.totalAmount), false, "STEP 5 ASSERTION 2 FAILED\r\n Transfered amount " + m_currency.formatAmount(tr.amount) + " doesn't match recieved amount from pool transaction " + m_currency.formatAmount(txInfo.totalAmount));
    CHECK_AND_ASSERT_MES((tr.amount == w2PendingDiff), false, "STEP 5 ASSERTION 3 FAILED\r\n Transfered amount " + m_currency.formatAmount(tr.amount) + " doesn't match recieved amount " + m_currency.formatAmount(w2PendingDiff));
    CHECK_AND_ASSERT_MES((w1PendingDiff - tr.amount - FEE == 0), false,
      "STEP 5 ASSERTION 4 FAILED\r\n wallet1 Pending Before Transaction doesn't match wallet1 Pending After Transaction + Transfered amount + Fee "
      + m_currency.formatAmount(wallet1PendingBeforeTransaction) + " <> " + m_currency.formatAmount(wallet1PendingAfterTransaction) + "+" + m_currency.formatAmount(tr.amount) + "+" + m_currency.formatAmount(FEE));

    LOG_TRACE("STEP 5 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));



    

    WaitForTransactionUpdated trasactionDeletionObserver;
    trasactionDeletionObserver.expectindTxId = txId;

    wallet2->addObserver(&trasactionDeletionObserver);
    auto txUpdated = trasactionDeletionObserver.promise.get_future();

    txUpdated.get();
    wallet2->getTransaction(txId, txInfo);
    wallet2->removeObserver(&trasactionDeletionObserver);

   
    CHECK_AND_ASSERT_MES(txInfo.state == WalletLegacyTransactionState::Deleted, false, "STEP 6 ASSERTION 1 FAILED tx not deleted");
    CHECK_AND_ASSERT_MES(wallet2PendingBeforeTransaction == wallet2->pendingBalance(), false, "STEP 6 ASSERTION 2 FAILED current pending balance <> pending balance before transaction");

    LOG_TRACE("STEP 6 PASSED");
    LOG_DEBUG("Wallet1 pending: " + m_currency.formatAmount(wallet1->pendingBalance()));
    LOG_DEBUG("Wallet1 actual:  " + m_currency.formatAmount(wallet1->actualBalance()));
    LOG_DEBUG("Wallet2 pending: " + m_currency.formatAmount(wallet2->pendingBalance()));
    LOG_DEBUG("Wallet2 actual:  " + m_currency.formatAmount(wallet2->actualBalance()));

    wallet1->removeObserver(&wallet1ActualGrown);
    wallet2->removeObserver(&pgo1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    return true;
  }


};


void testMultiVersion(const CryptoNote::Currency& currency, System::Dispatcher& d, const Tests::Common::BaseFunctionalTestsConfig& config);


class SimpleTestCase : public ::testing::Test {

public:

  SimpleTestCase() : 
    currency(CryptoNote::CurrencyBuilder(logger).testnet(true).currency()), 
    test(currency, dispatcher, baseCfg) {
  }

  System::Dispatcher dispatcher;
  Logging::ConsoleLogger logger;
  CryptoNote::Currency currency;
  SimpleTest test;
};

class SimpleTestCaseOtherConfig : public ::testing::Test {

public:

  SimpleTestCaseOtherConfig() : 
    currency(CryptoNote::CurrencyBuilder(logger).testnet(true).mempoolTxLiveTime(60).currency()),
    test(currency, dispatcher, baseCfg) {
  }

  System::Dispatcher dispatcher;
  Logging::ConsoleLogger logger;
  CryptoNote::Currency currency;
  SimpleTest test;
};

TEST_F(SimpleTestCase, WALLET2WALLET) {
  ASSERT_TRUE(test.perform1());
}

TEST_F(SimpleTestCase, BLOCKTHRUDAEMONS) {
  ASSERT_TRUE(test.perform2());
}

TEST_F(SimpleTestCase, RELAYBLOCKTHRUDAEMONS) {
  ASSERT_TRUE(test.perform4());
}

TEST_F(SimpleTestCase, TESTPOOLANDINPROCNODE) {
  ASSERT_TRUE(test.perform5());
}

TEST_F(SimpleTestCaseOtherConfig, TESTPOOLDELETION) {
  ASSERT_TRUE(test.perform6());
}

TEST_F(SimpleTestCase, MULTIVERSION) {
  ASSERT_NO_THROW(testMultiVersion(currency, dispatcher, baseCfg));
}

int main(int argc, char** argv) {
  CLogger::Instance().init(CLogger::DEBUG);

  try {
    ::Configuration config;
    if (!config.handleCommandLine(argc, argv)) {
      return 0; //help message requested or so
    }

    baseCfg = config;

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
  }
  catch (::ConfigurationError& ex) {
    std::cerr << "Configuration error: " << ex.what() << std::endl;
    return 1;
  }
  catch (std::exception& ex) {
    LOG_ERROR("Fatal error: " + std::string(ex.what()));
    return 1;
  }

  return 0;
}
