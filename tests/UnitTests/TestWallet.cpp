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

#include <chrono>
#include <fstream>
#include <numeric>
#include <system_error>
#include <tuple>

#include "Common/StringTools.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "CryptoNoteCore/TransactionApiExtra.h"
#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"
#include "TransactionApiHelpers.h"
#include <Logging/ConsoleLogger.h>
#include "Wallet/WalletErrors.h"
#include "Wallet/WalletGreen.h"
#include "Wallet/WalletSerializationV2.h"
#include "Wallet/WalletUtils.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "WalletLegacy/WalletLegacySerializer.h"
#include <System/Dispatcher.h>
#include <System/Timer.h>
#include <System/Context.h>

#include "TransactionApiHelpers.h"

using namespace Crypto;
using namespace Common;
using namespace CryptoNote;

namespace CryptoNote {

    std::ostream& operator<<(std::ostream& o, const WalletTransaction& tx) {
      o << "WalletTransaction{state=" << tx.state << ", timestamp=" << tx.timestamp
        << ", blockHeight=" << tx.blockHeight << ", hash=" << tx.hash
        << ", totalAmount=" << tx.totalAmount << ", fee=" << tx.fee
        << ", creationTime=" << tx.creationTime << ", unlockTime=" << tx.unlockTime
        << ", extra=" << tx.extra  << ", isBase=" << tx.isBase << "}";
      return o;
    }

    bool operator==(const WalletTransaction& lhs, const WalletTransaction& rhs) {
      if (lhs.state != rhs.state) {
        return false;
      }

      if (lhs.timestamp != rhs.timestamp) {
        return false;
      }

      if (lhs.blockHeight != rhs.blockHeight) {
        return false;
      }

      if (lhs.hash != rhs.hash) {
        return false;
      }

      if (lhs.totalAmount != rhs.totalAmount) {
        return false;
      }

      if (lhs.fee != rhs.fee) {
        return false;
      }

      if (lhs.creationTime != rhs.creationTime) {
        return false;
      }

      if (lhs.unlockTime != rhs.unlockTime) {
        return false;
      }

      if (lhs.extra != rhs.extra) {
        return false;
      }

      if (lhs.isBase != rhs.isBase) {
        return false;
      }

      return true;
    }

    bool operator!=(const WalletTransaction& lhs, const WalletTransaction& rhs) {
      return !(lhs == rhs);
    }

    bool operator==(const WalletTransfer& lhs, const WalletTransfer& rhs) {
      if (lhs.address != rhs.address) {
        return false;
      }

      if (lhs.amount != rhs.amount) {
        return false;
      }

      if (lhs.type != rhs.type) {
        return false;
      }

      return true;
    }

    bool operator!=(const WalletTransfer& lhs, const WalletTransfer& rhs) {
      return !(lhs == rhs);
    }

    bool operator==(const IFusionManager::EstimateResult& lhs, const IFusionManager::EstimateResult& rhs) {
      return lhs.fusionReadyCount == rhs.fusionReadyCount && lhs.totalOutputCount == rhs.totalOutputCount;
    }

    bool operator<(const WalletTransfer& lhs, const WalletTransfer& rhs) {
      return std::make_tuple(lhs.amount, lhs.address) < std::make_tuple(rhs.amount, rhs.address);
    }
}

class WalletApi: public ::testing::Test {
public:
  WalletApi() :
    TRANSACTION_SOFTLOCK_TIME(10),
    logger(Logging::ERROR),
    currency(CryptoNote::CurrencyBuilder(logger).currency()),
    generator(currency),
    node(generator),
    alice(dispatcher, currency, node, logger),
    FEE(currency.minimumFee()),
    FUSION_THRESHOLD(currency.defaultDustThreshold() * 10)
  {
    CryptoNote::AccountBase randomAccount;
    randomAccount.generate();
    RANDOM_ADDRESS = currency.accountAddressAsString(randomAccount);
  }

  virtual void SetUp() override;
  virtual void TearDown() override;

protected:
  void cleanUpWalletFiles() const;
  CryptoNote::AccountPublicAddress parseAddress(const std::string& address);
  void generateBlockReward();
  void generateBlockReward(const std::string& address);
  void generateAndUnlockMoney();
  void generateAddressesWithPendingMoney(size_t count);
  void generateFusionOutputsAndUnlock(WalletGreen& wallet, INodeTrivialRefreshStub& node,
    const CryptoNote::Currency& walletCurrency, uint64_t threshold, size_t addressIndex = 0);
  void unlockMoney();
  void unlockMoney(CryptoNote::WalletGreen& wallet, INodeTrivialRefreshStub& inode);
  void setMinerTo(CryptoNote::WalletGreen& wallet);

  void waitForPredicate(CryptoNote::WalletGreen& wallet, std::function<bool()>&& pred, std::chrono::nanoseconds timeout = std::chrono::seconds(30));

  template<typename T>
  void waitValueChanged(CryptoNote::WalletGreen& wallet, T prev, std::function<T()>&& f, std::chrono::nanoseconds timeout = std::chrono::seconds(30));

  template<typename T>
  void waitForValue(CryptoNote::WalletGreen& wallet, T value, std::function<T()>&& f, std::chrono::nanoseconds timeout = std::chrono::seconds(30));

  bool waitForWalletEvent(CryptoNote::WalletGreen& wallet, CryptoNote::WalletEventType eventType, std::chrono::nanoseconds timeout);

  void waitActualBalanceUpdated();
  void waitActualBalanceUpdated(uint64_t prev);
  void waitActualBalanceUpdated(CryptoNote::WalletGreen& wallet, uint64_t prev);

  void waitPendingBalanceUpdated();
  void waitPendingBalanceUpdated(uint64_t prev);
  void waitPendingBalanceUpdated(CryptoNote::WalletGreen& wallet, uint64_t prev);

  void waitForTransactionCount(CryptoNote::WalletGreen& wallet, uint64_t expected);
  void waitForTransactionConfirmed(CryptoNote::WalletGreen& wallet, size_t transactionId,
    size_t confirmations = 1, std::chrono::nanoseconds timeout = std::chrono::seconds(5));
  void waitForTransactionUpdated(CryptoNote::WalletGreen& wallet, size_t expectedTransactionId, std::chrono::nanoseconds timeout = std::chrono::seconds(30));
  void waitForActualBalance(uint64_t expected);
  void waitForActualBalance(CryptoNote::WalletGreen& wallet, uint64_t expected);

  size_t sendMoneyToRandomAddressFrom(const std::string& address, uint64_t amount, uint64_t fee, const std::string& changeDestination);
  size_t sendMoneyToRandomAddressFrom(const std::string& address, const std::string& changeDestination);

  static size_t sendMoney(CryptoNote::WalletGreen& wallet, const std::vector<std::string>& sourceAdresses, const std::string& to,
    uint64_t amount, uint64_t fee, uint16_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);
  static size_t sendMoney(CryptoNote::WalletGreen& wallet, const std::string& to, uint64_t amount, uint64_t fee,
    uint16_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);
  size_t sendMoney(const std::string& to, uint64_t amount, uint64_t fee, uint16_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);
  size_t sendMoneyWithDonation(const std::string& to, uint64_t amount, uint64_t fee,
    const std::string& donationAddress, uint64_t donationAmount, uint16_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);

  size_t makeTransaction(const std::vector<std::string>& sourceAdresses, const std::string& to, uint64_t amount, uint64_t fee, uint16_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);
  size_t makeTransaction(CryptoNote::WalletGreen& wallet, const std::vector<std::string>& sourceAdresses, const std::string& to, uint64_t amount, uint64_t fee, uint16_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);
  size_t makeTransaction(const std::vector<std::string>& sourceAdresses, const std::vector<CryptoNote::WalletOrder>& orders, uint64_t fee, uint16_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);
  size_t makeTransaction(CryptoNote::WalletGreen& wallet, const std::vector<std::string>& sourceAdresses, const std::vector<CryptoNote::WalletOrder>& orders, uint64_t fee, uint16_t mixIn = 0, const std::string& extra = "", uint64_t unlockTimestamp = 0);

  void fillWalletWithDetailsCache();

  void wait(uint64_t milliseconds);
  void testIWalletDataCompatibility(bool details, const std::string& cache = std::string(),
          const std::vector<WalletLegacyTransaction>& txs = std::vector<WalletLegacyTransaction>(),
          const std::vector<WalletLegacyTransfer>& trs = std::vector<WalletLegacyTransfer>(),
          const std::vector<std::pair<TransactionInformation, int64_t>>& externalTxs = std::vector<std::pair<TransactionInformation, int64_t>>());

  uint32_t TRANSACTION_SOFTLOCK_TIME;

  System::Dispatcher dispatcher;
  Logging::ConsoleLogger logger;
  CryptoNote::Currency currency;
  TestBlockchainGenerator generator;
  INodeTrivialRefreshStub node;
  CryptoNote::WalletGreen alice;
  std::string aliceAddress;

  const uint64_t SENT = 1122334455;
  const uint64_t FEE;
  std::string RANDOM_ADDRESS;
  const uint64_t FUSION_THRESHOLD;
  const std::string ALICE_WALLET_PATH = "alice.wallet";
  const std::string BOB_WALLET_PATH = "bob.wallet";
  const std::string BOB_WALLET_BACKUP_PATH = BOB_WALLET_PATH + ".backup";
};

void WalletApi::SetUp() {
  cleanUpWalletFiles();

  alice.initialize(ALICE_WALLET_PATH, "pass");
  aliceAddress = alice.createAddress();
}

void WalletApi::TearDown() {
  alice.shutdown();
  wait(100); //ObserverManager bug workaround

  cleanUpWalletFiles();
}

void WalletApi::cleanUpWalletFiles() const {
  if (boost::filesystem::exists(ALICE_WALLET_PATH)) {
    boost::filesystem::remove(ALICE_WALLET_PATH);
  }

  if (boost::filesystem::exists(BOB_WALLET_PATH)) {
    boost::filesystem::remove(BOB_WALLET_PATH);
  }

  if (boost::filesystem::exists(BOB_WALLET_BACKUP_PATH)) {
    boost::filesystem::remove(BOB_WALLET_BACKUP_PATH);
  }
}

void WalletApi::setMinerTo(CryptoNote::WalletGreen& wallet) {
  AccountBase base;
  AccountKeys keys;
  auto viewKey = wallet.getViewKey();
  auto spendKey = wallet.getAddressSpendKey(0);
  keys.address.spendPublicKey = spendKey.publicKey;
  keys.address.viewPublicKey = viewKey.publicKey;
  keys.viewSecretKey = viewKey.secretKey;
  keys.spendSecretKey = spendKey.secretKey;
  base.setAccountKeys(keys);
  generator.setMinerAccount(base);
}

CryptoNote::AccountPublicAddress WalletApi::parseAddress(const std::string& address) {
  CryptoNote::AccountPublicAddress pubAddr;
  if (!currency.parseAccountAddressString(address, pubAddr)) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }

  return pubAddr;
}

void WalletApi::generateBlockReward() {
  generateBlockReward(aliceAddress);
}

void WalletApi::generateBlockReward(const std::string& address) {
  generator.getBlockRewardForAddress(parseAddress(address));
}

void WalletApi::generateFusionOutputsAndUnlock(WalletGreen& wallet, INodeTrivialRefreshStub& node,
  const CryptoNote::Currency& walletCurrency, uint64_t threshold, size_t addressIndex) {

  uint64_t digit = walletCurrency.defaultDustThreshold();
  uint64_t mul = 1;

  while (digit > 9) {
    digit /= 10;
    mul *= 10;
  }

  auto initialAmount = wallet.getActualBalance();
  auto expectedTxCount = wallet.getTransactionCount();

  CryptoNote::AccountPublicAddress publicAddress = parseAddress(wallet.getAddress(addressIndex));
  const size_t POWERS_COUNT = 3;

  uint64_t addedAmount = 0;
  for (size_t power = 0; power < POWERS_COUNT; ++power) {
    int start = power == 0 ? static_cast<int>(digit): 1;
    if (start * mul > threshold) {
      break;
    }

    for (int count = 0, d = start; count < walletCurrency.fusionTxMinInputCount() && start * mul < threshold; ++count) {
      //TODO: make it possible to put several outputs to one transaction
      auto amount = d * mul;
      generator.getSingleOutputTransaction(publicAddress, amount);
      addedAmount += amount;
      ++expectedTxCount;

      if (++d > 9 || amount >= threshold) {
        d = start;
      }
    }

    mul *= 10;
  }

  assert(addedAmount > 0);

  generator.generateEmptyBlocks(11);
  node.updateObservers();

  waitForActualBalance(wallet, initialAmount + addedAmount);
  waitForTransactionCount(wallet, expectedTxCount);
}

void WalletApi::unlockMoney() {
  unlockMoney(alice, node);
}

void WalletApi::unlockMoney(CryptoNote::WalletGreen& wallet, INodeTrivialRefreshStub& inode) {
  auto prev = wallet.getActualBalance();
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow()); //coinbase money should become available after 10 blocks
  inode.updateObservers();
  waitActualBalanceUpdated(wallet, prev);
}

void WalletApi::generateAndUnlockMoney() {
  generateBlockReward();
  unlockMoney();
}

void WalletApi::waitForPredicate(CryptoNote::WalletGreen& wallet, std::function<bool()>&& pred, std::chrono::nanoseconds timeout) {
  System::Context<> waitContext(dispatcher, [&wallet, &pred]() {
    while (!pred()) {
      wallet.getEvent();
    }
  });

  System::Context<> timeoutContext(dispatcher, [this, &waitContext, timeout] {
    System::Timer(dispatcher).sleep(timeout);
    waitContext.interrupt();
  });

  waitContext.get();
}

template<typename T>
void WalletApi::waitValueChanged(CryptoNote::WalletGreen& wallet, T prev, std::function<T()>&& f, std::chrono::nanoseconds timeout) {
  waitForPredicate(wallet, [&f, &prev] { return prev != f(); }, timeout);
}

template<typename T>
void WalletApi::waitForValue(CryptoNote::WalletGreen& wallet, T value, std::function<T()>&& f, std::chrono::nanoseconds timeout) {
  waitForPredicate(wallet, [&f, value] { return value == f(); }, timeout);
}

bool WalletApi::waitForWalletEvent(CryptoNote::WalletGreen& wallet, CryptoNote::WalletEventType eventType, std::chrono::nanoseconds timeout) {
  System::Context<> eventContext(dispatcher, [&wallet, eventType] () {
    CryptoNote::WalletEvent event;

    do {
      event = wallet.getEvent();
    } while(event.type != eventType);
  });

  System::Context<> timeoutContext(dispatcher, [timeout, &eventContext, this] {
    System::Timer(dispatcher).sleep(timeout);
    eventContext.interrupt();
  });

  try {
    eventContext.get();
    return true;
  } catch (System::InterruptedException&) {
    return false;
  }
}

void WalletApi::waitActualBalanceUpdated() {
  waitActualBalanceUpdated(alice, alice.getActualBalance());
}

void WalletApi::waitActualBalanceUpdated(uint64_t prev) {
  waitActualBalanceUpdated(alice, prev);
}

void WalletApi::waitForActualBalance(uint64_t expected) {
  waitForValue<uint64_t>(alice, expected, [this] () { return this->alice.getActualBalance(); });
}

void WalletApi::waitForActualBalance(CryptoNote::WalletGreen& wallet, uint64_t expected) {
  waitForValue<uint64_t>(wallet, expected, [&wallet] () { return wallet.getActualBalance(); });
}

void WalletApi::waitActualBalanceUpdated(CryptoNote::WalletGreen& wallet, uint64_t prev) {
  waitValueChanged<uint64_t>(wallet, prev, [&wallet] () { return wallet.getActualBalance(); });
}

void WalletApi::waitPendingBalanceUpdated() {
  waitPendingBalanceUpdated(alice, alice.getPendingBalance());
}

void WalletApi::waitPendingBalanceUpdated(uint64_t prev) {
  waitPendingBalanceUpdated(alice, prev);
}

void WalletApi::waitPendingBalanceUpdated(CryptoNote::WalletGreen& wallet, uint64_t prev) {
  waitValueChanged<uint64_t>(wallet, prev, [&wallet] () { return wallet.getPendingBalance(); });
}

void WalletApi::waitForTransactionCount(CryptoNote::WalletGreen& wallet, uint64_t expected) {
  waitForValue<size_t>(wallet, expected, [&wallet] () { return wallet.getTransactionCount(); });
}

void WalletApi::waitForTransactionUpdated(CryptoNote::WalletGreen& wallet, size_t expectedTransactionId, std::chrono::nanoseconds timeout) {
  System::Context<> waitContext(dispatcher, [&wallet, expectedTransactionId]() {
    for (;;) {
      WalletEvent event = wallet.getEvent();
      if (event.type == WalletEventType::TRANSACTION_UPDATED && event.transactionUpdated.transactionIndex == expectedTransactionId) {
        break;
      }
    }
  });

  System::Context<> timeoutContext(dispatcher, [this, &waitContext, timeout] {
    System::Timer(dispatcher).sleep(timeout);
    waitContext.interrupt();
  });

  waitContext.get();
}

void WalletApi::waitForTransactionConfirmed(CryptoNote::WalletGreen& wallet, size_t transactionId, size_t confirmations, std::chrono::nanoseconds timeout) {
  assert(confirmations > 0);

  waitForPredicate(wallet,
    [&wallet, transactionId, confirmations] {
      auto tx = wallet.getTransaction(transactionId);
      return tx.blockHeight + confirmations <= wallet.getBlockCount();
    },
    timeout);
}

void WalletApi::generateAddressesWithPendingMoney(size_t count) {
  for (size_t i = 0; i < count; ++i) {
    generateBlockReward(alice.createAddress());
  }
}

size_t WalletApi::sendMoneyToRandomAddressFrom(const std::string& address, uint64_t amount, uint64_t fee, const std::string& changeDestination) {
  CryptoNote::WalletOrder order;
  order.address = RANDOM_ADDRESS;
  order.amount = amount;

  CryptoNote::TransactionParameters params;
  params.sourceAddresses = {address};
  params.destinations = {order};
  params.fee = fee;
  params.changeDestination = changeDestination;
  return alice.transfer(params);
}

size_t WalletApi::sendMoneyToRandomAddressFrom(const std::string& address, const std::string& changeDestination) {
  return sendMoneyToRandomAddressFrom(address, SENT, FEE, changeDestination);
}

void WalletApi::fillWalletWithDetailsCache() {
  generateAddressesWithPendingMoney(10);
  unlockMoney();

  auto alicePrev = alice.getActualBalance();
  for (size_t i = 1; i < 5; ++i) {
    sendMoneyToRandomAddressFrom(alice.getAddress(i), alice.getAddress(0));
  }

  node.updateObservers();
  waitActualBalanceUpdated(alicePrev);

  for (size_t i = 5; i < 10; ++i) {
    sendMoneyToRandomAddressFrom(alice.getAddress(i), alice.getAddress(0));
  }
}

size_t WalletApi::sendMoney(CryptoNote::WalletGreen& wallet, const std::vector<std::string>& sourceAdresses, const std::string& to,
  uint64_t amount, uint64_t fee, uint16_t mixIn, const std::string& extra, uint64_t unlockTimestamp) {

  CryptoNote::WalletOrder order;
  order.address = to;
  order.amount = amount;

  CryptoNote::TransactionParameters params;
  params.sourceAddresses = sourceAdresses;
  params.destinations = { order };
  params.fee = fee;
  params.mixIn = mixIn;
  params.extra = extra;
  params.unlockTimestamp = unlockTimestamp;
  params.changeDestination = wallet.getAddress(0);

  return wallet.transfer(params);
}

size_t WalletApi::sendMoney(CryptoNote::WalletGreen& wallet, const std::string& to, uint64_t amount, uint64_t fee, uint16_t mixIn, const std::string& extra, uint64_t unlockTimestamp) {
  return sendMoney(wallet, {}, to, amount, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletApi::sendMoney(const std::string& to, uint64_t amount, uint64_t fee, uint16_t mixIn, const std::string& extra, uint64_t unlockTimestamp) {
  return sendMoney(alice, to, amount, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletApi::sendMoneyWithDonation(const std::string& to, uint64_t amount, uint64_t fee,
  const std::string& donationAddress, uint64_t donationAmount, uint16_t mixIn, const std::string& extra, uint64_t unlockTimestamp) {

  TransactionParameters params;
  params.destinations.push_back({to, amount});
  params.fee = fee;
  params.donation.address = donationAddress;
  params.donation.threshold = donationAmount;
  params.mixIn = mixIn;
  params.extra = extra;
  params.unlockTimestamp = unlockTimestamp;

  return alice.transfer(params);
}

size_t WalletApi::makeTransaction(
  const std::vector<std::string>& sourceAdresses,
  const std::string& to,
  uint64_t amount,
  uint64_t fee,
  uint16_t mixIn,
  const std::string& extra,
  uint64_t unlockTimestamp) {

  return makeTransaction(alice, sourceAdresses, to, amount, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletApi::makeTransaction(
  CryptoNote::WalletGreen& wallet,
  const std::vector<std::string>& sourceAdresses,
  const std::string& to,
  uint64_t amount,
  uint64_t fee,
  uint16_t mixIn,
  const std::string& extra,
  uint64_t unlockTimestamp) {

  CryptoNote::TransactionParameters params;
  params.destinations = { {to, amount} };
  params.sourceAddresses = sourceAdresses;
  params.fee = fee;
  params.mixIn = mixIn;
  params.extra = extra;
  params.unlockTimestamp = unlockTimestamp;

  return wallet.makeTransaction(params);
}

size_t WalletApi::makeTransaction(
  const std::vector<std::string>& sourceAdresses,
  const std::vector<CryptoNote::WalletOrder>& orders,
  uint64_t fee,
  uint16_t mixIn,
  const std::string& extra,
  uint64_t unlockTimestamp) {

  return makeTransaction(alice, sourceAdresses, orders, fee, mixIn, extra, unlockTimestamp);
}

size_t WalletApi::makeTransaction(
  CryptoNote::WalletGreen& wallet,
  const std::vector<std::string>& sourceAdresses,
  const std::vector<CryptoNote::WalletOrder>& orders,
  uint64_t fee,
  uint16_t mixIn,
  const std::string& extra,
  uint64_t unlockTimestamp) {

  CryptoNote::TransactionParameters params;
  params.destinations = orders;
  params.sourceAddresses = sourceAdresses;
  params.fee = fee;
  params.mixIn = mixIn;
  params.extra = extra;
  params.unlockTimestamp = unlockTimestamp;

  return wallet.makeTransaction(params);
}

void WalletApi::wait(uint64_t milliseconds) {
  System::Timer timer(dispatcher);
  timer.sleep(std::chrono::nanoseconds(milliseconds * 1000000));
}

auto transfersAmountSortingFunction = [] (const CryptoNote::WalletTransfer& lhs, const CryptoNote::WalletTransfer& rhs) {
  return lhs.amount < rhs.amount;
};

std::vector<CryptoNote::WalletTransfer> getTransfersFromTransaction(CryptoNote::WalletGreen& wallet, size_t transactionId, bool isPositiveAmount) {
  std::vector<CryptoNote::WalletTransfer> transfers;
  size_t transfersCount = wallet.getTransactionTransferCount(transactionId);

  for (size_t i = 0; i < transfersCount; ++i) {
    WalletTransfer transfer = wallet.getTransactionTransfer(transactionId, i);

    if (isPositiveAmount == (transfer.amount >= 0)) {
      transfers.push_back(std::move(transfer));
    }
  }

  return transfers;
}

void sortTransfersByAmount(std::vector<CryptoNote::WalletTransfer>& transfers) {
  std::sort(transfers.begin(), transfers.end(), transfersAmountSortingFunction); //sort by amount
}

//returns sorted transfers by amount
std::vector<CryptoNote::WalletTransfer> getTransfersFromTransaction(CryptoNote::WalletGreen& wallet, size_t transactionId) {
  auto result = getTransfersFromTransaction(wallet, transactionId, true);
  auto neg = getTransfersFromTransaction(wallet, transactionId, false);

  result.insert(result.end(), neg.begin(), neg.end());

  sortTransfersByAmount(result);

  return result;
}

static const uint64_t TEST_BLOCK_REWARD = 70368744177663;

TEST_F(WalletApi, initializeThrowsExceptionIfWalletFileAlreadyExists) {
  std::ofstream file(BOB_WALLET_PATH);
  file.close();
  ASSERT_TRUE(boost::filesystem::exists(BOB_WALLET_PATH));

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_THROW(bob.initialize(BOB_WALLET_PATH, "pass2"), std::system_error);

  wait(100);
}

TEST_F(WalletApi, initializeCreatesWalletThatCanBeLoaded) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  auto walletViewKey = bob.getViewKey();
  bob.shutdown();

  CryptoNote::WalletGreen carol(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  carol.load(BOB_WALLET_PATH, "pass2");
  ASSERT_EQ(walletViewKey.publicKey, carol.getViewKey().publicKey);
  ASSERT_EQ(walletViewKey.secretKey, carol.getViewKey().secretKey);
  carol.shutdown();

  wait(100);
}

TEST_F(WalletApi, addressPreservedEvenIfSaveWasNotCalled) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  auto address = bob.createAddress();
  bob.shutdown();

  CryptoNote::WalletGreen carol(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  carol.load(BOB_WALLET_PATH, "pass2");
  ASSERT_EQ(1, carol.getAddressCount());
  ASSERT_EQ(address, carol.getAddress(0));
  carol.shutdown();

  wait(100);
}

TEST_F(WalletApi, loadThrowsExceptionIfWalletFileDoesNotExist) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_THROW(bob.load(BOB_WALLET_PATH, "pass2"), std::system_error);
}

TEST_F(WalletApi, loadThrowsExceptionIfWalletFileHasInvalidVersion) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  bob.shutdown();

  std::fstream file(BOB_WALLET_PATH, std::ios::in | std::ios::out | std::ios::binary);
  file.seekp(0);
  char wrongVersion = static_cast<char>(CryptoNote::WalletSerializerV2::SERIALIZATION_VERSION + 1);
  file.write(&wrongVersion, sizeof(wrongVersion));
  file.close();

  CryptoNote::WalletGreen carol(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_THROW(carol.load(BOB_WALLET_PATH, "pass2"), std::system_error);
}

TEST_F(WalletApi, loadThrowsExceptionIfWalletFileIsEmpty) {
  std::ofstream file(BOB_WALLET_PATH);
  file.close();
  ASSERT_TRUE(boost::filesystem::exists(BOB_WALLET_PATH));

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_THROW(bob.load(BOB_WALLET_PATH, "pass2"), std::system_error);
}

TEST_F(WalletApi, loadThrowsExceptionIfWalletFileIsCorrupted) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  bob.shutdown();

  boost::filesystem::rename(BOB_WALLET_PATH, BOB_WALLET_BACKUP_PATH);
  std::ifstream src(BOB_WALLET_BACKUP_PATH, std::ios::binary);
  std::ofstream dst(BOB_WALLET_PATH, std::ios::binary);
  std::copy_n(std::istreambuf_iterator<char>(src), boost::filesystem::file_size(BOB_WALLET_BACKUP_PATH) - 1, std::ostreambuf_iterator<char>(dst));
  src.close();
  dst.close();

  CryptoNote::WalletGreen carol(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_ANY_THROW(carol.load(BOB_WALLET_PATH, "pass2"));
}

TEST_F(WalletApi, emptyBalance) {
  ASSERT_EQ(0, alice.getActualBalance());
  ASSERT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, receiveMoneyOneAddress) {
  generateBlockReward();

  auto prev = alice.getPendingBalance();
  node.updateObservers();
  waitPendingBalanceUpdated(prev);

  ASSERT_EQ(0, alice.getActualBalance());
  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getPendingBalance());

  ASSERT_EQ(0, alice.getActualBalance(aliceAddress));
  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getPendingBalance(aliceAddress));
}

TEST_F(WalletApi, unlockMoney) {
  generateAndUnlockMoney();

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance());
  ASSERT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, transferFromOneAddress) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  std::string bobAddress = bob.createAddress();

  generateAndUnlockMoney();

  auto alicePrev = alice.getActualBalance();
  sendMoney(bobAddress, SENT, FEE);
  node.updateObservers();

  waitActualBalanceUpdated(alicePrev);
  waitPendingBalanceUpdated(bob, 0);

  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(SENT, bob.getPendingBalance());

  ASSERT_EQ(TEST_BLOCK_REWARD - SENT - FEE, alice.getActualBalance() + alice.getPendingBalance());
  ASSERT_EQ(TEST_BLOCK_REWARD - SENT - FEE, alice.getActualBalance(aliceAddress) + alice.getPendingBalance(aliceAddress));

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, pendingBalanceUpdatedAfterTransactionGotInBlock) {
  generateAndUnlockMoney();

  auto initialActual = alice.getActualBalance();

  sendMoney(RANDOM_ADDRESS, SENT, FEE);
  node.updateObservers();
  waitActualBalanceUpdated(initialActual);
  waitPendingBalanceUpdated(0);

  auto prevPending = alice.getPendingBalance();

  generator.generateEmptyBlocks(static_cast<size_t>(TRANSACTION_SOFTLOCK_TIME));
  node.updateObservers();

  waitPendingBalanceUpdated(prevPending);
  ASSERT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, moneyLockedIfTransactionIsSoftLocked) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");

  sendMoney(bob.createAddress(), SENT, FEE);
  generator.generateEmptyBlocks(static_cast<size_t>(TRANSACTION_SOFTLOCK_TIME - 1));
  node.updateObservers();

  waitPendingBalanceUpdated(bob, 0);

  ASSERT_EQ(SENT, bob.getPendingBalance());
  ASSERT_EQ(0, bob.getActualBalance());

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, transferMixin) {
  generateAndUnlockMoney();

  auto alicePrev = alice.getActualBalance();

  ASSERT_NO_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE, 12));
  node.updateObservers();

  waitActualBalanceUpdated(alicePrev);

  auto tx = alice.getTransaction(0);
  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);
}

TEST_F(WalletApi, transferTooBigMixin) {
  generateAndUnlockMoney();

  node.setMaxMixinCount(10);
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE, 15));
}

TEST_F(WalletApi, transferNegativeAmount) {
  generateAndUnlockMoney();
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, -static_cast<int64_t>(SENT), FEE));
}

TEST_F(WalletApi, transferFromTwoAddresses) {
  generateBlockReward();
  generateBlockReward(alice.createAddress());
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();

  waitForActualBalance(2 * TEST_BLOCK_REWARD);

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  std::string bobAddress = bob.createAddress();

  const uint64_t sent = 2 * TEST_BLOCK_REWARD - 10 * FEE;

  auto bobPrev = bob.getPendingBalance();
  auto alicePendingPrev = alice.getPendingBalance();
  auto aliceActualPrev = alice.getActualBalance();

  sendMoney(bobAddress, sent, FEE);

  node.updateObservers();

  waitActualBalanceUpdated(aliceActualPrev);
  waitPendingBalanceUpdated(bob, bobPrev);
  waitPendingBalanceUpdated(alicePendingPrev);

  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(sent, bob.getPendingBalance());

  ASSERT_EQ(2 * TEST_BLOCK_REWARD - sent - FEE, alice.getActualBalance() + alice.getPendingBalance());

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, transferTooBigTransaction) {
  const size_t testBlockGrantedFullRewardZone = parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_CURRENT;
  const size_t outputSize = 32 + 1;
  const size_t bigTxOutputCount = 2 * testBlockGrantedFullRewardZone / outputSize;

  CryptoNote::Currency cur = CryptoNote::CurrencyBuilder(logger).blockGrantedFullRewardZone(testBlockGrantedFullRewardZone).currency();
  TestBlockchainGenerator gen(cur);
  INodeTrivialRefreshStub n(gen);

  CryptoNote::WalletGreen wallet(dispatcher, cur, n, logger, TRANSACTION_SOFTLOCK_TIME);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  wallet.createAddress();

  gen.getBlockRewardForAddress(parseAddress(wallet.getAddress(0)));

  auto prev = wallet.getActualBalance();
  gen.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  n.updateObservers();
  waitActualBalanceUpdated(wallet, prev);

  CryptoNote::TransactionParameters params;
  for (size_t i = 0; i < bigTxOutputCount; ++i) {
    params.destinations.push_back({ RANDOM_ADDRESS, 1 });
  }

  params.fee = FEE;

  ASSERT_ANY_THROW(wallet.transfer(params));
}

TEST_F(WalletApi, transferCanSpendAllWalletOutputsIncludingDustOutputs) {
  const uint64_t TEST_DUST_THRESHOLD = UINT64_C(1) << 63;

  CryptoNote::Currency currency = CryptoNote::CurrencyBuilder(logger).defaultDustThreshold(TEST_DUST_THRESHOLD).currency();
  TestBlockchainGenerator generator(currency);
  INodeTrivialRefreshStub node(generator);

  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  std::string src = wallet.createAddress();
  std::string dst = wallet.createAddress();

  generator.getBlockRewardForAddress(parseAddress(src));
  generator.getBlockRewardForAddress(parseAddress(src));

  auto balance = wallet.getActualBalance();
  generator.generateEmptyBlocks(std::max(currency.minedMoneyUnlockWindow(), TRANSACTION_SOFTLOCK_TIME));
  node.updateObservers();
  waitActualBalanceUpdated(wallet, balance);

  uint64_t allWalletMoney = wallet.getActualBalance(src);
  ASSERT_LT(0, allWalletMoney);
  ASSERT_LT(currency.minimumFee(), allWalletMoney);
  ASSERT_EQ(0, wallet.getPendingBalance(src));
  ASSERT_EQ(0, wallet.getActualBalance(dst));
  ASSERT_EQ(0, wallet.getPendingBalance(dst));

  uint64_t sentMoney = allWalletMoney - currency.minimumFee();
  CryptoNote::TransactionParameters params;
  params.sourceAddresses = { src };
  params.destinations = { { dst, sentMoney } };
  params.changeDestination = src;
  params.fee = currency.minimumFee();

  // Make sure, that transaction will contain dust
  try {
    params.mixIn = 2;
    wallet.transfer(params);
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(make_error_code(CryptoNote::error::WRONG_AMOUNT), e.code());
    params.mixIn = 0;
  }

  auto txId = wallet.transfer(params);
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, txId);

  ASSERT_EQ(0, wallet.getActualBalance(src));
  ASSERT_EQ(0, wallet.getPendingBalance(src));
  ASSERT_EQ(0, wallet.getActualBalance(dst));
  ASSERT_EQ(sentMoney, wallet.getPendingBalance(dst));
}

TEST_F(WalletApi, balanceAfterTransfer) {
  generateAndUnlockMoney();

  auto prev = alice.getActualBalance();
  sendMoney(RANDOM_ADDRESS, SENT, FEE);

  waitActualBalanceUpdated(alice, prev);

  ASSERT_EQ(TEST_BLOCK_REWARD - SENT - FEE, alice.getActualBalance() + alice.getPendingBalance());
}

TEST_F(WalletApi, specificAddressesBalances) {
  generateAndUnlockMoney();

  auto secondAddress = alice.createAddress();
  generateBlockReward(secondAddress);
  node.updateObservers();
  waitPendingBalanceUpdated();

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance());
  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance(aliceAddress));
  ASSERT_EQ(0, alice.getActualBalance(secondAddress));

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getPendingBalance());
  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getPendingBalance(secondAddress));
  ASSERT_EQ(0, alice.getPendingBalance(aliceAddress));
}

TEST_F(WalletApi, transferFromSpecificAddress) {
  generateBlockReward();

  auto secondAddress = alice.createAddress();
  generateBlockReward(secondAddress);

  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();
  waitActualBalanceUpdated();

  auto prevActual = alice.getActualBalance();
  auto prevPending = alice.getPendingBalance();

  //send change to aliceAddress
  sendMoneyToRandomAddressFrom(secondAddress, aliceAddress);

  node.updateObservers();
  waitActualBalanceUpdated(prevActual);
  waitPendingBalanceUpdated(prevPending);

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance(aliceAddress));

  ASSERT_NE(TEST_BLOCK_REWARD, alice.getActualBalance(secondAddress));
  ASSERT_NE(0, alice.getPendingBalance(aliceAddress));
  ASSERT_EQ(2 * TEST_BLOCK_REWARD - SENT - FEE, alice.getActualBalance() + alice.getPendingBalance());
}

TEST_F(WalletApi, loadEmptyWallet) {
  alice.save();
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.load(BOB_WALLET_PATH, "pass");

  ASSERT_EQ(alice.getAddressCount(), bob.getAddressCount());
  ASSERT_EQ(alice.getActualBalance(), bob.getActualBalance());
  ASSERT_EQ(alice.getPendingBalance(), bob.getPendingBalance());
  ASSERT_EQ(alice.getTransactionCount(), bob.getTransactionCount());

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, walletGetsBaseTransaction) {
  // mine to alice's address to make it recieve block base transaction
  setMinerTo(alice);
  generateAndUnlockMoney();
  ASSERT_TRUE(alice.getTransaction(0).isBase);
}

TEST_F(WalletApi, walletGetsNonBaseTransaction) {
  generateAndUnlockMoney();
  ASSERT_FALSE(alice.getTransaction(0).isBase);
}

TEST_F(WalletApi, loadWalletWithBaseTransaction) {
  // mine to alice's address to make it recieve block base transaction
  setMinerTo(alice);
  generateAndUnlockMoney();

  alice.save();
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger);
  bob.load(BOB_WALLET_PATH, "pass");

  ASSERT_TRUE(bob.getTransaction(0).isBase);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, updateBaseTransactionAfterLoad) {
  // mine to alice's address to make it recieve block base transaction
  setMinerTo(alice);
  generateAndUnlockMoney();

  alice.save(WalletSaveLevel::SAVE_KEYS_AND_TRANSACTIONS);
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger);
  bob.load(BOB_WALLET_PATH, "pass");
  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  ASSERT_TRUE(bob.getTransaction(0).isBase);
  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, setBaseTransactionAfterInSynchronization) {
  // mine to alice's address to make it recieve block base transaction
  setMinerTo(alice);
  generateAndUnlockMoney();

  alice.save(WalletSaveLevel::SAVE_KEYS_ONLY);
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger);
  bob.load(BOB_WALLET_PATH, "pass");
  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  ASSERT_TRUE(bob.getTransaction(0).isBase);
  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, loadWalletWithoutAddresses) {
  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass");

  bob.save(WalletSaveLevel::SAVE_KEYS_ONLY);
  bob.shutdown();

  WalletGreen carol(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  carol.load(BOB_WALLET_PATH, "pass");

  ASSERT_EQ(0, carol.getAddressCount());
  carol.shutdown();
  wait(100);
}

void compareWalletsAddresses(const CryptoNote::WalletGreen& alice, const CryptoNote::WalletGreen& bob) {
  ASSERT_EQ(alice.getAddressCount(), bob.getAddressCount());
  for (size_t i = 0; i < alice.getAddressCount(); ++i) {
    ASSERT_EQ(alice.getAddress(i), bob.getAddress(i));
  }
}

void compareWalletsActualBalance(const CryptoNote::WalletGreen& alice, const CryptoNote::WalletGreen& bob) {
  ASSERT_EQ(alice.getActualBalance(), bob.getActualBalance());
  for (size_t i = 0; i < bob.getAddressCount(); ++i) {
    auto addr = bob.getAddress(i);
    ASSERT_EQ(alice.getActualBalance(addr), bob.getActualBalance(addr));
  }
}

void compareWalletsPendingBalance(const CryptoNote::WalletGreen& alice, const CryptoNote::WalletGreen& bob) {
  ASSERT_EQ(alice.getPendingBalance(), bob.getPendingBalance());
  for (size_t i = 0; i < bob.getAddressCount(); ++i) {
    auto addr = bob.getAddress(i);
    ASSERT_EQ(alice.getActualBalance(addr), bob.getActualBalance(addr));
  }
}

CryptoNote::WalletTransaction prepareTxForComparison(const CryptoNote::WalletTransaction& tx, bool strict) {
  auto result = tx;

  if (!strict) {
    // creationTime is assigned to time() for outgoing transaction, after wallet reset it is changed to the block time
    result.creationTime = 0;
    // When wallet saved without cache, transaction state is set to WalletTransactionState::CANCELLED and block height to WALLET_UNCONFIRMED_TRANSACTION_HEIGHT
    result.state = WalletTransactionState::CANCELLED;
    result.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
  }

  return result;
}

std::vector<CryptoNote::WalletTransactionWithTransfers> exportWalletTransactions(const CryptoNote::WalletGreen& wallet) {
  std::vector<CryptoNote::WalletTransactionWithTransfers> result;
  result.reserve(wallet.getTransactionCount());
  for (size_t i = 0; i < wallet.getTransactionCount(); ++i) {
    auto txHash = wallet.getTransaction(i).hash;
    result.emplace_back(wallet.getTransaction(txHash));
  }

  return result;
}

void compareWalletsTransactionTransfers(const std::vector<CryptoNote::WalletTransactionWithTransfers>& aliceTransactions,
  const CryptoNote::WalletGreen& bob, bool strict) {

  ASSERT_EQ(aliceTransactions.size(), bob.getTransactionCount());
  for (size_t i = 0; i < bob.getTransactionCount(); ++i) {
    ASSERT_EQ(prepareTxForComparison(aliceTransactions[i].transaction, strict), prepareTxForComparison(bob.getTransaction(i), strict));
    auto& aliceTransfers = aliceTransactions[i].transfers;
    ASSERT_EQ(aliceTransfers.size(), bob.getTransactionTransferCount(i));

    if (strict) {
      size_t trCount = bob.getTransactionTransferCount(i);
      for (size_t j = 0; j < trCount; ++j) {
        ASSERT_EQ(aliceTransfers[j], bob.getTransactionTransfer(i, j));
      }
    }
  }
}

void compareWalletsTransactionTransfers(const CryptoNote::WalletGreen& alice, const CryptoNote::WalletGreen& bob, bool strict) {
  compareWalletsTransactionTransfers(exportWalletTransactions(alice), bob, strict);
}

TEST_F(WalletApi, loadAll) {
  fillWalletWithDetailsCache();
  node.waitForAsyncContexts();
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));
  alice.save(WalletSaveLevel::SAVE_ALL);

  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger);
  bob.load(BOB_WALLET_PATH, "pass");

  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob, true);

  bob.shutdown();
  wait(100); //ObserverManager bug workaround
}

TEST_F(WalletApi, loadKeysOnly) {
  fillWalletWithDetailsCache();

  alice.save(WalletSaveLevel::SAVE_KEYS_ONLY);

  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger);
  bob.load(BOB_WALLET_PATH, "pass");

  compareWalletsAddresses(alice, bob);

  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(0, bob.getPendingBalance());
  ASSERT_EQ(0, bob.getTransactionCount());

  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));

  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob, false);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, loadKeysAndTransactions) {
  fillWalletWithDetailsCache();

  alice.save(WalletSaveLevel::SAVE_KEYS_AND_TRANSACTIONS);

  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger);
  bob.load(BOB_WALLET_PATH, "pass");

  compareWalletsAddresses(alice, bob);

  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(0, bob.getPendingBalance());

  compareWalletsTransactionTransfers(alice, bob, false);

  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));

  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob, true);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, loadWithWrongPassword) {
  alice.save(WalletSaveLevel::SAVE_KEYS_ONLY);
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);

  try {
    bob.load(BOB_WALLET_PATH, "pass2");
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(error::WRONG_PASSWORD, e.code().value());
  }
}

void WalletApi::testIWalletDataCompatibility(bool details, const std::string& cache, const std::vector<WalletLegacyTransaction>& txs,
    const std::vector<WalletLegacyTransfer>& trs, const std::vector<std::pair<TransactionInformation, int64_t>>& externalTxs) {
  CryptoNote::AccountBase account;
  account.generate();

  WalletUserTransactionsCache iWalletCache;
  WalletLegacySerializer walletSerializer(account, iWalletCache);

  for (const auto& tx: txs) {
    std::vector<WalletLegacyTransfer> txtrs;
    if (tx.firstTransferId != WALLET_LEGACY_INVALID_TRANSFER_ID && tx.transferCount != 0) {
      for (size_t i = tx.firstTransferId; i < (tx.firstTransferId  + tx.transferCount); ++i) {
        txtrs.push_back(trs[i]);
      }
    }
    auto txId = iWalletCache.addNewTransaction(tx.totalAmount, tx.fee, tx.extra, txtrs, tx.unlockTime);
    iWalletCache.updateTransactionSendingState(txId, std::error_code());
  }

  for (const auto& item: externalTxs) {
    iWalletCache.onTransactionUpdated(item.first, item.second);
  }

  std::ofstream stream(BOB_WALLET_PATH, std::ios_base::binary);
  walletSerializer.serialize(stream, "pass", details, std::string());
  stream.flush();
  stream.close();

  WalletGreen wallet(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  wallet.load(BOB_WALLET_PATH, "pass");

  EXPECT_EQ(1, wallet.getAddressCount());

  AccountPublicAddress addr;
  currency.parseAccountAddressString(wallet.getAddress(0), addr);
  EXPECT_EQ(account.getAccountKeys().address.spendPublicKey, addr.spendPublicKey);
  EXPECT_EQ(account.getAccountKeys().address.viewPublicKey, addr.viewPublicKey);
  EXPECT_EQ(0, wallet.getActualBalance());
  EXPECT_EQ(0, wallet.getPendingBalance());

  if (details) {
    auto outcomingTxCount = wallet.getTransactionCount() - externalTxs.size();
    ASSERT_EQ(txs.size(), outcomingTxCount);
    for (size_t i = 0; i < outcomingTxCount; ++i) {
      auto tx = wallet.getTransaction(i);
      EXPECT_EQ(WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, tx.blockHeight);
      EXPECT_EQ(txs[i].extra, tx.extra);
      EXPECT_EQ(txs[i].fee, tx.fee);
      EXPECT_EQ(WalletTransactionState::SUCCEEDED, tx.state);
      EXPECT_EQ(-txs[i].totalAmount, tx.totalAmount);
      EXPECT_EQ(txs[i].unlockTime, tx.unlockTime);

      size_t trsCount = wallet.getTransactionTransferCount(i);
      ASSERT_EQ(txs[i].transferCount, trsCount);
      for (size_t j = 0; j < trsCount; ++j) {
        ASSERT_NE(WALLET_LEGACY_INVALID_TRANSFER_ID, txs[i].firstTransferId);

        size_t index = txs[i].firstTransferId + j;
        EXPECT_EQ(trs[index].address, wallet.getTransactionTransfer(i, j).address);
        EXPECT_EQ(trs[index].amount, wallet.getTransactionTransfer(i, j).amount);
      }
    }

    ASSERT_EQ(txs.size() + externalTxs.size(), wallet.getTransactionCount());
    for (size_t i = outcomingTxCount; i < wallet.getTransactionCount(); ++i) {
      auto inTx = externalTxs[i - outcomingTxCount].first;
      auto txBalance = externalTxs[i - outcomingTxCount].second;
      auto tx = wallet.getTransaction(i);

      EXPECT_EQ(inTx.blockHeight, tx.blockHeight);
      EXPECT_EQ(0, tx.creationTime);
      std::string extraString(inTx.extra.begin(), inTx.extra.end());
      EXPECT_EQ(extraString, tx.extra);
      EXPECT_EQ(txBalance, tx.totalAmount);

      if (inTx.totalAmountIn) {
        EXPECT_EQ(inTx.totalAmountIn - inTx.totalAmountOut, tx.fee);
      } else {
        EXPECT_EQ(0, tx.fee);
      }

      EXPECT_EQ(inTx.transactionHash, tx.hash);
      EXPECT_EQ(WalletTransactionState::SUCCEEDED, tx.state);
      EXPECT_EQ(inTx.unlockTime, tx.unlockTime);
    }
  } else {
    EXPECT_EQ(0, wallet.getTransactionCount());
  }
}

TEST_F(WalletApi, IWalletDataCompatibilityEmptyDetailsNoCache) {
  testIWalletDataCompatibility(true);
}

TEST_F(WalletApi, IWalletDataCompatibilityEmptyNoDetailsNoCache) {
  testIWalletDataCompatibility(false);
}

TEST_F(WalletApi, IWalletDataCompatibilityEmptyNoDetailsCache) {
  std::string cache(1024, 'c');
  testIWalletDataCompatibility(false, cache);
}

TEST_F(WalletApi, IWalletDataCompatibilityEmptyDetailsCache) {
  std::string cache(1024, 'c');
  testIWalletDataCompatibility(true, cache);
}

TEST_F(WalletApi, IWalletDataCompatibilityDetails) {
  std::vector<WalletLegacyTransaction> txs;

  WalletLegacyTransaction tx1;
  tx1.firstTransferId = 0;
  tx1.transferCount = 2;
  tx1.unlockTime = 12;
  tx1.totalAmount = 1234567890;
  tx1.timestamp = (uint64_t) 8899007711;
  tx1.extra = "jsjeokvsnxcvkhdoifjaslkcvnvuergeonlsdnlaksmdclkasowehunkjn";
  tx1.fee = 1000;
  tx1.isCoinbase = false;
  txs.push_back(tx1);

  std::vector<WalletLegacyTransfer> trs;

  WalletLegacyTransfer tr1;
  tr1.address = RANDOM_ADDRESS;
  tr1.amount = SENT;
  trs.push_back(tr1);

  WalletLegacyTransfer tr2;
  tr2.amount = 102034;
  tr2.address = alice.getAddress(0);
  trs.push_back(tr2);

  std::vector<std::pair<TransactionInformation, int64_t>> incomingTxs;

  TransactionInformation iTx1;
  iTx1.timestamp = 929453;
  iTx1.totalAmountIn = 200353;
  iTx1.blockHeight = 2349;
  std::iota(iTx1.transactionHash.data, iTx1.transactionHash.data+32, 125);
  iTx1.extra = {1,2,3,4,5,6,7,8,9,1,2,3,4,5,6,7,8,9};
  std::iota(iTx1.publicKey.data, iTx1.publicKey.data+32, 15);
  iTx1.totalAmountOut = 948578;
  iTx1.unlockTime = 17;
  incomingTxs.push_back(std::make_pair(iTx1, 99874442));

  TransactionInformation iTx2;
  iTx2.timestamp = 10010;
  iTx2.totalAmountIn = 0;
  iTx2.blockHeight = 2350;
  std::iota(iTx2.transactionHash.data, iTx2.transactionHash.data+32, 15);
  iTx2.extra = {11,22,33,44,55,66,77,88,99,12,13,14,15,16};
  std::iota(iTx2.publicKey.data, iTx2.publicKey.data+32, 5);
  iTx2.totalAmountOut = 99874442;
  iTx2.unlockTime = 12;
  incomingTxs.push_back(std::make_pair(iTx2, 99874442));

  std::string cache(1024, 'c');
  testIWalletDataCompatibility(true, cache, txs, trs, incomingTxs);
}

TEST_F(WalletApi, getEventStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getEvent());
}

TEST_F(WalletApi, stopStart) {
  alice.stop();
  alice.start();

  ASSERT_NO_THROW(alice.getActualBalance());
}

TEST_F(WalletApi, uninitializedObject) {
  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);

  ASSERT_ANY_THROW(bob.changePassword("s", "p"));
  ASSERT_ANY_THROW(bob.save());
  ASSERT_ANY_THROW(bob.getAddressCount());
  ASSERT_ANY_THROW(bob.getAddress(0));
  ASSERT_ANY_THROW(bob.createAddress());
  ASSERT_ANY_THROW(bob.deleteAddress(RANDOM_ADDRESS));
  ASSERT_ANY_THROW(bob.getActualBalance());
  ASSERT_ANY_THROW(bob.getActualBalance(RANDOM_ADDRESS));
  ASSERT_ANY_THROW(bob.getPendingBalance());
  ASSERT_ANY_THROW(bob.getPendingBalance(RANDOM_ADDRESS));
  ASSERT_ANY_THROW(bob.getTransactionCount());
  ASSERT_ANY_THROW(bob.getTransaction(0));
  ASSERT_ANY_THROW(bob.getTransactionTransferCount(0));
  ASSERT_ANY_THROW(bob.getTransactionTransfer(0, 0));
  ASSERT_ANY_THROW(sendMoneyToRandomAddressFrom(aliceAddress, aliceAddress));
  ASSERT_ANY_THROW(bob.shutdown());
  wait(100);
}

const size_t TX_PUB_KEY_EXTRA_SIZE = 33;

TEST_F(WalletApi, checkSentTransaction) {
  generateAndUnlockMoney();

  auto prev = alice.getActualBalance();
  size_t txId = sendMoney(RANDOM_ADDRESS, SENT, FEE);

  waitActualBalanceUpdated(alice, prev);

  CryptoNote::WalletTransaction tx = alice.getTransaction(txId);
  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);
  ASSERT_EQ(0, tx.timestamp);
  ASSERT_EQ(CryptoNote::WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, tx.blockHeight);
  ASSERT_EQ(-static_cast<int64_t>(SENT + FEE), tx.totalAmount);
  ASSERT_EQ(FEE, tx.fee);
  ASSERT_EQ(0, tx.unlockTime);
  ASSERT_FALSE(tx.isBase);
  ASSERT_EQ(TX_PUB_KEY_EXTRA_SIZE, tx.extra.size()); //Transaction public key only
}

std::string removeTxPublicKey(const std::string& txExtra) {
  if (txExtra.size() <= TX_PUB_KEY_EXTRA_SIZE) {
    return std::string();
  }

  return txExtra.substr(TX_PUB_KEY_EXTRA_SIZE);
}

std::string createExtraNonce(const std::string& nonce) {
  CryptoNote::TransactionExtra txExtra;
  CryptoNote::TransactionExtraNonce extraNonce;
  extraNonce.nonce = asBinaryArray(nonce);
  txExtra.set(extraNonce);
  auto vec = txExtra.serialize();
  return std::string(vec.begin(), vec.end());
}

TEST_F(WalletApi, checkSentTransactionWithExtra) {
  const std::string extra = createExtraNonce("\x01\x23\x45\x67\x89\xab\xcd\xef");

  generateAndUnlockMoney();

  auto prev = alice.getActualBalance();
  size_t txId = sendMoney(RANDOM_ADDRESS, SENT, FEE, 0, extra);

  waitActualBalanceUpdated(alice, prev);

  CryptoNote::WalletTransaction tx = alice.getTransaction(txId);
  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);
  ASSERT_EQ(0, tx.timestamp);
  ASSERT_EQ(CryptoNote::WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT, tx.blockHeight);
  ASSERT_EQ(-static_cast<int64_t>(SENT + FEE), tx.totalAmount);
  ASSERT_EQ(FEE, tx.fee);
  ASSERT_EQ(0, tx.unlockTime);
  ASSERT_FALSE(tx.isBase);
  ASSERT_EQ(extra, removeTxPublicKey(tx.extra));
}

TEST_F(WalletApi, checkFailedTransaction) {
  generateAndUnlockMoney();

  node.setNextTransactionError();
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE));

  auto tx = alice.getTransaction(alice.getTransactionCount() - 1);
  ASSERT_EQ(CryptoNote::WalletTransactionState::FAILED, tx.state);
}

TEST_F(WalletApi, transactionSendsAfterFailedTransaction) {
  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE);
  unlockMoney();

  node.setNextTransactionError();
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE));
  ASSERT_NO_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE));
}

TEST_F(WalletApi, checkIncomingTransaction) {
  const std::string extra = createExtraNonce("\x01\x23\x45\x67\x89\xab\xcd\xef");

  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  std::string bobAddress = bob.createAddress();

  sendMoney(bobAddress, SENT, FEE, 0, extra, 11);
  node.updateObservers();
  waitPendingBalanceUpdated(bob, 0);

  auto tx = bob.getTransaction(bob.getTransactionCount() - 1);

  bob.shutdown();
  wait(100); //observer manager bug

  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);
  ASSERT_NE(0, tx.timestamp);
  ASSERT_EQ(generator.getBlockchain().size() - 1, tx.blockHeight);
  ASSERT_EQ(SENT, tx.totalAmount);
  ASSERT_EQ(FEE, tx.fee);
  ASSERT_EQ(11, tx.unlockTime);
  ASSERT_EQ(extra, removeTxPublicKey(tx.extra));
}

TEST_F(WalletApi, notEnoughMoney) {
  generateAndUnlockMoney();
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, TEST_BLOCK_REWARD, FEE));
}

TEST_F(WalletApi, changePassword) {
  generateAndUnlockMoney();

  ASSERT_NO_THROW(alice.changePassword("pass", "pass2"));

  alice.save(WalletSaveLevel::SAVE_KEYS_ONLY);
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_NO_THROW(bob.load(BOB_WALLET_PATH, "pass2"));

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, changePasswordWrong) {
  ASSERT_ANY_THROW(alice.changePassword("pass2", "pass3"));
}

TEST_F(WalletApi, shutdownInit) {
  generateBlockReward();
  node.updateObservers();
  waitPendingBalanceUpdated(0);

  alice.shutdown();
  alice.initialize(BOB_WALLET_PATH, "p");

  EXPECT_EQ(0, alice.getAddressCount());
  EXPECT_EQ(0, alice.getActualBalance());
  EXPECT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, detachBlockchain) {
  generateAndUnlockMoney();

  auto alicePrev = alice.getActualBalance();

  node.startAlternativeChain(1);
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();
  waitActualBalanceUpdated(alicePrev);

  ASSERT_EQ(0, alice.getActualBalance());
  ASSERT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, deleteAddresses) {
  fillWalletWithDetailsCache();
  alice.createAddress();

  for (size_t i = 0; i < 11; ++i) {
    alice.deleteAddress(alice.getAddress(0));
  }

  EXPECT_EQ(0, alice.getActualBalance());
  EXPECT_EQ(0, alice.getPendingBalance());
}

TEST_F(WalletApi, incomingTxTransferWithChange) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  bob.createAddress();
  bob.createAddress();

  sendMoney(bob.getAddress(0), SENT, FEE);
  sendMoney(bob.getAddress(1), 2 * SENT, FEE);
  node.updateObservers();
  waitForTransactionCount(bob, 2);

  EXPECT_EQ(3, bob.getTransactionTransferCount(0)); //sent from alice + received on bob + alice change
  ASSERT_EQ(3, bob.getTransactionTransferCount(1));

  auto tr1 = bob.getTransactionTransfer(0, 0);
  EXPECT_EQ(tr1.address, bob.getAddress(0));
  EXPECT_EQ(tr1.amount, SENT);

  auto tr2 = bob.getTransactionTransfer(1, 0);
  EXPECT_EQ(tr2.address, bob.getAddress(1));
  EXPECT_EQ(tr2.amount, 2 * SENT);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, incomingTxTransferWithoutChange) {
  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE);
  unlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  bob.createAddress();

  sendMoney(bob.getAddress(0), SENT, FEE);

  node.updateObservers();
  waitForTransactionCount(bob, 1);

  ASSERT_EQ(2, bob.getTransactionTransferCount(0));
  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, walletSendsTransactionUpdatedEventAfterAddingTransfer) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  bob.createAddress();
  bob.createAddress();
  bob.createAddress();

  CryptoNote::TransactionParameters params;
  params.destinations.emplace_back(CryptoNote::WalletOrder{ bob.getAddress(0), SENT });
  params.destinations.emplace_back(CryptoNote::WalletOrder{ bob.getAddress(1), SENT });
  params.destinations.emplace_back(CryptoNote::WalletOrder{ bob.getAddress(2), SENT });
  params.fee = FEE;
  alice.transfer(params);

  node.updateObservers();
  ASSERT_TRUE(waitForWalletEvent(bob, CryptoNote::WalletEventType::TRANSACTION_CREATED, std::chrono::seconds(5)));

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, walletCreatesTransferForEachTransactionFunding) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");
  bob.createAddress();
  bob.createAddress();

  CryptoNote::TransactionParameters params;
  params.destinations.emplace_back(CryptoNote::WalletOrder{ bob.getAddress(0), SENT });
  params.destinations.emplace_back(CryptoNote::WalletOrder{ bob.getAddress(1), 2 * SENT });

  params.fee = FEE;
  alice.transfer(params);

  node.updateObservers();
  ASSERT_TRUE(waitForWalletEvent(bob, CryptoNote::WalletEventType::TRANSACTION_CREATED, std::chrono::seconds(5)));

  //2 incoming transfers to bob's addresses(0, 1) and one outgoing for alice(0) + change to alice(0)
  ASSERT_EQ(4, bob.getTransactionTransferCount(0));
  auto tr1 = bob.getTransactionTransfer(0, 0);
  auto tr2 = bob.getTransactionTransfer(0, 1);
  ASSERT_TRUE(tr1.address == bob.getAddress(0) || tr1.address == bob.getAddress(1));
  ASSERT_TRUE(tr2.address == bob.getAddress(0) || tr2.address == bob.getAddress(1));
  ASSERT_NE(tr1.address, tr2.address);

  bob.shutdown();
  wait(100);
}

size_t getTransactionUsualTransferCount(WalletGreen& wallet, size_t transactionIndex) {
  size_t transfersCount = wallet.getTransactionTransferCount(transactionIndex);
  size_t usualTransfersCount = 0;
  for (size_t i = 0; i < transfersCount; ++i) {
    if (wallet.getTransactionTransfer(transactionIndex, i).type == WalletTransferType::USUAL) {
      ++usualTransfersCount;
    }
  }

  return usualTransfersCount;
}

TEST_F(WalletApi, hybridTxTransfer) {
  generateAndUnlockMoney();

  alice.createAddress();
  alice.createAddress();

  CryptoNote::WalletOrder tr1 { alice.getAddress(1), SENT };
  CryptoNote::WalletOrder tr2 { alice.getAddress(2), 2 * SENT };

  CryptoNote::TransactionParameters params;
  params.destinations = {tr1, tr2};
  params.fee = FEE;
  params.changeDestination = alice.getAddress(0);
  alice.transfer(params);
  node.updateObservers();
  dispatcher.yield();

  //2 incoming transfers to alice's addresses(1, 2) and one outgoing for alice(0)
  ASSERT_EQ(3, getTransactionUsualTransferCount(alice, 1));

  WalletTransactionWithTransfers transfersWithTx;
  ASSERT_NO_THROW({
    transfersWithTx = alice.getTransaction(alice.getTransaction(1).hash);
  });
  //2 incoming transfers to alice's addresses(1, 2) and one outgoing for alice(0) + change to alice(0)
  ASSERT_EQ(4, transfersWithTx.transfers.size());

  auto iter = std::find_if(transfersWithTx.transfers.begin(), transfersWithTx.transfers.end(), [&tr1](const WalletTransfer& transfer) {
    return tr1.address == transfer.address && tr1.amount == transfer.amount && WalletTransferType::USUAL == transfer.type;
  });
  EXPECT_NE(transfersWithTx.transfers.end(), iter);
  
  iter = std::find_if(transfersWithTx.transfers.begin(), transfersWithTx.transfers.end(), [&tr2](const WalletTransfer& transfer) {
    return tr2.address == transfer.address && tr2.amount == transfer.amount && WalletTransferType::USUAL == transfer.type;
  });
  EXPECT_NE(transfersWithTx.transfers.end(), iter);

  iter = std::find_if(transfersWithTx.transfers.begin(), transfersWithTx.transfers.end(), [this](const WalletTransfer& transfer) {
    return alice.getAddress(0) == transfer.address && WalletTransferType::CHANGE == transfer.type;
  });
  EXPECT_NE(transfersWithTx.transfers.end(), iter);
  WalletTransfer changeTransfer = *iter;

  iter = std::find_if(transfersWithTx.transfers.begin(), transfersWithTx.transfers.end(), [this, &tr1, &tr2, &changeTransfer](const WalletTransfer& transfer) {
    return alice.getAddress(0) == transfer.address && -static_cast<int64_t>(tr1.amount + tr2.amount + FEE + changeTransfer.amount) == transfer.amount && WalletTransferType::USUAL == transfer.type;
  });
  EXPECT_NE(transfersWithTx.transfers.end(), iter);
}

TEST_F(WalletApi, doubleSpendJustSentOut) {
  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE);
  unlockMoney();

  sendMoney(RANDOM_ADDRESS, SENT, FEE);
  ASSERT_ANY_THROW(sendMoney(RANDOM_ADDRESS, SENT, FEE));
}

TEST_F(WalletApi, syncAfterLoad) {
  alice.save();
  alice.shutdown();

  generateBlockReward();
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());

  alice.load(ALICE_WALLET_PATH, "pass");

  wait(300);

  ASSERT_EQ(TEST_BLOCK_REWARD, alice.getActualBalance());
}

class INodeNoRelay : public INodeTrivialRefreshStub {
public:
  INodeNoRelay(TestBlockchainGenerator& generator) : INodeTrivialRefreshStub(generator) {}

  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) override {
    m_asyncCounter.addAsyncContext();
    std::thread task(&INodeNoRelay::doNoRelayTransaction, this, transaction, callback);
    task.detach();
  }

  void doNoRelayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback)
  {
    callback(std::error_code());
    m_asyncCounter.delAsyncContext();
  }
};

TEST_F(WalletApi, DISABLED_loadTest) {
  using namespace std::chrono;

  INodeNoRelay noRelayNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, noRelayNode, logger, TRANSACTION_SOFTLOCK_TIME);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  const size_t ADDRESSES_COUNT = 1000;

  std::cout << "creating addresses" << std::endl;
  steady_clock::time_point start = steady_clock::now();

  for (size_t i = 0; i < ADDRESSES_COUNT; ++i) {
    wallet.createAddress();
  }

  steady_clock::time_point end = steady_clock::now();
  std::cout << "addresses creation finished in: " << duration_cast<milliseconds>(end - start).count() << " ms" << std::endl;
  std::cout << "filling up the wallets" << std::endl;

  for (size_t i = 0; i < ADDRESSES_COUNT; ++i) {
    if (!(i % 100)) {
      std::cout << "filling " << i << "th wallet" << std::endl;
    }
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
    generator.generateTransactionsInOneBlock(parseAddress(wallet.getAddress(i)), 10);
  }

  std::cout << "wallets filled. input any character" << std::endl;
  char x;
  std::cin >> x;

  std::cout << "sync start" << std::endl;
  steady_clock::time_point syncStart = steady_clock::now();
  noRelayNode.updateObservers();
  waitForTransactionCount(wallet, ADDRESSES_COUNT * 50);
  steady_clock::time_point syncEnd = steady_clock::now();
  std::cout << "sync took: " << duration_cast<milliseconds>(syncEnd - syncStart).count() << " ms" << std::endl;

  unlockMoney(wallet, noRelayNode);

  const size_t TRANSACTIONS_COUNT = 1000;
  std::cout << "wallets filled. input any character" << std::endl;
  std::cin >> x;

  steady_clock::time_point transferStart = steady_clock::now();
  for (size_t i = 0; i < TRANSACTIONS_COUNT; ++i) {
    sendMoney(wallet, RANDOM_ADDRESS, SENT, FEE);
  }
  steady_clock::time_point transferEnd = steady_clock::now();
  std::cout << "transfers took: " << duration_cast<milliseconds>(transferEnd - transferStart).count() << " ms" << std::endl;

  wallet.shutdown();
  wait(100);
}

TEST_F(WalletApi, transferSmallFeeTransactionThrows) {
  generateAndUnlockMoney();

  ASSERT_ANY_THROW(sendMoneyToRandomAddressFrom(alice.getAddress(0), SENT, currency.minimumFee() - 1, alice.getAddress(0)));
}

TEST_F(WalletApi, initializeWithKeysSucceded) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);

  CryptoNote::KeyPair viewKeys;
  Crypto::generate_keys(viewKeys.publicKey, viewKeys.secretKey);
  ASSERT_NO_THROW(wallet.initializeWithViewKey(BOB_WALLET_PATH, "pass", viewKeys.secretKey));

  wallet.shutdown();
}

TEST_F(WalletApi, initializeWithKeysThrowsIfAlreadyInitialized) {
  CryptoNote::KeyPair viewKeys;
  Crypto::generate_keys(viewKeys.publicKey, viewKeys.secretKey);

  ASSERT_ANY_THROW(alice.initializeWithViewKey(ALICE_WALLET_PATH, "pass", viewKeys.secretKey));
}

TEST_F(WalletApi, initializeWithKeysThrowsIfStopped) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  wallet.stop();

  CryptoNote::KeyPair viewKeys;
  Crypto::generate_keys(viewKeys.publicKey, viewKeys.secretKey);
  ASSERT_ANY_THROW(wallet.initializeWithViewKey(ALICE_WALLET_PATH, "pass", viewKeys.secretKey));
}

TEST_F(WalletApi, getViewKeyReturnsProperKey) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);

  CryptoNote::KeyPair viewKeys;
  Crypto::generate_keys(viewKeys.publicKey, viewKeys.secretKey);
  wallet.initializeWithViewKey(BOB_WALLET_PATH, "pass", viewKeys.secretKey);

  CryptoNote::KeyPair retrievedKeys = wallet.getViewKey();
  ASSERT_EQ(viewKeys.publicKey, retrievedKeys.publicKey);
  ASSERT_EQ(viewKeys.secretKey, retrievedKeys.secretKey);

  wallet.shutdown();
}

TEST_F(WalletApi, getViewKeyThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  ASSERT_ANY_THROW(wallet.getViewKey());
}

TEST_F(WalletApi, getViewKeyThrowsIfStopped) {
  alice.stop();

  ASSERT_ANY_THROW(alice.getViewKey());
}

TEST_F(WalletApi, getAddressSpendKeyReturnsProperKey) {
  CryptoNote::KeyPair spendKeys;
  Crypto::generate_keys(spendKeys.publicKey, spendKeys.secretKey);

  alice.createAddress(spendKeys.secretKey);

  CryptoNote::KeyPair retrievedKeys = alice.getAddressSpendKey(1);
  ASSERT_EQ(spendKeys.publicKey, retrievedKeys.publicKey);
  ASSERT_EQ(spendKeys.secretKey, retrievedKeys.secretKey);
}

TEST_F(WalletApi, getAddressSpendKeyThrowsForWrongAddressIndex) {
  ASSERT_ANY_THROW(alice.getAddressSpendKey(1));
}

TEST_F(WalletApi, getAddressSpendKeyThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  ASSERT_ANY_THROW(wallet.getAddressSpendKey(0));
}

TEST_F(WalletApi, getAddressSpendKeyThrowsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getAddressSpendKey(0));
}

Crypto::PublicKey generatePublicKey() {
  CryptoNote::KeyPair spendKeys;
  Crypto::generate_keys(spendKeys.publicKey, spendKeys.secretKey);

  return spendKeys.publicKey;
}

TEST_F(WalletApi, createTrackingKeyAddressSucceeded) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  Crypto::PublicKey publicKey = generatePublicKey();

  ASSERT_NO_THROW(wallet.createAddress(publicKey));
  ASSERT_EQ(1, wallet.getAddressCount());
  wallet.shutdown();
}

TEST_F(WalletApi, createTrackingKeyThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);

  Crypto::PublicKey publicKey = generatePublicKey();
  ASSERT_ANY_THROW(wallet.createAddress(publicKey));
}

TEST_F(WalletApi, createTrackingKeyThrowsIfStopped) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  wallet.stop();

  Crypto::PublicKey publicKey = generatePublicKey();
  ASSERT_ANY_THROW(wallet.createAddress(publicKey));
  wallet.shutdown();
}

TEST_F(WalletApi, createTrackingKeyThrowsIfKeyExists) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  Crypto::PublicKey publicKey = generatePublicKey();
  wallet.createAddress(publicKey);
  ASSERT_ANY_THROW(wallet.createAddress(publicKey));
  wallet.shutdown();
}

TEST_F(WalletApi, createTrackingKeyThrowsIfWalletHasNotTrackingKeys) {
  Crypto::PublicKey publicKey = generatePublicKey();
  ASSERT_ANY_THROW(alice.createAddress(publicKey));
}

TEST_F(WalletApi, getAddressSpendKeyForTrackingKeyReturnsNullSecretKey) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  Crypto::PublicKey publicKey = generatePublicKey();
  wallet.createAddress(publicKey);

  KeyPair spendKeys = wallet.getAddressSpendKey(0);
  ASSERT_EQ(NULL_SECRET_KEY, spendKeys.secretKey);

  wallet.shutdown();
}

TEST_F(WalletApi, trackingAddressReceivesMoney) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger);
  bob.initialize(BOB_WALLET_PATH, "pass2");

  Crypto::PublicKey publicKey = generatePublicKey();
  bob.createAddress(publicKey);

  sendMoney(bob.getAddress(0), SENT, FEE);
  node.updateObservers();

  auto expectedTransactionHeight = node.getLastKnownBlockHeight();
  waitPendingBalanceUpdated(bob, 0);

  ASSERT_EQ(SENT, bob.getPendingBalance());
  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(1, bob.getTransactionCount());

  CryptoNote::WalletTransaction transaction = bob.getTransaction(0);
  ASSERT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, transaction.state);
  ASSERT_EQ(expectedTransactionHeight, transaction.blockHeight);
  ASSERT_EQ(SENT, transaction.totalAmount);
  ASSERT_EQ(FEE, transaction.fee);
  ASSERT_EQ(0, transaction.unlockTime);

  bob.shutdown();
}

TEST_F(WalletApi, trackingAddressUnlocksMoney) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger);
  bob.initialize(BOB_WALLET_PATH, "pass2");

  Crypto::PublicKey publicKey = generatePublicKey();
  bob.createAddress(publicKey);

  sendMoney(bob.getAddress(0), SENT, FEE);
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();
  waitActualBalanceUpdated(bob, 0);

  ASSERT_EQ(0, bob.getPendingBalance());
  ASSERT_EQ(SENT, bob.getActualBalance());
}

TEST_F(WalletApi, transferFromTrackingKeyThrows) {
  generateAndUnlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger);
  bob.initialize(BOB_WALLET_PATH, "pass2");

  Crypto::PublicKey publicKey = generatePublicKey();
  bob.createAddress(publicKey);

  sendMoney(bob.getAddress(0), SENT, FEE);
  generator.generateEmptyBlocks(currency.minedMoneyUnlockWindow());
  node.updateObservers();
  waitActualBalanceUpdated(bob, 0);

  ASSERT_ANY_THROW(sendMoney(bob, RANDOM_ADDRESS, SENT, FEE));
  bob.shutdown();
}

TEST_F(WalletApi, createAddressListSuccessfullyCreatesAddresses) {
  ASSERT_EQ(1, alice.getAddressCount());
  for (size_t i = 0; i < 10; ++i) {
    alice.createAddress();
  }

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initializeWithViewKey(BOB_WALLET_PATH, "pass", alice.getViewKey().secretKey);

  std::vector<Crypto::SecretKey> spendSecretKeys(alice.getAddressCount());
  for (size_t i = 0; i < spendSecretKeys.size(); ++i) {
    spendSecretKeys[i] = alice.getAddressSpendKey(i).secretKey;
  }

  ASSERT_NO_THROW(bob.createAddressList(spendSecretKeys));
  compareWalletsAddresses(alice, bob);

  bob.shutdown();

  wait(100);
}

TEST_F(WalletApi, walletGetsSyncCompletedEvent) {
  generator.generateEmptyBlocks(1);
  node.updateObservers();

  ASSERT_TRUE(waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5)));
}

TEST_F(WalletApi, walletGetsSyncProgressUpdatedEvent) {
  generator.generateEmptyBlocks(1);
  node.updateObservers();

  ASSERT_TRUE(waitForWalletEvent(alice, CryptoNote::SYNC_PROGRESS_UPDATED, std::chrono::seconds(5)));
}

struct CatchTransactionNodeStub : public INodeTrivialRefreshStub {
  CatchTransactionNodeStub(TestBlockchainGenerator& generator): INodeTrivialRefreshStub(generator), caught(false) {}

  virtual void relayTransaction(const CryptoNote::Transaction& incomingTransaction, const Callback& callback) override {
    transaction = incomingTransaction;
    caught = true;
    INodeTrivialRefreshStub::relayTransaction(incomingTransaction, callback);
  }

  bool caught;
  CryptoNote::Transaction transaction;
};

TEST_F(WalletApi, createFusionTransactionCreatesValidFusionTransactionWithoutMixin) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
  ASSERT_TRUE(catchNode.caught);
  ASSERT_TRUE(currency.isFusionTransaction(catchNode.transaction));

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionCreatesValidFusionTransactionWithMixin) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, wallet.createFusionTransaction(FUSION_THRESHOLD, 2));
  ASSERT_TRUE(catchNode.caught);
  ASSERT_TRUE(currency.isFusionTransaction(catchNode.transaction));

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionDoesnotAffectTotalBalance) {
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);

  auto totalBalance = alice.getActualBalance() + alice.getPendingBalance();
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, alice.createFusionTransaction(FUSION_THRESHOLD, 2));
  ASSERT_EQ(totalBalance, alice.getActualBalance() + alice.getPendingBalance());
}

TEST_F(WalletApi, createFusionTransactionFailsIfMixinToobig) {
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);
  ASSERT_ANY_THROW(alice.createFusionTransaction(FUSION_THRESHOLD, std::numeric_limits<uint16_t>::max()));
}

TEST_F(WalletApi, createFusionTransactionFailsIfNoTransfers) {
  ASSERT_EQ(WALLET_INVALID_TRANSACTION_ID, alice.createFusionTransaction(FUSION_THRESHOLD, 0));
}

TEST_F(WalletApi, createFusionTransactionThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  ASSERT_ANY_THROW(wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
}

TEST_F(WalletApi, createFusionTransactionThrowsIfStopped) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  wallet.stop();
  ASSERT_ANY_THROW(wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfThresholdTooSmall) {
  ASSERT_ANY_THROW(alice.createFusionTransaction(currency.defaultDustThreshold() - 1, 0));
}

TEST_F(WalletApi, createFusionTransactionThrowsIfNoAddresses) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  ASSERT_ANY_THROW(wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfTransactionSendError) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD);

  catchNode.setNextTransactionError();
  ASSERT_ANY_THROW(wallet.createFusionTransaction(FUSION_THRESHOLD, 0));
  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionSpendsAllWalletsOutputsIfSourceAddressIsEmpty) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  std::string address1 = wallet.createAddress();
  std::string address2 = wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 1);

  uint64_t balance0 = wallet.getActualBalance(address0);
  uint64_t balance1 = wallet.getActualBalance(address1);
  uint64_t balance2 = wallet.getActualBalance(address2);
  ASSERT_GT(balance0, 0);
  ASSERT_GT(balance1, 0);
  ASSERT_EQ(balance2, 0);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, wallet.createFusionTransaction(FUSION_THRESHOLD, 0, { }, address2));
  uint64_t balance0Updated = wallet.getActualBalance(address0);
  uint64_t balance1Updated = wallet.getActualBalance(address1);
  ASSERT_LT(balance0Updated, balance0);
  ASSERT_LT(balance1Updated, balance1);

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionTransfersAllMoneyToTheOnlySourceAddressIfDestinationIsEmpty) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  std::string address1 = wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 1);

  uint64_t balance0 = wallet.getActualBalance(address0);
  uint64_t balance1 = wallet.getActualBalance(address1);
  ASSERT_GT(balance0, 0);
  ASSERT_GT(balance1, 0);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, wallet.createFusionTransaction(FUSION_THRESHOLD, 0, { address1 }));
  ASSERT_EQ(balance0, wallet.getActualBalance(address0));
  ASSERT_EQ(balance1, wallet.getActualBalance(address1) + wallet.getPendingBalance(address1));
  ASSERT_GT(wallet.getPendingBalance(address1), 0);

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfSourceAddresIsNotAValidAddress) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);

  try {
    wallet.createFusionTransaction(FUSION_THRESHOLD, 0, { "BAD ADDRESS" }, address0);
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), e.code());
  }

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfSourceAddresDoesNotBelongToTheContainer) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);

  CryptoNote::AccountBase randomAccount;
  randomAccount.generate();
  std::string randomAddress = currency.accountAddressAsString(randomAccount);

  try {
    wallet.createFusionTransaction(FUSION_THRESHOLD, 0, { address0, randomAddress }, address0);
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), e.code());
  }

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfDestinationAddresIsNotAValidAddress) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);

  try {
    wallet.createFusionTransaction(FUSION_THRESHOLD, 0, { address0 }, "BAD ADDRESS");
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), e.code());
  }

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfContainerHasAFewWalletsAndSourceAddressesAndDestinationAddressIsEmpty) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  std::string address1 = wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 1);

  try {
    wallet.createFusionTransaction(FUSION_THRESHOLD, 0);
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(make_error_code(CryptoNote::error::DESTINATION_ADDRESS_REQUIRED), e.code());
  }

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionThrowsIfItHasAFewSourceAddressesButDestinationAddressIsEmpty) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  std::string address1 = wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 1);

  try {
    wallet.createFusionTransaction(FUSION_THRESHOLD, 0, { address0, address1 });
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(make_error_code(CryptoNote::error::DESTINATION_ADDRESS_REQUIRED), e.code());
  }

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionSpendsOnlySourceAddressOutputs) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  std::string address1 = wallet.createAddress();
  std::string address2 = wallet.createAddress();
  std::string address3 = wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 1);
  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 2);

  uint64_t balance0 = wallet.getActualBalance(address0);
  uint64_t balance1 = wallet.getActualBalance(address1);
  uint64_t balance2 = wallet.getActualBalance(address2);
  uint64_t balance3 = wallet.getActualBalance(address3);
  ASSERT_GT(balance0, 0);
  ASSERT_GT(balance1, 0);
  ASSERT_GT(balance2, 0);
  ASSERT_EQ(balance3, 0);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, wallet.createFusionTransaction(FUSION_THRESHOLD, 0, { address1, address2 }, address3));
  uint64_t balance1Updated = wallet.getActualBalance(address1);
  uint64_t balance2Updated = wallet.getActualBalance(address2);
  ASSERT_EQ(wallet.getActualBalance(address0), balance0);
  ASSERT_LT(balance1Updated, balance1);
  ASSERT_LT(balance2Updated, balance2);
  ASSERT_EQ(wallet.getPendingBalance(address3), balance1 - balance1Updated + balance2 - balance2Updated);

  wallet.shutdown();
}

TEST_F(WalletApi, createFusionTransactionTransfersAllMoneyToDestinationAddress) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");

  std::string address0 = wallet.createAddress();
  std::string address1 = wallet.createAddress();

  generateFusionOutputsAndUnlock(wallet, catchNode, currency, FUSION_THRESHOLD, 0);

  uint64_t balance0 = wallet.getActualBalance(address0);
  uint64_t balance1 = wallet.getActualBalance(address1);
  ASSERT_GT(balance0, 0);
  ASSERT_EQ(balance1, 0);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, wallet.createFusionTransaction(FUSION_THRESHOLD, 0, { }, address1));
  uint64_t balance0Updated = wallet.getActualBalance(address0);
  ASSERT_LT(balance0Updated, balance0);
  ASSERT_EQ(wallet.getPendingBalance(address1), balance0 - balance0Updated);

  wallet.shutdown();
}

TEST_F(WalletApi, fusionManagerEstimateThrowsIfNotInitialized) {
  const uint64_t THRESHOLD = 100;
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  ASSERT_ANY_THROW(wallet.estimate(THRESHOLD));
}

TEST_F(WalletApi, fusionManagerEstimateThrowsIfStopped) {
  const uint64_t THRESHOLD = 100;
  alice.stop();
  ASSERT_ANY_THROW(alice.estimate(THRESHOLD));
}

TEST_F(WalletApi, fusionManagerEstimateEmpty) {
  const uint64_t THRESHOLD = 100;
  IFusionManager::EstimateResult emptyResult = {0, 0};
  ASSERT_EQ(emptyResult, alice.estimate(THRESHOLD));
}

TEST_F(WalletApi, fusionManagerEstimateLocked) {
  auto pending = alice.getPendingBalance();
  generateBlockReward();
  node.updateObservers();
  waitPendingBalanceUpdated(alice, pending);

  IFusionManager::EstimateResult expectedResult =  {0, 0};
  ASSERT_EQ(expectedResult, alice.estimate(0));
}

TEST_F(WalletApi, fusionManagerEstimateNullThreshold) {
  generateAndUnlockMoney();

  ASSERT_EQ(1, alice.getTransactionCount());
  CryptoNote::Transaction tx = boost::value_initialized<CryptoNote::Transaction>();
  ASSERT_TRUE(generator.getTransactionByHash(alice.getTransaction(0).hash, tx, false));
  ASSERT_FALSE(tx.outputs.empty());

  IFusionManager::EstimateResult expectedResult = {0, tx.outputs.size()};
  ASSERT_EQ(expectedResult, alice.estimate(0));
}

TEST_F(WalletApi, fusionManagerEstimateCountsOnlySourceAddressOutputs) {
  ASSERT_EQ(1, alice.getAddressCount());

  std::string address0 = alice.getAddress(0);
  std::string address1 = alice.createAddress();

  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD, 0);
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD, 1);

  auto estimateAll = alice.estimate(FUSION_THRESHOLD);
  auto estimate0 = alice.estimate(FUSION_THRESHOLD, { address0 });
  auto estimate1 = alice.estimate(FUSION_THRESHOLD, { address1 });

  ASSERT_EQ(estimateAll.totalOutputCount, estimate0.totalOutputCount + estimate1.totalOutputCount);
  ASSERT_GT(estimateAll.fusionReadyCount, estimate0.fusionReadyCount);
  ASSERT_GT(estimateAll.fusionReadyCount, estimate1.fusionReadyCount);
  ASSERT_GT(estimate0.fusionReadyCount, 0);
  ASSERT_GT(estimate1.fusionReadyCount, 0);
  ASSERT_GE(estimate0.totalOutputCount, estimate0.fusionReadyCount);
  ASSERT_GE(estimate1.totalOutputCount, estimate1.fusionReadyCount);
}

TEST_F(WalletApi, fusionManagerEstimateThrowsIfSourceAddresIsNotAValidAddress) {
  ASSERT_EQ(1, alice.getAddressCount());

  std::string address0 = alice.getAddress(0);
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD, 0);

  try {
    alice.estimate(FUSION_THRESHOLD, { "BAD ADDRESS" });
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), e.code());
  }
}

TEST_F(WalletApi, fusionManagerEstimateThrowsIfAddressDoesNotBelongToTheContainer) {
  ASSERT_EQ(1, alice.getAddressCount());

  std::string address0 = alice.getAddress(0);
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD, 0);

  CryptoNote::AccountBase randomAccount;
  randomAccount.generate();
  std::string randomAddress = currency.accountAddressAsString(randomAccount);

  try {
    alice.estimate(FUSION_THRESHOLD, { address0, randomAddress });
    ASSERT_FALSE(true);
  } catch (const std::system_error& e) {
    ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), e.code());
  }
}

TEST_F(WalletApi, DISABLED_fusionManagerEstimate) {
  generateAndUnlockMoney();

  ASSERT_EQ(1, alice.getTransactionCount());
  CryptoNote::Transaction tx = boost::value_initialized<CryptoNote::Transaction>();
  ASSERT_TRUE(generator.getTransactionByHash(alice.getTransaction(0).hash, tx, false));
  ASSERT_FALSE(tx.outputs.empty());

  IFusionManager::EstimateResult expectedResult = {0, tx.outputs.size()};
  size_t maxOutputIndex = 0;
  uint64_t maxOutputAmount = 0;
  for (size_t i = 0; i < tx.outputs.size(); ++i) {
    if (tx.outputs[i].amount > maxOutputAmount) {
      maxOutputAmount = tx.outputs[i].amount;
      maxOutputIndex = i;
    }

    if (currency.isAmountApplicableInFusionTransactionInput(tx.outputs[i].amount, tx.outputs[i].amount + 1)) {
      ++expectedResult.fusionReadyCount;
    }
  }

  ASSERT_EQ(expectedResult, alice.estimate(tx.outputs[maxOutputIndex].amount + 1));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionThrowsIfNotInitialized) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  ASSERT_ANY_THROW(wallet.isFusionTransaction(0));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionThrowsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.isFusionTransaction(0));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionEmpty) {
  ASSERT_ANY_THROW(alice.isFusionTransaction(0));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionNotFusion) {
  generateAndUnlockMoney();

  ASSERT_EQ(1, alice.getTransactionCount());
  ASSERT_FALSE(alice.isFusionTransaction(0));
}

TEST_F(WalletApi, fusionManagerIsFusionTransaction) {
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);

  auto id = alice.createFusionTransaction(FUSION_THRESHOLD, 0);
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, id);

  node.updateObservers();
  waitForTransactionUpdated(alice, id);

  ASSERT_TRUE(alice.isFusionTransaction(id));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionNotInTransfersContainer) {
  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);

  auto id = alice.createFusionTransaction(FUSION_THRESHOLD, 0);
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, id);

  ASSERT_TRUE(alice.isFusionTransaction(id));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionThrowsIfOutOfRange) {
  ASSERT_ANY_THROW(alice.isFusionTransaction(1));
}

TEST_F(WalletApi, fusionManagerIsFusionTransactionSpent) {
  CryptoNote::WalletGreen wallet(dispatcher, currency, node, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  wallet.createAddress();

  generateFusionOutputsAndUnlock(alice, node, currency, FUSION_THRESHOLD);
  auto initialBalance = alice.getActualBalance();

  auto id = alice.createFusionTransaction(FUSION_THRESHOLD, 0);
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, id);

  unlockMoney();
  waitForActualBalance(initialBalance);

  auto pending = wallet.getPendingBalance();
  ASSERT_NE(0, alice.getActualBalance());
  sendMoney(wallet.getAddress(0), alice.getActualBalance() - currency.minimumFee(), currency.minimumFee());

  node.updateObservers();
  waitPendingBalanceUpdated(wallet, pending);

  ASSERT_TRUE(alice.isFusionTransaction(id));
}

size_t findDonationTransferId(const WalletGreen& wallet, size_t transactionId) {
  for (size_t i = 0; i < wallet.getTransactionTransferCount(transactionId); ++i) {
    if (wallet.getTransactionTransfer(transactionId, i).type == WalletTransferType::DONATION) {
      return i;
    }
  }

  return WALLET_INVALID_TRANSFER_ID;
}

TEST_F(WalletApi, donationTransferPresents) {
  const uint64_t DONATION_THRESHOLD = 1000000;

  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE + DONATION_THRESHOLD);
  unlockMoney();

  auto transactionId = sendMoneyWithDonation(RANDOM_ADDRESS, SENT, FEE, RANDOM_ADDRESS, DONATION_THRESHOLD);

  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, transactionId);

  auto donationTransferId = findDonationTransferId(alice, transactionId);
  ASSERT_NE(WALLET_INVALID_TRANSFER_ID, donationTransferId);

  auto donationTransfer = alice.getTransactionTransfer(transactionId, donationTransferId);
  ASSERT_EQ(WalletTransferType::DONATION, donationTransfer.type);
  ASSERT_EQ(DONATION_THRESHOLD, donationTransfer.amount);
  ASSERT_EQ(RANDOM_ADDRESS, donationTransfer.address);
}

TEST_F(WalletApi, donationDidntHappenIfNotEnoughMoney) {
  const uint64_t DONATION_THRESHOLD = 1000000;

  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE);
  unlockMoney();

  auto transactionId = sendMoneyWithDonation(RANDOM_ADDRESS, SENT, FEE, RANDOM_ADDRESS, DONATION_THRESHOLD);
  ASSERT_NE(WALLET_INVALID_TRANSACTION_ID, transactionId);
  ASSERT_EQ(WALLET_INVALID_TRANSFER_ID, findDonationTransferId(alice, transactionId));
}

TEST_F(WalletApi, donationThrowsIfAddressEmpty) {
  const uint64_t DONATION_THRESHOLD = 1000000;

  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE + DONATION_THRESHOLD);
  unlockMoney();

  TransactionParameters params;
  params.destinations.push_back({RANDOM_ADDRESS, SENT});
  params.fee = FEE;
  params.donation.threshold = DONATION_THRESHOLD;

  ASSERT_ANY_THROW(alice.transfer(params));
}

TEST_F(WalletApi, donationThrowsIfThresholdZero) {
  const uint64_t DONATION_THRESHOLD = 1000000;

  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE + DONATION_THRESHOLD);
  unlockMoney();

  TransactionParameters params;
  params.destinations.push_back({RANDOM_ADDRESS, SENT});
  params.fee = FEE;
  params.donation.address = RANDOM_ADDRESS;
  params.donation.threshold = 0;

  ASSERT_ANY_THROW(alice.transfer(params));
}

TEST_F(WalletApi, donationTransactionHaveCorrectFee) {
  CatchTransactionNodeStub catchNode(generator);
  CryptoNote::WalletGreen wallet(dispatcher, currency, catchNode, logger);
  wallet.initialize(BOB_WALLET_PATH, "pass");
  wallet.createAddress();

  const uint64_t DONATION_THRESHOLD = 1000000;

  generator.getSingleOutputTransaction(parseAddress(wallet.getAddress(0)), SENT + FEE + DONATION_THRESHOLD);
  unlockMoney(wallet, catchNode);

  TransactionParameters params;
  params.destinations.push_back({RANDOM_ADDRESS, SENT});
  params.fee = FEE;
  params.donation.address = RANDOM_ADDRESS;
  params.donation.threshold = DONATION_THRESHOLD;

  wallet.transfer(params);

  ASSERT_TRUE(catchNode.caught);
  ASSERT_EQ(FEE, getInputAmount(catchNode.transaction) - getOutputAmount(catchNode.transaction));

  wallet.shutdown();
}

TEST_F(WalletApi, donationSerialization) {
  const uint64_t DONATION_THRESHOLD = 1000000;

  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE + DONATION_THRESHOLD);
  unlockMoney();

  sendMoneyWithDonation(RANDOM_ADDRESS, SENT, FEE, RANDOM_ADDRESS, DONATION_THRESHOLD);

  alice.save();
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.load(BOB_WALLET_PATH, "pass");

  compareWalletsTransactionTransfers(alice, bob, true);
  bob.shutdown();
}

TEST_F(WalletApi, transferThrowsIfDonationThresholdTooBig) {
  const uint64_t DONATION_THRESHOLD = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;

  generator.getSingleOutputTransaction(parseAddress(aliceAddress), SENT + FEE);
  unlockMoney();

  ASSERT_ANY_THROW(sendMoneyWithDonation(RANDOM_ADDRESS, SENT, FEE, RANDOM_ADDRESS, DONATION_THRESHOLD));
}

namespace {

class WalletApi_makeTransaction : public WalletApi {
public:
  WalletApi_makeTransaction() :
    WalletApi() {
  }

protected:
  int makeAliceTransactionAndReturnErrorCode(const std::string& sourceAddress, const std::vector<CryptoNote::WalletOrder>& destinations,
    uint64_t fee, uint16_t mixIn, const std::string& extra = "") {

    try {
      makeTransaction({sourceAddress}, destinations, fee, mixIn, extra);
    } catch (std::system_error& e) {
      return e.code().value();
    }

    return 0;
  }

  std::string getExtraForBigTransaction() const {
    size_t extraSize = 2 * currency.blockGrantedFullRewardZone();
    return std::string(extraSize, static_cast<std::string::value_type>(0));
  }
};

}

TEST_F(WalletApi_makeTransaction, throwsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(makeTransaction({}, RANDOM_ADDRESS, SENT, FEE, 0));
}

TEST_F(WalletApi_makeTransaction, throwsIfSourceAddressIsInvalid) {
  generateAndUnlockMoney();
  ASSERT_ANY_THROW(makeTransaction({"not an address"}, RANDOM_ADDRESS, SENT, FEE, 0));
}

TEST_F(WalletApi_makeTransaction, throwsIfDestinationsIsEmpty) {
  generateAndUnlockMoney();
  int error = makeAliceTransactionAndReturnErrorCode(alice.getAddress(0), {}, FEE, 0);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::ZERO_DESTINATION), error);
}

TEST_F(WalletApi_makeTransaction, throwsIfDestinationsHasInvalidAddress) {
  generateAndUnlockMoney();
  int error = makeAliceTransactionAndReturnErrorCode({alice.getAddress(0)}, { CryptoNote::WalletOrder{ "not an address", SENT } }, FEE, 0);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::BAD_ADDRESS), error);
}

TEST_F(WalletApi_makeTransaction, throwsIfDestinationHasZeroAmount) {
  generateAndUnlockMoney();
  int error = makeAliceTransactionAndReturnErrorCode({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, 0 } }, FEE, 0);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::ZERO_DESTINATION), error);
}

TEST_F(WalletApi_makeTransaction, throwsIfDestinationHasTooBigAmount) {
  generateAndUnlockMoney();
  CryptoNote::WalletOrder order;
  order.address = RANDOM_ADDRESS;
  order.amount = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
  int error = makeAliceTransactionAndReturnErrorCode({alice.getAddress(0)}, { order }, FEE, 0);
  ASSERT_EQ(static_cast<int>(CryptoNote::error::WalletErrorCodes::WRONG_AMOUNT), error);
}

TEST_F(WalletApi_makeTransaction, throwsIfSumOfDestinationsAmountsOverflows) {
  generateAndUnlockMoney();
  std::vector<WalletOrder> destinations;
  destinations.push_back({ RANDOM_ADDRESS, SENT });
  destinations.push_back({ RANDOM_ADDRESS, std::numeric_limits<uint64_t>::max() });
  int error = makeAliceTransactionAndReturnErrorCode({alice.getAddress(0)}, destinations, FEE, 0);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::WRONG_AMOUNT), error);
}

TEST_F(WalletApi_makeTransaction, throwsIfFeeIsLessThanMinimumFee) {
  if (currency.minimumFee() > 0) {
    generateAndUnlockMoney();
    int error = makeAliceTransactionAndReturnErrorCode({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, currency.minimumFee() - 1, 0);
    ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::FEE_TOO_SMALL), error);
  }
}

TEST_F(WalletApi_makeTransaction, throwsIfWalletHasNotEnoughMoney) {
  generateAndUnlockMoney();
  uint64_t available = alice.getActualBalance();
  ASSERT_GT(available, FEE);
  uint64_t amount = available - FEE + 1;
  int error = makeAliceTransactionAndReturnErrorCode({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, amount } }, FEE, 0);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::WRONG_AMOUNT), error);
}

TEST_F(WalletApi_makeTransaction, throwsIfMixInIsTooBig) {
  generateAndUnlockMoney();
  uint16_t mixin = 10;
  node.setMaxMixinCount(mixin - 1);
  int error = makeAliceTransactionAndReturnErrorCode({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, mixin);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::MIXIN_COUNT_TOO_BIG), error);
}

TEST_F(WalletApi_makeTransaction, throwsIfTransactionIsTooBig) {
  generateAndUnlockMoney();
  std::string extra = getExtraForBigTransaction();
  int error = makeAliceTransactionAndReturnErrorCode({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0, extra);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::TRANSACTION_SIZE_TOO_BIG), error);
}

TEST_F(WalletApi_makeTransaction, createdTransactionCanBeReceivedByGetTransactionAndHasCorrectFieldValues) {
  const uint64_t MONEY = SENT + FEE + 1;
  generator.getSingleOutputTransaction(parseAddress(aliceAddress), MONEY);
  unlockMoney();

  std::string extra = "some extra";
  uint64_t unlockTimestamp = 7823673;

  auto txId = makeTransaction({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0, extra, unlockTimestamp);

  waitForTransactionUpdated(alice, txId);

  CryptoNote::WalletTransaction tx;
  ASSERT_NO_THROW(tx = alice.getTransaction(txId));
  ASSERT_EQ(WalletTransactionState::CREATED, tx.state);
  ASSERT_EQ(0, tx.timestamp);
  ASSERT_EQ(WALLET_UNCONFIRMED_TRANSACTION_HEIGHT, tx.blockHeight);
  ASSERT_EQ(-static_cast<int64_t>(SENT + FEE), tx.totalAmount);
  ASSERT_EQ(FEE, tx.fee);
  ASSERT_NE(0, tx.creationTime);
  ASSERT_EQ(unlockTimestamp, tx.unlockTime);
  ASSERT_NE(std::string::npos, tx.extra.find(extra));
  ASSERT_FALSE(tx.isBase);

  auto transfers = getTransfersFromTransaction(alice, txId);
  ASSERT_EQ(3, transfers.size()); //one transfer for source address, one transfer for destination, one transfer for change

  //source
  EXPECT_EQ(aliceAddress, transfers[0].address);
  EXPECT_EQ(-static_cast<int64_t>(MONEY), transfers[0].amount);

  //change
  EXPECT_EQ(aliceAddress, transfers[1].address);
  EXPECT_EQ(MONEY - SENT - FEE, transfers[1].amount);

  //destination
  EXPECT_EQ(RANDOM_ADDRESS, transfers[2].address);
  EXPECT_EQ(SENT, transfers[2].amount);
}

TEST_F(WalletApi_makeTransaction, methodLocksMoneyUsedInTransaction) {
  generateAndUnlockMoney();

  std::string sourceAddress = alice.getAddress(0);
  uint64_t actualBefore = alice.getActualBalance(sourceAddress);
  uint64_t pendingBefore = alice.getPendingBalance(sourceAddress);
  auto txId = makeTransaction({sourceAddress}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0);

  waitForTransactionUpdated(alice, txId);

  ASSERT_GE(actualBefore - SENT - FEE, alice.getActualBalance(sourceAddress));
  ASSERT_LE(pendingBefore, alice.getPendingBalance(sourceAddress));
}

TEST_F(WalletApi_makeTransaction, ifFailedMoneyDoesNotLocked) {
  generateAndUnlockMoney();

  std::string sourceAddress = alice.getAddress(0);
  uint64_t actualBefore = alice.getActualBalance(sourceAddress);
  uint64_t pendingBefore = alice.getPendingBalance(sourceAddress);
  ASSERT_ANY_THROW(makeTransaction({sourceAddress}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0, getExtraForBigTransaction()));

  ASSERT_EQ(actualBefore, alice.getActualBalance(sourceAddress));
  ASSERT_EQ(pendingBefore, alice.getPendingBalance(sourceAddress));
}

TEST_F(WalletApi_makeTransaction, sendsTransactionCreatedEvent) {
  generateAndUnlockMoney();
  makeTransaction({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0);
  ASSERT_TRUE(waitForWalletEvent(alice, WalletEventType::TRANSACTION_CREATED, std::chrono::seconds(5)));
}

TEST_F(WalletApi_makeTransaction, ifFailedDoesNotSendTransactionCreatedEvent) {
  generateAndUnlockMoney();

  System::Context<bool> eventContext(dispatcher, [this]() {
    bool res;

    for (;;) {
      try {
        CryptoNote::WalletEvent event = alice.getEvent();
        if (event.type == WalletEventType::TRANSACTION_CREATED) {
          res = true;
          break;
        }
      } catch (System::InterruptedException&) {
        res = false;
        break;
      }
    }

    return res;
  });

  ASSERT_ANY_THROW(makeTransaction({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0, getExtraForBigTransaction()));

  dispatcher.yield();
  eventContext.interrupt();
  ASSERT_FALSE(eventContext.get());
}

namespace {

class WalletApi_commitTransaction : public WalletApi {
public:
  WalletApi_commitTransaction() :
    WalletApi() {
  }

protected:
  size_t generateMoneyAndMakeAliceTransaction() {
    generateAndUnlockMoney();
    return makeTransaction({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0);
  }

  int commitAliceTransactionAndReturnErrorCode(size_t transactionId) {
    try {
      alice.commitTransaction(transactionId);
    } catch (std::system_error& e) {
      return e.code().value();
    }

    return 0;
  }
};

}

TEST_F(WalletApi_commitTransaction, throwsIfStopped) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.stop();
  ASSERT_ANY_THROW(alice.commitTransaction(txId));
}

TEST_F(WalletApi_commitTransaction, throwsIfTransactionIdIsInvalid) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  int error = commitAliceTransactionAndReturnErrorCode(txId + 1);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::INDEX_OUT_OF_RANGE), error);
}

TEST_F(WalletApi_commitTransaction, throwsIfTransactionIsInSucceededState) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.commitTransaction(txId);

  int error = commitAliceTransactionAndReturnErrorCode(txId);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::TX_TRANSFER_IMPOSSIBLE), error);
}

TEST_F(WalletApi_commitTransaction, canSendTransactionAfterFail) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  node.setNextTransactionError();
  ASSERT_ANY_THROW(alice.commitTransaction(txId));

  ASSERT_NO_THROW(alice.commitTransaction(txId));
}

TEST_F(WalletApi_commitTransaction, throwsIfTransactionIsInCancelledState) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.rollbackUncommitedTransaction(txId);

  int error = commitAliceTransactionAndReturnErrorCode(txId);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::TX_TRANSFER_IMPOSSIBLE), error);
}

TEST_F(WalletApi_commitTransaction, changesTransactionStateToSucceededIfTransactionSent) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.commitTransaction(txId);
  auto tx = alice.getTransaction(txId);
  ASSERT_EQ(WalletTransactionState::SUCCEEDED, tx.state);
}

TEST_F(WalletApi_commitTransaction, remainsTransactionStateCreatedIfTransactionSendFailed) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  node.setNextTransactionError();
  ASSERT_ANY_THROW(alice.commitTransaction(txId));
  auto tx = alice.getTransaction(txId);
  ASSERT_EQ(WalletTransactionState::CREATED, tx.state);
}

TEST_F(WalletApi_commitTransaction, doesNotUnlockMoneyIfTransactionCommitFailed) {
  generateAndUnlockMoney();

  std::string sourceAddress = alice.getAddress(0);
  auto txId = makeTransaction({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0);

  uint64_t actualBefore = alice.getActualBalance(sourceAddress);
  uint64_t pendingBefore = alice.getPendingBalance(sourceAddress);

  node.setNextTransactionError();
  ASSERT_ANY_THROW(alice.commitTransaction(txId));

  ASSERT_EQ(actualBefore, alice.getActualBalance(sourceAddress));
  ASSERT_EQ(pendingBefore, alice.getPendingBalance(sourceAddress));
}

TEST_F(WalletApi_commitTransaction, doesNotChangeBalanceIfTransactionSent) {
  generateAndUnlockMoney();

  std::string sourceAddress = alice.getAddress(0);
  auto txId = makeTransaction({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0);
  waitForTransactionUpdated(alice, txId);

  uint64_t actualBefore = alice.getActualBalance(sourceAddress);
  uint64_t pendingBefore = alice.getPendingBalance(sourceAddress);

  alice.commitTransaction(txId);

  waitForTransactionUpdated(alice, txId);

  EXPECT_EQ(actualBefore, alice.getActualBalance(sourceAddress));
  EXPECT_EQ(pendingBefore, alice.getPendingBalance(sourceAddress));
}

TEST_F(WalletApi_commitTransaction, sendsTransactionUpdatedEventIfTransactionSent) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.commitTransaction(txId);

  ASSERT_TRUE(waitForWalletEvent(alice, WalletEventType::TRANSACTION_UPDATED, std::chrono::seconds(5)));
}

namespace {

class WalletApi_rollbackUncommitedTransaction : public WalletApi {
public:
  WalletApi_rollbackUncommitedTransaction() :
    WalletApi() {
  }

protected:
  size_t generateMoneyAndMakeAliceTransaction() {
    generateAndUnlockMoney();
    auto txId = makeTransaction({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0);

    waitForTransactionUpdated(alice, txId);
    return txId;
  }

  int rollbackAliceTransactionAndReturnErrorCode(size_t transactionId) {
    try {
      alice.rollbackUncommitedTransaction(transactionId);
    } catch (std::system_error& e) {
      return e.code().value();
    }

    return 0;
  }
};

}

TEST_F(WalletApi_rollbackUncommitedTransaction, throwsIfStopped) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.stop();
  ASSERT_ANY_THROW(alice.rollbackUncommitedTransaction(txId));
}

TEST_F(WalletApi_rollbackUncommitedTransaction, throwsIfTransactionIdIsInvalid) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  int error = rollbackAliceTransactionAndReturnErrorCode(txId + 1);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::INDEX_OUT_OF_RANGE), error);
}

TEST_F(WalletApi_rollbackUncommitedTransaction, throwsIfTransactionIsInSucceededState) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.commitTransaction(txId);

  int error = rollbackAliceTransactionAndReturnErrorCode(txId);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::TX_CANCEL_IMPOSSIBLE), error);
}

TEST_F(WalletApi_rollbackUncommitedTransaction, rollsBackTransactionAfterFail) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  node.setNextTransactionError();
  ASSERT_ANY_THROW(alice.commitTransaction(txId));

  int error = rollbackAliceTransactionAndReturnErrorCode(txId);
  ASSERT_EQ(0, error);
}

TEST_F(WalletApi_rollbackUncommitedTransaction, throwsIfTransactionIsInCancelledState) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.rollbackUncommitedTransaction(txId);

  int error = rollbackAliceTransactionAndReturnErrorCode(txId);
  ASSERT_EQ(static_cast<int>(error::WalletErrorCodes::TX_CANCEL_IMPOSSIBLE), error);
}

TEST_F(WalletApi_rollbackUncommitedTransaction, changesTransactionStateToCancelledIfTransactionRolledback) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.rollbackUncommitedTransaction(txId);
  auto tx = alice.getTransaction(txId);
  ASSERT_EQ(WalletTransactionState::CANCELLED, tx.state);
}

TEST_F(WalletApi_rollbackUncommitedTransaction, doesNotChangeTransactionStateToCancelledIfTransactionRolledbackFailed) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.commitTransaction(txId);
  ASSERT_ANY_THROW(alice.rollbackUncommitedTransaction(txId));
  auto tx = alice.getTransaction(txId);
  ASSERT_NE(WalletTransactionState::CANCELLED, tx.state);
}

TEST_F(WalletApi_rollbackUncommitedTransaction, unlocksMoneyIfTransactionRolledback) {
  generateAndUnlockMoney();

  std::string sourceAddress = alice.getAddress(0);
  uint64_t actualBefore = alice.getActualBalance(sourceAddress);
  uint64_t pendingBefore = alice.getPendingBalance(sourceAddress);

  auto txId = makeTransaction({alice.getAddress(0)}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0);
  alice.rollbackUncommitedTransaction(txId);

  ASSERT_EQ(actualBefore, alice.getActualBalance(sourceAddress));
  ASSERT_EQ(pendingBefore, alice.getPendingBalance(sourceAddress));
}

TEST_F(WalletApi_rollbackUncommitedTransaction, doesNotChangeBalanceIfTransactionRollbackFailed) {
  generateAndUnlockMoney();

  std::string sourceAddress = alice.getAddress(0);
  auto txId = makeTransaction({sourceAddress}, { CryptoNote::WalletOrder{ RANDOM_ADDRESS, SENT } }, FEE, 0);
  alice.rollbackUncommitedTransaction(txId);

  uint64_t actualBefore = alice.getActualBalance(sourceAddress);
  uint64_t pendingBefore = alice.getPendingBalance(sourceAddress);
  ASSERT_ANY_THROW(alice.rollbackUncommitedTransaction(txId));

  ASSERT_EQ(actualBefore, alice.getActualBalance(sourceAddress));
  ASSERT_EQ(pendingBefore, alice.getPendingBalance(sourceAddress));
}

TEST_F(WalletApi_rollbackUncommitedTransaction, sendsTransactionUpdatedEventIfTransactionRolledback) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.rollbackUncommitedTransaction(txId);

  ASSERT_TRUE(waitForWalletEvent(alice, WalletEventType::TRANSACTION_UPDATED, std::chrono::seconds(5)));
}

TEST_F(WalletApi_rollbackUncommitedTransaction, doesNotSendTransactionUpdatedEventIfTransactionRollbackFailed) {
  auto txId = generateMoneyAndMakeAliceTransaction();
  alice.commitTransaction(txId);
  ASSERT_TRUE(waitForWalletEvent(alice, WalletEventType::TRANSACTION_UPDATED, std::chrono::seconds(5)));

  System::Context<bool> eventContext(dispatcher, [this]() {
    bool res;

    for (;;) {
      try {
        CryptoNote::WalletEvent event = alice.getEvent();
        if (event.type == WalletEventType::TRANSACTION_UPDATED) {
          res = true;
          break;
        }
      } catch (System::InterruptedException&) {
        res = false;
        break;
      }
    }

    return res;
  });

  ASSERT_ANY_THROW(alice.rollbackUncommitedTransaction(txId));

  dispatcher.yield();
  eventContext.interrupt();
  ASSERT_FALSE(eventContext.get());
}

TEST_F(WalletApi, getTransactionThrowsIfTransactionNotFound) {
  Crypto::Hash hash;
  std::generate(std::begin(hash.data), std::end(hash.data), std::rand);

  ASSERT_ANY_THROW(alice.getTransaction(hash));
}

TEST_F(WalletApi, getTransactionThrowsIfStopped) {
  alice.stop();

  Crypto::Hash hash;
  std::generate(std::begin(hash.data), std::end(hash.data), std::rand);

  ASSERT_ANY_THROW(alice.getTransaction(hash));
}

TEST_F(WalletApi, getTransactionThrowsIfNotInitialized) {
  WalletGreen wallet(dispatcher, currency, node, logger);

  Crypto::Hash hash;
  std::generate(std::begin(hash.data), std::end(hash.data), std::rand);

  ASSERT_ANY_THROW(wallet.getTransaction(hash));
}

TEST_F(WalletApi, getTransactionReturnsCorrectTransaction) {
  const uint64_t MONEY = 2 * SENT + 2 * FEE + 1;

  generator.getSingleOutputTransaction(parseAddress(aliceAddress), MONEY);
  unlockMoney();

  CryptoNote::TransactionParameters params;
  params.destinations = { CryptoNote::WalletOrder {RANDOM_ADDRESS, SENT},  CryptoNote::WalletOrder {RANDOM_ADDRESS, SENT + FEE} };
  params.fee = FEE;

  auto txId = alice.transfer(params);

  waitForTransactionUpdated(alice, txId); //first notification comes right after inserting transaction. totalAmount at the moment is 0
  waitForTransactionUpdated(alice, txId); //second notification comes after processing the transaction by TransfersContainer

  Crypto::Hash hash = alice.getTransaction(txId).hash;

  CryptoNote::WalletTransactionWithTransfers tx = alice.getTransaction(hash);
  CryptoNote::WalletTransaction transaction = tx.transaction;

  EXPECT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, transaction.state);
  EXPECT_EQ(CryptoNote::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT, transaction.blockHeight);
  EXPECT_EQ(FEE, transaction.fee);
  EXPECT_FALSE(transaction.isBase);
  EXPECT_EQ(0, transaction.unlockTime);

  ASSERT_EQ(-static_cast<int64_t>(SENT * 2 + FEE * 2), tx.transaction.totalAmount);

  ASSERT_EQ(4, tx.transfers.size()); //2 transfers for user's orders, 1 transfer for change, 1 transfer for source
  sortTransfersByAmount(tx.transfers);

  //source
  EXPECT_EQ(aliceAddress, tx.transfers[0].address);
  EXPECT_EQ(-static_cast<int64_t>(MONEY), tx.transfers[0].amount);

  //change
  EXPECT_EQ(aliceAddress, tx.transfers[1].address);
  EXPECT_EQ(static_cast<int64_t>(MONEY - 2 * SENT - 2 * FEE), tx.transfers[1].amount);

  //destinations
  EXPECT_EQ(RANDOM_ADDRESS, tx.transfers[2].address);
  EXPECT_EQ(static_cast<int64_t>(SENT), tx.transfers[2].amount);

  EXPECT_EQ(RANDOM_ADDRESS, tx.transfers[3].address);
  EXPECT_EQ(static_cast<int64_t>(SENT + FEE), tx.transfers[3].amount);
}

TEST_F(WalletApi, getTransactionsThrowsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getTransactions(0, 10));
  alice.start();
}

TEST_F(WalletApi, getTransactionsThrowsIfNotInitialized) {
  WalletGreen wallet(dispatcher, currency, node, logger);
  ASSERT_ANY_THROW(wallet.getTransactions(0, 10));
}

TEST_F(WalletApi, getTransactionsThrowsCountZero) {
  ASSERT_ANY_THROW(alice.getTransactions(0, 0));
}

TEST_F(WalletApi, getTransactionsReturnsEmptyArrayIfBlockIndexTooBig) {
  auto transactions = alice.getTransactions(1, 1);
  ASSERT_TRUE(transactions.empty());
}

TEST_F(WalletApi, transferDoesntAppearTwiceAfterIncludingToBlockchain) {
  //we generate single output transaction to make sure we'll have change transfer in transaction
  generator.getSingleOutputTransaction(parseAddress(aliceAddress), 2 * SENT + FEE);
  unlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, 1);
  bob.initialize(BOB_WALLET_PATH, "p");

  node.setNextTransactionToPool();
  sendMoney(bob.createAddress(), SENT, FEE);

  node.sendPoolChanged();

  waitForTransactionCount(bob, 1);
  waitForWalletEvent(bob, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  ASSERT_EQ(3, bob.getTransactionTransferCount(0));

  node.includeTransactionsFromPoolToBlock();
  generator.generateEmptyBlocks(1);
  node.updateObservers();
  waitForWalletEvent(bob, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  ASSERT_EQ(3, bob.getTransactionTransferCount(0));
}

TEST_F(WalletApi, incomingTransactionToTwoAddressesContainsTransfersForEachAddress) {
  //we don't want to produce change
  generator.getSingleOutputTransaction(parseAddress(aliceAddress), 2 * SENT + 2 * FEE);
  unlockMoney();

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, 1);
  bob.initialize(BOB_WALLET_PATH, "p");

  CryptoNote::TransactionParameters params;
  params.destinations = {{bob.createAddress(), SENT}, {bob.createAddress(), SENT + FEE}};
  params.fee = FEE;

  waitForWalletEvent(bob, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  alice.transfer(params);
  node.updateObservers();

  waitForTransactionCount(bob, 1);
  waitForWalletEvent(bob, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  //2 outcoming transfers to bob's addresses and one incoming for alice
  ASSERT_EQ(3, bob.getTransactionTransferCount(0));

  std::vector<CryptoNote::WalletTransfer> receivedTransfers = getTransfersFromTransaction(bob, 0, true);
  std::sort(receivedTransfers.begin(), receivedTransfers.end());

  //we expect to have exactly 2 positive transfers - one for each bob's address
  ASSERT_EQ(2, receivedTransfers.size());

  ASSERT_EQ(bob.getAddress(0), receivedTransfers[0].address);
  ASSERT_EQ(SENT, receivedTransfers[0].amount);

  ASSERT_EQ(bob.getAddress(1), receivedTransfers[1].address);
  ASSERT_EQ(SENT + FEE, receivedTransfers[1].amount);
}

TEST_F(WalletApi, getTransactionsReturnsEmptyArrayIfBlockHashDoesntExist) {
  Crypto::Hash hash;
  std::generate(std::begin(hash.data), std::end(hash.data), std::rand);

  auto transactions = alice.getTransactions(hash, 1);
  ASSERT_TRUE(transactions.empty());
}

TEST_F(WalletApi, getTransactionsReturnsEmptyArrayWhenNoTransactions) {
  auto transactions = alice.getTransactions(0, 1);

  ASSERT_FALSE(transactions.empty());
  ASSERT_TRUE(transactions[0].transactions.empty());
}

bool compareTransactionsWithTransfers(CryptoNote::WalletTransactionWithTransfers& leftTransaction, CryptoNote::WalletTransactionWithTransfers& rightTransaction) {
  std::sort(leftTransaction.transfers.begin(), leftTransaction.transfers.end());
  std::sort(rightTransaction.transfers.begin(), rightTransaction.transfers.end());

  if (leftTransaction.transaction != rightTransaction.transaction) {
    return false;
  }

  return leftTransaction.transfers == rightTransaction.transfers;
}

CryptoNote::WalletTransactionWithTransfers makeTransactionWithTransfers(CryptoNote::WalletGreen& wallet, size_t transactionId) {
  CryptoNote::WalletTransactionWithTransfers transactionWithTransfers;
  transactionWithTransfers.transaction = wallet.getTransaction(transactionId);

  for (size_t i = 0; i < wallet.getTransactionTransferCount(transactionId); ++i ) {
    transactionWithTransfers.transfers.push_back(wallet.getTransactionTransfer(transactionId, i));
  }

  return transactionWithTransfers;
}

bool transactionWithTransfersFound(CryptoNote::WalletGreen& wallet, const std::vector<TransactionsInBlockInfo>& transactions, size_t transactionId) {
  CryptoNote::WalletTransactionWithTransfers walletTransaction = makeTransactionWithTransfers(wallet, transactionId);

  for (auto& block: transactions) {
    for (auto& transaction: block.transactions) {
      auto transactionCopy = transaction;
      if (compareTransactionsWithTransfers(walletTransaction, transactionCopy)) {
        return true;
      }
    }
  }

  return false;
}

size_t getTransactionsCount(const std::vector<TransactionsInBlockInfo>& transactions) {
  size_t count = 0;

  for (auto& block: transactions) {
    count += block.transactions.size();
  }

  return count;
}

TEST_F(WalletApi, getTransactionsDoesntReturnUnconfirmedTransactions) {
  generateAndUnlockMoney();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  size_t transactionId = sendMoney(RANDOM_ADDRESS, SENT, FEE);
  auto transactions = alice.getTransactions(0, generator.getBlockchain().size());

  ASSERT_FALSE(transactionWithTransfersFound(alice, transactions, transactionId));
}

TEST_F(WalletApi, getTransactionsReturnsCorrectTransactionsFromOneBlock) {
  generateAndUnlockMoney();
  const uint32_t MIXIN_1 = 1;
  const uint32_t MIXIN_2 = 0;

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  node.setNextTransactionToPool();
  size_t transactionId1 = sendMoney(RANDOM_ADDRESS, SENT, FEE, MIXIN_1);

  node.setNextTransactionToPool();
  size_t transactionId2 = sendMoney(RANDOM_ADDRESS, SENT + FEE, FEE, MIXIN_2);

  node.includeTransactionsFromPoolToBlock();
  node.updateObservers();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto transactions = alice.getTransactions(static_cast<uint32_t>(generator.getBlockchain().size()) - 1, 1);

  size_t transactionsCount = getTransactionsCount(transactions);
  ASSERT_EQ(2, transactionsCount);

  ASSERT_TRUE(transactionWithTransfersFound(alice, transactions, transactionId1));
  ASSERT_TRUE(transactionWithTransfersFound(alice, transactions, transactionId2));
}

Crypto::Hash getBlockHash(const CryptoNote::BlockTemplate& block) {
  CryptoNote::CachedBlock cachedBlock(block);
  return cachedBlock.getBlockHash();
}

TEST_F(WalletApi, getTransactionsReturnsBlockWithCorrectHash) {
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  generator.generateEmptyBlocks(1);
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  Crypto::Hash lastBlockHash = getBlockHash(generator.getBlockchain().back());
  auto transactions = alice.getTransactions(lastBlockHash, 1);

  ASSERT_EQ(1, transactions.size());
  ASSERT_EQ(lastBlockHash, transactions[0].blockHash);
}

TEST_F(WalletApi, getTransactionsReturnsCorrectTransactionByBlockHash) {
  generateAndUnlockMoney();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  size_t transactionId = sendMoney(RANDOM_ADDRESS, SENT, FEE);

  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  Crypto::Hash lastBlockHash = getBlockHash(generator.getBlockchain().back());
  auto transactions = alice.getTransactions(lastBlockHash, 1);

  ASSERT_TRUE(transactionWithTransfersFound(alice, transactions, transactionId));
}

TEST_F(WalletApi, getTransactionsDoesntReturnUnconfirmedIncomingTransactions) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");

  generateAndUnlockMoney();

  node.setNextTransactionToPool();
  sendMoney(bob.createAddress(), SENT, FEE);
  node.updateObservers();

  waitForTransactionCount(bob, 1);

  auto transactions = bob.getTransactions(0, generator.getBlockchain().size());
  ASSERT_EQ(0, getTransactionsCount(transactions));

  bob.shutdown();
}

TEST_F(WalletApi, getTransactionsReturnsConfirmedIncomingTransactions) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass2");

  generateAndUnlockMoney();

  sendMoney(bob.createAddress(), SENT, FEE);
  node.updateObservers();

  waitForTransactionCount(bob, 1);
  waitForWalletEvent(bob, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto transactions = bob.getTransactions(static_cast<uint32_t>(generator.getBlockchain().size()) - 1, 1);
  ASSERT_EQ(1, getTransactionsCount(transactions));
  ASSERT_TRUE(transactionWithTransfersFound(bob, transactions, 0));

  bob.shutdown();
}

TEST_F(WalletApi, getTransactionsDoesntReturnFailedTransactions) {
  generateAndUnlockMoney();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  node.setNextTransactionError();
  try {
    sendMoney(RANDOM_ADDRESS, SENT + FEE, FEE);
  } catch (std::exception&) {
  }

  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto transactions = alice.getTransactions(0, generator.getBlockchain().size());
  ASSERT_FALSE(transactionWithTransfersFound(alice, transactions, alice.getTransactionCount() - 1));
}

TEST_F(WalletApi, getTransactionsDoesntReturnDelayedTransactions) {
  generateAndUnlockMoney();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  size_t id = makeTransaction({}, RANDOM_ADDRESS, SENT, FEE);

  generator.generateEmptyBlocks(1);
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto transactions = alice.getTransactions(0, generator.getBlockchain().size());
  ASSERT_FALSE(transactionWithTransfersFound(alice, transactions, id));
}

TEST_F(WalletApi, getTransactionsReturnsDelayedTransactionsAfterSend) {
  generateAndUnlockMoney();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  size_t id = makeTransaction({}, RANDOM_ADDRESS, SENT, FEE);
  alice.commitTransaction(id);

  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto transactions = alice.getTransactions(static_cast<uint32_t>(generator.getBlockchain().size()) - 1, 1);
  ASSERT_TRUE(transactionWithTransfersFound(alice, transactions, id));
}

TEST_F(WalletApi, getTransactionsDoesntReturnDeletedTransactions) {
  generateAndUnlockMoney();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  uint32_t detachHeight = static_cast<uint32_t>(generator.getBlockchain().size()) - 1;
  size_t id = sendMoney(RANDOM_ADDRESS, SENT + FEE, FEE);

  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  node.startAlternativeChain(detachHeight);
  generator.generateEmptyBlocks(1);
  node.updateObservers();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto transactions = alice.getTransactions(static_cast<uint32_t>(generator.getBlockchain().size()) - 1, 1);
  ASSERT_FALSE(transactionWithTransfersFound(alice, transactions, id));
}

TEST_F(WalletApi, getTransactionsByBlockHashThrowsIfNotInitialized) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  auto hash = getBlockHash(generator.getBlockchain().back());
  ASSERT_ANY_THROW(bob.getTransactions(hash, 1));
}

TEST_F(WalletApi, getTransactionsByBlockHashThrowsIfStopped) {
  alice.stop();
  auto hash = getBlockHash(generator.getBlockchain().back());
  ASSERT_ANY_THROW(alice.getTransactions(hash, 1));
  alice.start();
}

TEST_F(WalletApi, getBlockHashesThrowsIfNotInitialized) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_ANY_THROW(bob.getBlockHashes(0, 1));
}

TEST_F(WalletApi, getBlockHashesThrowsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getBlockHashes(0, 1));
  alice.start();
}

TEST_F(WalletApi, getBlockHashesReturnsEmptyVectorIfBlockIndexGreaterThanBlockhainSize) {
  auto hashes = alice.getBlockHashes(1, 1);
  ASSERT_TRUE(hashes.empty());
}

TEST_F(WalletApi, getBlockHashesReturnsNewBlocks) {
  waitForPredicate(alice, [this] { return alice.getBlockCount() == 2; }, std::chrono::seconds(5));

  generator.generateEmptyBlocks(1);
  node.updateObservers();

  waitForPredicate(alice, [this] { return alice.getBlockCount() == 3; }, std::chrono::seconds(5));

  auto hash = getBlockHash(generator.getBlockchain().back());
  auto hashes = alice.getBlockHashes(0, generator.getBlockchain().size());

  ASSERT_EQ(generator.getBlockchain().size(), hashes.size());
  ASSERT_EQ(hash, hashes.back());
}

TEST_F(WalletApi, getBlockHashesReturnsCorrectBlockHashesAfterDetach) {
  generator.generateEmptyBlocks(1);

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  node.startAlternativeChain(1);
  generator.generateEmptyBlocks(1);
  node.updateObservers();

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto hash = getBlockHash(generator.getBlockchain()[1]);
  auto hashes = alice.getBlockHashes(0, 2);

  ASSERT_EQ(2, hashes.size());
  ASSERT_EQ(hash, hashes.back());
}

TEST_F(WalletApi, getBlockHashesReturnsOnlyGenesisBlockHashForWalletWithoutAddresses) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass");

  auto hashes = bob.getBlockHashes(0, 100);
  auto hash = hashes[0];

  ASSERT_EQ(1, hashes.size());
  ASSERT_EQ(currency.genesisBlockHash(), hash);
  bob.shutdown();
}

TEST_F(WalletApi, getBlockHashesReturnsOnlyGenesisBlockHashAfterDeletingAddresses) {
  generator.generateEmptyBlocks(1);

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  alice.deleteAddress(alice.getAddress(0));

  auto hashes = alice.getBlockHashes(0, 100);
  auto hash = hashes[0];

  ASSERT_EQ(1, hashes.size());
  ASSERT_EQ(currency.genesisBlockHash(), hash);
}

TEST_F(WalletApi, getBlockHashesReturnsCorrectHashesAfterLoad) {
  generator.generateEmptyBlocks(1);

  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto hashesBefore = alice.getBlockHashes(0, generator.getBlockchain().size());

  alice.save(WalletSaveLevel::SAVE_ALL);
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.load(BOB_WALLET_PATH, "pass");

  auto hashesAfter = bob.getBlockHashes(0, generator.getBlockchain().size());
  ASSERT_EQ(hashesBefore, hashesAfter);
  bob.shutdown();
}

TEST_F(WalletApi, getBlockCountThrowIfNotInitialized) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_ANY_THROW(bob.getBlockCount());
}

TEST_F(WalletApi, getBlockCountThrowIfNotStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getBlockCount());
  alice.start();
}

TEST_F(WalletApi, getBlockCountForWalletWithoutAddressesReturnsOne) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "pass");
  ASSERT_EQ(1, bob.getBlockCount());
  bob.shutdown();
}

TEST_F(WalletApi, getBlockCountReturnsCorrectBlockCount) {
  generator.generateEmptyBlocks(1);
  node.updateObservers();

  waitForPredicate(alice, [this] { return alice.getBlockCount() == 3; }, std::chrono::seconds(5));

  ASSERT_EQ(generator.getBlockchain().size(), alice.getBlockCount());
}

TEST_F(WalletApi, getBlockCountReturnsPlusOneAfterBlockAdded) {
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto prevBlockCount = alice.getBlockCount();

  generator.generateEmptyBlocks(1);
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  ASSERT_EQ(prevBlockCount + 1, alice.getBlockCount());
}

TEST_F(WalletApi, getBlockCountReturnsCorrectBlockCountAfterDetach) {
  generator.generateEmptyBlocks(2);
  node.updateObservers();
  waitForPredicate(alice, [this] { return alice.getBlockCount() == 4; }, std::chrono::seconds(5));

  auto prevBlockCount = alice.getBlockCount();

  uint32_t detachBlockIndex = static_cast<uint32_t>(generator.getBlockchain().size() - 2);
  node.startAlternativeChain(detachBlockIndex);
  generator.generateEmptyBlocks(1);
  node.updateObservers();
  waitForPredicate(alice, [this] { return alice.getBlockCount() == 3; }, std::chrono::seconds(5));

  ASSERT_EQ(prevBlockCount - 1, alice.getBlockCount());
}

TEST_F(WalletApi, getBlockCountReturnsOneAfterAddressesRemoving) {
  generator.generateEmptyBlocks(1);
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  alice.deleteAddress(alice.getAddress(0));
  ASSERT_EQ(1, alice.getBlockCount());
}

TEST_F(WalletApi, getBlockCountReturnsCorrectBlockCountAfterLoad) {
  generator.generateEmptyBlocks(1);
  node.updateObservers();
  waitForPredicate(alice, [this] { return alice.getBlockCount() == 3; }, std::chrono::seconds(5));

  auto aliceBlockCount = alice.getBlockCount();

  alice.save(WalletSaveLevel::SAVE_ALL);
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_NO_THROW(bob.load(BOB_WALLET_PATH, "pass"));

  ASSERT_EQ(aliceBlockCount, bob.getBlockCount());
  bob.shutdown();
}

TEST_F(WalletApi, getUnconfirmedTransactionsThrowsIfNotInitialized) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_ANY_THROW(bob.getUnconfirmedTransactions());
}

TEST_F(WalletApi, getUnconfirmedTransactionsThrowsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getUnconfirmedTransactions());
  alice.start();
}

TEST_F(WalletApi, getUnconfirmedTransactionsReturnsOneTransaction) {
  generateAndUnlockMoney();
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  CryptoNote::TransactionParameters params;
  params.destinations = {{RANDOM_ADDRESS, SENT}, {RANDOM_ADDRESS, SENT + FEE}};
  params.fee = FEE;

  node.setNextTransactionToPool();
  auto transaction = makeTransactionWithTransfers(alice, alice.transfer(params));

  auto unconfirmed = alice.getUnconfirmedTransactions();
  ASSERT_EQ(1, unconfirmed.size());
  ASSERT_TRUE(compareTransactionsWithTransfers(transaction, unconfirmed[0]));
}

TEST_F(WalletApi, getUnconfirmedTransactionsReturnsTwoTransactions) {
  generateAndUnlockMoney();
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  node.setNextTransactionToPool();
  auto transaction1 = makeTransactionWithTransfers(alice, sendMoney(RANDOM_ADDRESS, SENT, FEE));

  node.setNextTransactionToPool();
  auto transaction2 = makeTransactionWithTransfers(alice, sendMoney(RANDOM_ADDRESS, SENT + FEE, FEE));

  auto unconfirmed = alice.getUnconfirmedTransactions();
  ASSERT_EQ(2, unconfirmed.size());

  auto found1 = std::find_if(unconfirmed.begin(), unconfirmed.end(), [&transaction1] (CryptoNote::WalletTransactionWithTransfers& tr) {
    return compareTransactionsWithTransfers(transaction1, tr);
  });

  ASSERT_NE(unconfirmed.end(), found1);

  auto found2 = std::find_if(unconfirmed.begin(), unconfirmed.end(), [&transaction2] (CryptoNote::WalletTransactionWithTransfers& tr) {
    return compareTransactionsWithTransfers(transaction2, tr);
  });

  ASSERT_NE(unconfirmed.end(), found2);
}

TEST_F(WalletApi, getUnconfirmedTransactionsDoesntReturnFailedTransactions) {
  generateAndUnlockMoney();
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  node.setNextTransactionError();
  try {
    sendMoney(RANDOM_ADDRESS, SENT, FEE);
  } catch (std::exception&) {
  }

  auto unconfirmed = alice.getUnconfirmedTransactions();
  ASSERT_TRUE(unconfirmed.empty());
}

TEST_F(WalletApi, getUnconfirmedTransactionsDoesntReturnConfirmedTransactions) {
  generateAndUnlockMoney();
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto txId = sendMoney(RANDOM_ADDRESS, SENT, FEE);
  node.updateObservers();
  waitForTransactionConfirmed(alice, txId);

  auto unconfirmed = alice.getUnconfirmedTransactions();
  ASSERT_TRUE(unconfirmed.empty());
}

TEST_F(WalletApi, getDelayedTransactionIdsThrowsIfNotInitialized) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  ASSERT_ANY_THROW(bob.getDelayedTransactionIds());
}

TEST_F(WalletApi, getDelayedTransactionIdsThrowsIfStopped) {
  alice.stop();
  ASSERT_ANY_THROW(alice.getDelayedTransactionIds());
  alice.start();
}

TEST_F(WalletApi, getDelayedTransactionIdsThrowsIfInTrackingMode) {
  CryptoNote::WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.initialize(BOB_WALLET_PATH, "p");

  Crypto::PublicKey pub;
  Crypto::SecretKey sec;
  Crypto::generate_keys(pub, sec);

  bob.createAddress(pub);
  ASSERT_ANY_THROW(bob.getDelayedTransactionIds());
}

TEST_F(WalletApi, getDelayedTransactionIdsReturnsDelayedTransaction) {
  generateAndUnlockMoney();
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto id = makeTransaction({}, RANDOM_ADDRESS, SENT, FEE);

  auto delayed = alice.getDelayedTransactionIds();

  ASSERT_EQ(1, delayed.size());
  ASSERT_EQ(id, delayed[0]);
}

TEST_F(WalletApi, getDelayedTransactionIdsDoesntReturnSentTransactions) {
  generateAndUnlockMoney();
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  auto id = makeTransaction({}, RANDOM_ADDRESS, SENT, FEE);
  alice.commitTransaction(id);

  auto delayed = alice.getDelayedTransactionIds();
  ASSERT_TRUE(delayed.empty());
}

TEST_F(WalletApi, getDelayedTransactionIdsDoesntReturnFailedTransactions) {
  generateAndUnlockMoney();
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::WalletEventType::SYNC_COMPLETED, std::chrono::seconds(3));

  node.setNextTransactionError();
  try {
    sendMoney(RANDOM_ADDRESS, SENT, FEE);
  } catch (std::exception&){
  }

  auto delayed = alice.getDelayedTransactionIds();
  ASSERT_TRUE(delayed.empty());
}

TEST_F(WalletApi, transferFailsIfWrongChangeAddress) {
  CryptoNote::TransactionParameters params;
  params.destinations = {{RANDOM_ADDRESS, SENT}};
  params.fee = FEE;
  params.changeDestination = "Wrong address";

  ASSERT_ANY_THROW(alice.transfer(params));
}

TEST_F(WalletApi, transferFailsIfChangeAddressDoesntExist) {
  auto changeAddress = alice.createAddress();

  CryptoNote::TransactionParameters params;
  params.destinations = {{RANDOM_ADDRESS, SENT}};
  params.fee = FEE;
  params.changeDestination = changeAddress;
  alice.deleteAddress(changeAddress);

  ASSERT_ANY_THROW(alice.transfer(params));
}

TEST_F(WalletApi, transferFailsIfChangeAddressIsNotMine) {
  CryptoNote::TransactionParameters params;
  params.destinations = {{RANDOM_ADDRESS, SENT}};
  params.fee = FEE;
  params.changeDestination = RANDOM_ADDRESS;

  ASSERT_ANY_THROW(alice.transfer(params));
}

TEST_F(WalletApi, transferFailsIfWalletHasManyAddressesSourceAddressesNotSetAndNoChangeDestination) {
  alice.createAddress();
  CryptoNote::TransactionParameters params;
  params.destinations = {{RANDOM_ADDRESS, SENT}};
  params.fee = FEE;

  ASSERT_ANY_THROW(alice.transfer(params));
}

TEST_F(WalletApi, transferSendsChangeToSingleSpecifiedSourceAddress) {
  const uint64_t MONEY = SENT + FEE + 1;

  alice.createAddress();

  generator.getSingleOutputTransaction(parseAddress(alice.getAddress(1)), MONEY);
  unlockMoney();

  CryptoNote::TransactionParameters params;
  params.destinations = {{RANDOM_ADDRESS, SENT}};
  params.fee = FEE;
  params.sourceAddresses = {alice.getAddress(1)};

  alice.transfer(params);
  waitForActualBalance(alice, 0);

  EXPECT_EQ(MONEY - SENT - FEE, alice.getPendingBalance());
  EXPECT_EQ(MONEY - SENT - FEE, alice.getPendingBalance(alice.getAddress(1)));
}

TEST_F(WalletApi, transferFailsIfNoChangeDestinationAndMultipleSourceAddressesSet) {
  generateAndUnlockMoney();
  alice.createAddress();

  CryptoNote::TransactionParameters params;
  params.destinations = {{RANDOM_ADDRESS, SENT}};
  params.fee = FEE;
  params.sourceAddresses = {aliceAddress, alice.getAddress(1)};

  ASSERT_ANY_THROW(alice.transfer(params));
}

TEST_F(WalletApi, transferSendsChangeToAddress) {
  const uint64_t MONEY = SENT * 3;

  generator.getSingleOutputTransaction(parseAddress(aliceAddress), MONEY);
  unlockMoney();

  CryptoNote::TransactionParameters params;
  params.destinations = {{RANDOM_ADDRESS, SENT}};
  params.fee = FEE;
  params.changeDestination = alice.createAddress();

  alice.transfer(params);
  node.updateObservers();

  waitActualBalanceUpdated(MONEY);

  EXPECT_EQ(MONEY - SENT - FEE, alice.getPendingBalance());
  EXPECT_EQ(0, alice.getActualBalance());
  EXPECT_EQ(0, alice.getActualBalance(aliceAddress));
  EXPECT_EQ(0, alice.getPendingBalance(aliceAddress));
  EXPECT_EQ(0, alice.getActualBalance(alice.getAddress(1)));
  EXPECT_EQ(MONEY - SENT - FEE, alice.getPendingBalance(alice.getAddress(1)));
}

TEST_F(WalletApi, checkBaseTransaction) {
  CryptoNote::AccountKeys keys{ parseAddress(alice.getAddress(0)), alice.getAddressSpendKey(0).secretKey, alice.getViewKey().secretKey };
  CryptoNote::AccountBase acc;
  acc.setAccountKeys(keys);
  acc.set_createtime(0);
  generator.generateFromBaseTx(acc);

  node.updateObservers();
  waitForTransactionCount(alice, 1);

  ASSERT_EQ(1, alice.getTransactionCount());
  WalletTransaction tx = alice.getTransaction(0);
  EXPECT_TRUE(tx.isBase);
  EXPECT_EQ(0, tx.fee);
  EXPECT_EQ(CryptoNote::WalletTransactionState::SUCCEEDED, tx.state);

  ASSERT_EQ(1, alice.getTransactionTransferCount(0));
  WalletTransfer transfer = alice.getTransactionTransfer(0, 0);
  EXPECT_LT(0, transfer.amount);
  EXPECT_EQ(tx.totalAmount, transfer.amount);
}

TEST_F(WalletApi, walletResetsIfSavedCacheDoesNotContainAddedAddress) {
  // Create address with money
  ASSERT_EQ(1, alice.getAddressCount());
  auto address1 = alice.getAddress(0);
  generateBlockReward(address1);
  unlockMoney(alice, node);
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));
  auto aliceTransactions1 = exportWalletTransactions(alice);

  // Save wallet with one address and one transaction
  alice.save();

  // Create address, that transactions will not be saved
  auto address2 = alice.createAddress();

  // Send money, and unlock it
  generateBlockReward(address2);
  unlockMoney(alice, node);
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  // Send money, but not unlock it
  generateBlockReward(address2);
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  ASSERT_NE(0, alice.getActualBalance());
  ASSERT_NE(0, alice.getPendingBalance());
  ASSERT_NE(0, alice.getTransactionCount());

  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.load(BOB_WALLET_PATH, "pass");

  // Check wallet was reset, but has both addresses and first transactions
  compareWalletsAddresses(alice, bob);
  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(0, bob.getPendingBalance());
  compareWalletsTransactionTransfers(aliceTransactions1, bob, false);

  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  // Check wallet restored all transactions and balance
  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob, true);

  bob.shutdown();

  wait(100);
}

TEST_F(WalletApi, walletRemovesTransactionsForAddressesDeletedAfterSaving) {
  ASSERT_EQ(1, alice.getAddressCount());
  auto address1 = alice.getAddress(0);
  generateBlockReward(address1);
  unlockMoney(alice, node);
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));
  auto tx0 = alice.getTransactionCount() - 1;
  ASSERT_EQ(0, tx0);

  // Create address, that will be deleted
  auto address2 = alice.createAddress();

  // Create incoming transaction to address2
  generateBlockReward(address2);
  unlockMoney(alice, node);
  auto tx1 = alice.getTransactionCount() - 1;
  ASSERT_EQ(1, tx1);

  // Create transaction, that spend money only from address2
  // Spend all money in order to transaction doesn't have change
  uint64_t address2Balance = alice.getActualBalance(address2);
  uint64_t sendAmount = address2Balance - currency.minimumFee();
  auto tx2 = sendMoney(alice, { address2 }, RANDOM_ADDRESS, sendAmount, currency.minimumFee());
  generator.generateEmptyBlocks(1);

  // Create transaction, that transfers money from address1 to address2
  uint64_t address1Balance = alice.getActualBalance(address1);
  sendAmount = (address1Balance - currency.minimumFee()) / 2;
  auto tx3 = sendMoney(alice, { address1 }, address2, sendAmount, currency.minimumFee());
  generator.generateEmptyBlocks(1);
  unlockMoney(alice, node);
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  // Create transaction, that spends money from address1 and address2 and send change to address1
  address1Balance = alice.getActualBalance(address1);
  address2Balance = alice.getActualBalance(address2);
  sendAmount = address1Balance + address2Balance - currency.minimumFee() - 1;
  auto tx4 = sendMoney(alice, { address1, address2 }, RANDOM_ADDRESS, sendAmount, currency.minimumFee());
  generator.generateEmptyBlocks(1);

  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(5));

  // Save wallet with 2 addresses and 5 transactions
  alice.save();

  alice.deleteAddress(address2);

  ASSERT_EQ(0, alice.getActualBalance());
  ASSERT_EQ(1, alice.getPendingBalance());
  ASSERT_EQ(1, alice.getPendingBalance(address1));
  ASSERT_EQ(5, alice.getTransactionCount());
  ASSERT_EQ(WalletTransactionState::SUCCEEDED, alice.getTransaction(tx0).state);
  ASSERT_EQ(WalletTransactionState::DELETED, alice.getTransaction(tx1).state);
  ASSERT_EQ(WalletTransactionState::DELETED, alice.getTransaction(tx2).state);
  ASSERT_EQ(WalletTransactionState::SUCCEEDED, alice.getTransaction(tx3).state);
  ASSERT_EQ(WalletTransactionState::SUCCEEDED, alice.getTransaction(tx4).state);

  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.load(BOB_WALLET_PATH, "pass");

  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob, true);

  bob.shutdown();

  wait(100);
}

TEST_F(WalletApi, walletResetsAndRemovesObsoleteTransactionsIfOneAddressDeletedAndOneAddressAddedAfterSaving) {
  // Create address, that will be deleted
  auto address1 = alice.createAddress();

  // Create transaction for address2
  generateBlockReward(address1);
  unlockMoney(alice, node);
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));
  auto tx1 = alice.getTransactionCount() - 1;

  alice.save();

  // Create address, that transactions will not be saved
  auto address2 = alice.createAddress();

  // Create transaction for address3
  generateBlockReward(address2);
  unlockMoney(alice, node);
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));

  alice.deleteAddress(address1);

  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.load(BOB_WALLET_PATH, "pass");

  // Check wallet was reset, but has both addresses
  compareWalletsAddresses(alice, bob);
  ASSERT_EQ(0, bob.getActualBalance());
  ASSERT_EQ(0, bob.getPendingBalance());
  ASSERT_EQ(WalletTransactionState::DELETED, bob.getTransaction(tx1).state);
  ASSERT_EQ(tx1 + 1, bob.getTransactionCount());

  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));

  // Check wallet restored all transactions and balance
  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob, true);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, walletSavesAndLoadsExtra) {
  std::string savedExtra = "some extra data";
  alice.save(WalletSaveLevel::SAVE_ALL, savedExtra);

  alice.shutdown();

  std::string loadedExtra;
  alice.load(ALICE_WALLET_PATH, "pass", loadedExtra);

  ASSERT_EQ(savedExtra, loadedExtra);
}

TEST_F(WalletApi, walletHandlesResetAndSwitchingToAlternativeChain) {
  // Create transaction 1, that will be preserved
  generateBlockReward(aliceAddress);
  unlockMoney(alice, node);
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));
  auto tx1 = alice.getTransactionCount() - 1;

  uint32_t detachHeight = static_cast<uint32_t>(generator.getBlockchain().size());

  // Create transaction 2, that will be cancelled
  generateBlockReward(aliceAddress);
  unlockMoney(alice, node);
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));
  auto tx2 = alice.getTransactionCount() - 1;

  size_t alternativeChainSize = generator.getBlockchain().size() + 1;

  ASSERT_EQ(WalletTransactionState::SUCCEEDED, alice.getTransaction(tx1).state);
  ASSERT_EQ(WalletTransactionState::SUCCEEDED, alice.getTransaction(tx2).state);

  // Don't save cache
  alice.save(WalletSaveLevel::SAVE_KEYS_AND_TRANSACTIONS);
  boost::filesystem::copy(ALICE_WALLET_PATH, BOB_WALLET_PATH);

  // Switch to alternative chain, that doesn't have transaction 2
  node.startAlternativeChain(detachHeight);
  generator.generateEmptyBlocks(alternativeChainSize - detachHeight);
  node.updateObservers();
  waitForWalletEvent(alice, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));

  // Make sure transaction 2 was cancelled
  ASSERT_EQ(WalletTransactionState::SUCCEEDED, alice.getTransaction(tx1).state);
  ASSERT_EQ(WalletTransactionState::CANCELLED, alice.getTransaction(tx2).state);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  bob.load(BOB_WALLET_PATH, "pass");

  // Wallet was saved without cache, so all transactions should be canceled
  ASSERT_EQ(WalletTransactionState::CANCELLED, bob.getTransaction(tx1).state);
  ASSERT_EQ(WalletTransactionState::CANCELLED, bob.getTransaction(tx2).state);

  waitForWalletEvent(bob, CryptoNote::SYNC_COMPLETED, std::chrono::seconds(30));

  // Check after synchronization wallets are equal
  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob, true);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, exportedWalletIsFullCopyOfSourceWallet) {
  fillWalletWithDetailsCache();

  std::string savedExtra = "some extra data";
  alice.exportWallet(BOB_WALLET_PATH, true, WalletSaveLevel::SAVE_ALL, savedExtra);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  std::string loadedExtra;
  bob.load(BOB_WALLET_PATH, "pass", loadedExtra);

  // Check wallets are equal
  ASSERT_EQ(savedExtra, loadedExtra);
  compareWalletsAddresses(alice, bob);
  compareWalletsActualBalance(alice, bob);
  compareWalletsPendingBalance(alice, bob);
  compareWalletsTransactionTransfers(alice, bob, true);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, walletExportedWithoutEncryptionCanBeOpenedWithEmptyPassword) {
  std::string savedExtra = "some extra data";
  alice.exportWallet(BOB_WALLET_PATH, false, WalletSaveLevel::SAVE_ALL, savedExtra);

  WalletGreen bob(dispatcher, currency, node, logger, TRANSACTION_SOFTLOCK_TIME);
  std::string loadedExtra;
  ASSERT_NO_THROW(bob.load(BOB_WALLET_PATH, "", loadedExtra));
  ASSERT_EQ(savedExtra, loadedExtra);

  bob.shutdown();
  wait(100);
}

TEST_F(WalletApi, walletExportFailedIfFileAlreadyExists) {
  std::ofstream file(BOB_WALLET_PATH, std::ios_base::binary);
  file.put('1');
  file.close();
  ASSERT_TRUE(boost::filesystem::exists(BOB_WALLET_PATH));
  ASSERT_EQ(1, boost::filesystem::file_size(BOB_WALLET_PATH));

  try {
    alice.exportWallet(BOB_WALLET_PATH, true, WalletSaveLevel::SAVE_ALL);
    ASSERT_FALSE(true);
  } catch (const std::system_error&) {
    ASSERT_TRUE(boost::filesystem::exists(BOB_WALLET_PATH));
    ASSERT_EQ(1, boost::filesystem::file_size(BOB_WALLET_PATH));
  }
}
