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

#include "gtest/gtest.h"
#include <numeric>

#include <System/Timer.h>
#include <Common/StringTools.h>
#include <Logging/ConsoleLogger.h>

#include "PaymentGate/WalletService.h"
#include "Wallet/WalletGreen.h"

// test helpers
#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"

using namespace PaymentService;
using namespace CryptoNote;

class PaymentGateTest : public testing::Test {
public:

  PaymentGateTest() :
    logger(Logging::ERROR),
    currency(CryptoNote::CurrencyBuilder(logger).currency()),
    generator(currency),
    nodeStub(generator) 
  {}

  WalletConfiguration createWalletConfiguration(const std::string& walletFile = "pgwalleg.bin") const {
    return WalletConfiguration{ walletFile, "pass" };
  }

  std::unique_ptr<WalletService> createWalletService(const WalletConfiguration& cfg) {
    WalletGreen* walletGreen = new CryptoNote::WalletGreen(dispatcher, currency, nodeStub, logger);
    wallet.reset(walletGreen);
    std::unique_ptr<WalletService> service(new WalletService(currency, dispatcher, nodeStub, *walletGreen, *walletGreen, cfg, logger));
    service->init();
    return service;
  }

  void generateWallet(const WalletConfiguration& conf) {
    unlink(conf.walletFile.c_str());
    generateNewWallet(currency, conf, logger, dispatcher);
  }

protected:  
  Logging::ConsoleLogger logger;
  CryptoNote::Currency currency;
  TestBlockchainGenerator generator;
  INodeTrivialRefreshStub nodeStub;
  System::Dispatcher dispatcher;

  std::unique_ptr<CryptoNote::IWallet> wallet;
};


TEST_F(PaymentGateTest, createWallet) {
  auto cfg = createWalletConfiguration();
  generateWallet(cfg);
  auto service = createWalletService(cfg);
}

TEST_F(PaymentGateTest, addTransaction) {
  auto cfg = createWalletConfiguration();
  generateWallet(cfg);
  auto service = createWalletService(cfg);

  std::string addressStr;
  ASSERT_TRUE(!service->createAddress(addressStr));

  AccountPublicAddress address;
  ASSERT_TRUE(currency.parseAccountAddressString(addressStr, address));

  generator.getBlockRewardForAddress(address);
  generator.getBlockRewardForAddress(address);
  generator.generateEmptyBlocks(11);
  generator.getBlockRewardForAddress(address);

  nodeStub.updateObservers();

  System::Timer(dispatcher).sleep(std::chrono::seconds(2));

  uint64_t pending = 0, actual = 0;

  service->getBalance(actual, pending);

  ASSERT_NE(0, pending);
  ASSERT_NE(0, actual);

  ASSERT_EQ(pending * 2, actual);
}

/*
TEST_F(PaymentGateTest, DISABLED_sendTransaction) {

  auto cfg = createWalletConfiguration();
  generateWallet(cfg);
  auto service = createWalletService(cfg);

  std::string addressStr;
  ASSERT_TRUE(!service->createAddress(addressStr));

  AccountPublicAddress address;
  ASSERT_TRUE(currency.parseAccountAddressString(addressStr, address));

  generator.getBlockRewardForAddress(address);
  generator.generateEmptyBlocks(11);

  nodeStub.updateObservers();

  System::Timer(dispatcher).sleep(std::chrono::seconds(5));

  auto cfg2 = createWalletConfiguration("pgwallet2.bin");
  generateWallet(cfg2);
  auto serviceRecv = createWalletService(cfg2);

  std::string recvAddress;
  serviceRecv->createAddress(recvAddress);

  uint64_t TEST_AMOUNT = 0;
  currency.parseAmount("100000.0", TEST_AMOUNT);

  Crypto::Hash paymentId;
  std::iota(reinterpret_cast<char*>(&paymentId), reinterpret_cast<char*>(&paymentId) + sizeof(paymentId), 0);
  std::string paymentIdStr = Common::podToHex(paymentId);

  uint64_t txId = 0;

  {
    SendTransaction::Request req;
    SendTransaction::Response res;

    req.transfers.push_back(WalletRpcOrder{ TEST_AMOUNT, recvAddress });
    req.fee = currency.minimumFee();
    req.anonymity = 1;
    req.unlockTime = 0;
    req.paymentId = paymentIdStr;

    ASSERT_TRUE(!service->sendTransaction(req, res.transactionHash));

    txId = res.transactionId;
  }

  generator.generateEmptyBlocks(11);

  nodeStub.updateObservers();

  System::Timer(dispatcher).sleep(std::chrono::seconds(5));

  TransactionRpcInfo txInfo;
  bool found = false;

  ASSERT_TRUE(!service->getTransaction(txId, found, txInfo));
  ASSERT_TRUE(found);

  uint64_t recvTxCount = 0;
  ASSERT_TRUE(!serviceRecv->getTransactionsCount(recvTxCount));
  ASSERT_EQ(1, recvTxCount);

  uint64_t sendTxCount = 0;
  ASSERT_TRUE(!service->getTransactionsCount(sendTxCount));
  ASSERT_EQ(2, sendTxCount); // 1 from mining, 1 transfer

  TransactionRpcInfo recvTxInfo;
  ASSERT_TRUE(!serviceRecv->getTransaction(0, found, recvTxInfo));
  ASSERT_TRUE(found);

  ASSERT_EQ(txInfo.hash, recvTxInfo.hash);
  ASSERT_EQ(txInfo.extra, recvTxInfo.extra);
  ASSERT_EQ(-txInfo.totalAmount - currency.minimumFee(), recvTxInfo.totalAmount);
  ASSERT_EQ(txInfo.blockHeight, recvTxInfo.blockHeight);

  {
    // check payments
    WalletService::IncomingPayments payments;
    ASSERT_TRUE(!serviceRecv->getIncomingPayments({ paymentIdStr }, payments));

    ASSERT_EQ(1, payments.size());

    ASSERT_EQ(paymentIdStr, payments.begin()->first);

    const auto& recvPayment = payments.begin()->second;

    ASSERT_EQ(1, recvPayment.size());

    ASSERT_EQ(txInfo.hash, recvPayment[0].txHash);
    ASSERT_EQ(TEST_AMOUNT, recvPayment[0].amount);
    ASSERT_EQ(txInfo.blockHeight, recvPayment[0].blockHeight);
  }

  // reload services

  service->saveWallet();
  serviceRecv->saveWallet();

  service.reset();
  serviceRecv.reset();

  service = createWalletService(cfg);
  serviceRecv = createWalletService(cfg2);

  recvTxInfo = boost::value_initialized<TransactionRpcInfo>();
  ASSERT_TRUE(!serviceRecv->getTransaction(0, found, recvTxInfo));
  ASSERT_TRUE(found);

  ASSERT_EQ(txInfo.hash, recvTxInfo.hash);
  ASSERT_EQ(txInfo.extra, recvTxInfo.extra);
  ASSERT_EQ(-txInfo.totalAmount - currency.minimumFee(), recvTxInfo.totalAmount);
  ASSERT_EQ(txInfo.blockHeight, recvTxInfo.blockHeight);

  // send some money back
  std::reverse(paymentIdStr.begin(), paymentIdStr.end());

  {
    std::string recvAddress;
    service->createAddress(recvAddress);

    SendTransactionRequest req;
    SendTransactionResponse res;

    req.destinations.push_back(TransferDestination{ TEST_AMOUNT/2, recvAddress });
    req.fee = currency.minimumFee();
    req.mixin = 1;
    req.unlockTime = 0;
    req.paymentId = paymentIdStr;

    ASSERT_TRUE(!serviceRecv->sendTransaction(req, res));

    txId = res.transactionId;
  }

  generator.generateEmptyBlocks(11);
  nodeStub.updateObservers();

  System::Timer(dispatcher).sleep(std::chrono::seconds(5));

  ASSERT_TRUE(!service->getTransactionsCount(recvTxCount));
  ASSERT_EQ(3, recvTxCount);

  {
    WalletService::IncomingPayments payments;
    ASSERT_TRUE(!service->getIncomingPayments({ paymentIdStr }, payments));
    ASSERT_EQ(1, payments.size());
    ASSERT_EQ(paymentIdStr, payments.begin()->first);

    const auto& recvPayment = payments.begin()->second;

    ASSERT_EQ(1, recvPayment.size());
    ASSERT_EQ(TEST_AMOUNT / 2, recvPayment[0].amount);
  }
} */
