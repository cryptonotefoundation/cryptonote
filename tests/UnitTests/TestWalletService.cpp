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

#include <algorithm>
#include <queue>
#include <system_error>

#include <IWallet.h>

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Currency.h"
#include "Logging/LoggerGroup.h"
#include "Logging/ConsoleLogger.h"
#include <System/Event.h>
#include "PaymentGate/WalletService.h"
#include "PaymentGate/WalletServiceErrorCategory.h"
#include "INodeStubs.h"
#include "Wallet/WalletErrors.h"

using namespace CryptoNote;
using namespace PaymentService;

namespace CryptoNote {

bool operator== (const WalletOrder& lhs, const WalletOrder& rhs) {
  return std::make_tuple(lhs.address, lhs.amount) == std::make_tuple(rhs.address, rhs.amount);
}

bool operator== (const DonationSettings& lhs, const DonationSettings& rhs) {
  return std::make_tuple(lhs.address, lhs.threshold) == std::make_tuple(rhs.address, rhs.threshold);
}

} //namespace CryptoNote

struct IWalletBaseStub : public CryptoNote::IWallet {
  IWalletBaseStub(System::Dispatcher& dispatcher) : m_eventOccurred(dispatcher) {}
  virtual ~IWalletBaseStub() {}

  virtual void initialize(const std::string& path, const std::string& password) override { }
  virtual void initializeWithViewKey(const std::string& path, const std::string& password, const Crypto::SecretKey& viewSecretKey) override { }
  virtual void initializeWithViewKey(const std::string& path, const std::string& password, const Crypto::SecretKey& viewSecretKey, const uint64_t& creationTimestamp) override { }
  virtual void initializeWithViewKey(const std::string& path, const std::string& password, const Crypto::SecretKey& viewSecretKey, const uint32_t scanHeight) override { }
  virtual void load(const std::string& path, const std::string& password, std::string& extra) override { }
  virtual void load(const std::string& path, const std::string& password) override { }
  virtual void shutdown() override { }

  virtual void changePassword(const std::string& oldPassword, const std::string& newPassword) override { }
  virtual void save(WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL, const std::string& extra = "") override { }
  virtual void reset(const uint64_t scanHeight) override { }
  virtual void exportWallet(const std::string& path, bool encrypt = true, WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL, const std::string& extra = "") override { }

  virtual size_t getAddressCount() const override { return 0; }
  virtual std::string getAddress(size_t index) const override { return ""; }
  virtual bool isMyAddress(const std::string& address) const override { return false; }
  virtual AccountPublicAddress getAccountPublicAddress(size_t index) const override { return AccountPublicAddress(); }
  virtual KeyPair getAddressSpendKey(size_t index) const override { return KeyPair(); }
  virtual KeyPair getAddressSpendKey(const std::string& address) const override { return KeyPair(); }
  virtual KeyPair getViewKey() const override { return KeyPair(); }
  virtual AddressGenerationMode getAddressGenerationMode() const override { return AddressGenerationMode::INDEPENDENT_SPEND_KEYS; }
  virtual Crypto::SecretKey getDeterministicSeed() const override { return Crypto::SecretKey(); }
  virtual void setAddressGenerationMode(AddressGenerationMode mode, const Crypto::SecretKey& deterministicSeed) override { }
  virtual std::string createAddress() override { return ""; }
  virtual std::string createAddress(uint32_t scanHeight) override { return ""; }
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey, bool reset) override { return ""; }
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey, bool reset) override { return ""; }
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey, const uint64_t& creationTimestamp) override { return ""; }
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey, const uint64_t& creationTimestamp) override { return ""; }
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey, const uint32_t scanHeight) override { return ""; }
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey, const uint32_t scanHeight) override { return ""; }
  virtual std::vector<std::string> createAddressList(const std::vector<Crypto::SecretKey>& spendSecretKeys, bool reset) override { return std::vector<std::string>(); }
  virtual std::vector<std::string> createAddressList(const std::vector<Crypto::SecretKey>& spendSecretKeys, const std::vector<uint64_t>& creationTimestamps) override { return std::vector<std::string>(); }
  virtual std::vector<std::string> createAddressList(const std::vector<Crypto::SecretKey>& spendSecretKeys, const std::vector<uint32_t>& scanHeights) override { return std::vector<std::string>(); }
  virtual void deleteAddress(const std::string& address) override { }

  virtual uint64_t getActualBalance() const override { return 0; }
  virtual uint64_t getActualBalance(const std::string& address) const override { return 0; }
  virtual uint64_t getPendingBalance() const override { return 0; }
  virtual uint64_t getPendingBalance(const std::string& address) const override { return 0; }

  virtual size_t getTransactionCount() const override { return 0; }
  virtual WalletTransaction getTransaction(size_t transactionIndex) const override { return WalletTransaction(); }
  virtual Crypto::SecretKey getTransactionSecretKey(size_t transactionIndex) const override { return Crypto::SecretKey(); }
  virtual Crypto::SecretKey getTransactionSecretKey(Crypto::Hash& transactionHash) const override { return Crypto::SecretKey(); }
  virtual bool getTransactionProof(const Crypto::Hash& transactionHash, const CryptoNote::AccountPublicAddress& destinationAddress, const Crypto::SecretKey& txKey, std::string& transactionProof) override { return false; }
  virtual size_t getTransactionTransferCount(size_t transactionIndex) const override { return 0; }
  virtual WalletTransfer getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const override { return WalletTransfer(); }

  virtual WalletTransactionWithTransfers getTransaction(const Crypto::Hash& transactionHash) const override { return WalletTransactionWithTransfers(); }
  virtual std::vector<TransactionsInBlockInfo> getTransactions(const Crypto::Hash& blockHash, size_t count) const override { return {}; }
  virtual std::vector<TransactionsInBlockInfo> getTransactions(uint32_t blockIndex, size_t count) const override { return {}; }
  virtual std::vector<Crypto::Hash> getBlockHashes(uint32_t blockIndex, size_t count) const override { return {}; }
  virtual uint32_t getBlockCount() const override { return 0; }
  virtual std::vector<WalletTransactionWithTransfers> getUnconfirmedTransactions() const override { return {}; }
  virtual std::vector<size_t> getDelayedTransactionIds() const override { return {}; }
  virtual std::vector<TransactionOutputInformation> getTransfers(size_t index, uint32_t flags) const override { return {}; }

  virtual std::string getReserveProof(const uint64_t &reserve, const std::string& address, const std::string &message) override { return ""; }
  virtual std::string signMessage(const std::string &message, const std::string& address) override { return ""; }
  virtual bool verifyMessage(const std::string &message, const std::string& address, const std::string &signature) override { return false; }

  virtual size_t transfer(const TransactionParameters& sendingTransaction, Crypto::SecretKey& txSecretKey) override { return 0; }

  virtual size_t makeTransaction(const TransactionParameters& sendingTransaction) override { return 0; }
  virtual void commitTransaction(size_t transactionId) override { }
  virtual void rollbackUncommitedTransaction(size_t transactionId) override { }

  virtual void start() override { m_stopped = false; }
  virtual void stop() override { m_stopped = true; m_eventOccurred.set(); }

  //blocks until an event occurred
  virtual WalletEvent getEvent() override {
    throwIfStopped();

    while(m_events.empty()) {
      m_eventOccurred.wait();
      m_eventOccurred.clear();
      throwIfStopped();
    }

    WalletEvent event = std::move(m_events.front());
    m_events.pop();

    return event;
  }

  void pushEvent(const WalletEvent& event) {
    m_events.push(event);
    m_eventOccurred.set();
  }

protected:
  void throwIfStopped() {
    if (m_stopped) {
      throw std::system_error(make_error_code(std::errc::operation_canceled));
    }
  }

  bool m_stopped = false;
  System::Event m_eventOccurred;
  std::queue<WalletEvent> m_events;
};

class WalletServiceTest: public ::testing::Test {
public:
  WalletServiceTest() :
    logger(Logging::ERROR),
    currency(CryptoNote::CurrencyBuilder(logger).currency()),
    generator(currency),
    nodeStub(generator),
    walletBase(dispatcher)
  {}

  virtual void SetUp() override;
protected:
  Logging::ConsoleLogger logger;
  Currency currency;
  TestBlockchainGenerator generator;
  INodeTrivialRefreshStub nodeStub;
  WalletConfiguration walletConfig;
  System::Dispatcher dispatcher;
  IWalletBaseStub walletBase;

  std::unique_ptr<WalletService> createWalletService(CryptoNote::IWallet& wallet);
  std::unique_ptr<WalletService> createWalletService();
  Crypto::Hash generateRandomHash();
};

void WalletServiceTest::SetUp() {
  logger.setMaxLevel(Logging::DEBUGGING);

  walletConfig.walletFile = "test";
  walletConfig.walletPassword = "test";
}

std::unique_ptr<WalletService> WalletServiceTest::createWalletService(CryptoNote::IWallet& wallet) {
  return std::unique_ptr<WalletService>(new WalletService(currency, dispatcher, nodeStub, wallet, walletConfig, logger));
}

std::unique_ptr<WalletService> WalletServiceTest::createWalletService() {
  return createWalletService(walletBase);
}

Crypto::Hash WalletServiceTest::generateRandomHash() {
  Crypto::Hash hash;
  std::generate(std::begin(hash.data), std::end(hash.data), std::rand);
  return hash;
}

class WalletServiceTest_createAddress : public WalletServiceTest {
};

struct WalletCreateAddressStub: public IWalletBaseStub {
  WalletCreateAddressStub(System::Dispatcher& d) : IWalletBaseStub(d) {}

  virtual std::string createAddress() override { return address; }
  virtual std::string createAddress(uint32_t scanHeight) override { return address; }
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey, bool reset) override { return address; }
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey, bool reset) override { return address; }
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey, const uint32_t scanHeight) override { return address; }
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey, const uint32_t scanHeight) override { return address; }

  std::string address = "correctAddress";
};

TEST_F(WalletServiceTest_createAddress, returnsCorrectAddress) {
  WalletCreateAddressStub wallet(dispatcher);

  std::unique_ptr<WalletService> service = createWalletService(wallet);
  std::string address;
  std::error_code ec = service->createAddress(address);

  ASSERT_FALSE(ec);
  ASSERT_EQ(wallet.address, address);
}

TEST_F(WalletServiceTest_createAddress, invalidSecretKey) {
  std::unique_ptr<WalletService> service = createWalletService();

  std::string address;
  std::error_code ec = service->createAddress("wrong key", true, address);
  ASSERT_EQ(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT), ec);
}

TEST_F(WalletServiceTest_createAddress, invalidPublicKey) {
  std::unique_ptr<WalletService> service = createWalletService();

  std::string address;
  std::error_code ec = service->createTrackingAddress("wrong key", address);
  ASSERT_EQ(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT), ec);
}

TEST_F(WalletServiceTest_createAddress, correctSecretKey) {
  Crypto::PublicKey pub;
  Crypto::SecretKey sec;
  Crypto::generate_keys(pub, sec);

  WalletCreateAddressStub wallet(dispatcher);
  std::unique_ptr<WalletService> service = createWalletService(wallet);

  std::string address;
  std::error_code ec = service->createAddress(Common::podToHex(sec), true, address);

  ASSERT_FALSE(ec);
  ASSERT_EQ(wallet.address, address);
}

TEST_F(WalletServiceTest_createAddress, correctPublicKey) {
  Crypto::PublicKey pub;
  Crypto::SecretKey sec;
  Crypto::generate_keys(pub, sec);

  WalletCreateAddressStub wallet(dispatcher);
  std::unique_ptr<WalletService> service = createWalletService(wallet);

  std::string address;
  std::error_code ec = service->createTrackingAddress(Common::podToHex(pub), address);

  ASSERT_FALSE(ec);
  ASSERT_EQ(wallet.address, address);
}

class WalletServiceTest_getSpendKeys : public WalletServiceTest {
};

struct WalletgetSpendKeysStub: public IWalletBaseStub {
  WalletgetSpendKeysStub(System::Dispatcher& d) : IWalletBaseStub(d) {
    Crypto::generate_keys(keyPair.publicKey, keyPair.secretKey);
  }

  virtual KeyPair getAddressSpendKey(const std::string& address) const override {
    return keyPair;
  }

  KeyPair keyPair;
};

TEST_F(WalletServiceTest_getSpendKeys, returnsKeysCorrectly) {
  WalletgetSpendKeysStub wallet(dispatcher);
  std::unique_ptr<WalletService> service = createWalletService(wallet);

  std::string publicSpendKey;
  std::string secretSpendKey;
  auto ec = service->getSpendkeys("address", publicSpendKey, secretSpendKey);
  ASSERT_FALSE(ec);
  ASSERT_EQ(Common::podToHex(wallet.keyPair.publicKey), publicSpendKey);
  ASSERT_EQ(Common::podToHex(wallet.keyPair.secretKey), secretSpendKey);
}

class WalletServiceTest_getBalance : public WalletServiceTest {
};

struct WalletGetBalanceStub: public IWalletBaseStub {
  WalletGetBalanceStub(System::Dispatcher& d, bool byAddress) : IWalletBaseStub(d), byAddress(byAddress) {
  }

  virtual uint64_t getActualBalance() const override {
    if (byAddress) {
      throw std::runtime_error("wrong overload");
    }

    return actualBalance;
  }

  virtual uint64_t getPendingBalance() const override {
    if (byAddress) {
      throw std::runtime_error("wrong overload");
    }

    return pendingBalance;
  }

  virtual uint64_t getActualBalance(const std::string& address) const override {
    if (!byAddress) {
      throw std::runtime_error("wrong overload");
    }

    return actualBalance;
  }

  virtual uint64_t getPendingBalance(const std::string& address) const override {
    if (!byAddress) {
      throw std::runtime_error("wrong overload");
    }

    return pendingBalance;
  }

  bool byAddress;
  uint64_t actualBalance = 345466;
  uint64_t pendingBalance = 12121;
};

TEST_F(WalletServiceTest_getBalance, returnsCorrectBalance) {
  WalletGetBalanceStub wallet(dispatcher, false);
  std::unique_ptr<WalletService> service = createWalletService(wallet);

  uint64_t actual;
  uint64_t pending;
  auto ec = service->getBalance(actual, pending);

  ASSERT_FALSE(ec);
  ASSERT_EQ(wallet.actualBalance, actual);
  ASSERT_EQ(wallet.pendingBalance, pending);
}

TEST_F(WalletServiceTest_getBalance, returnsCorrectBalanceByAddress) {
  WalletGetBalanceStub wallet(dispatcher, true);
  std::unique_ptr<WalletService> service = createWalletService(wallet);

  uint64_t actual;
  uint64_t pending;
  auto ec = service->getBalance("address", actual, pending);

  ASSERT_FALSE(ec);
  ASSERT_EQ(wallet.actualBalance, actual);
  ASSERT_EQ(wallet.pendingBalance, pending);
}

class WalletServiceTest_getBlockHashes : public WalletServiceTest {
protected:
  std::vector<Crypto::Hash> convertBlockHashes(const std::vector<std::string>& hashes) {
    std::vector<Crypto::Hash> result;
    result.reserve(hashes.size());

    std::for_each(hashes.begin(), hashes.end(), [&result] (const std::string& str) {
      Crypto::Hash hash;
      Common::podFromHex(str, hash);
      result.push_back(hash);
    });

    return result;
  }
};

struct WalletGetBlockHashesStub: public IWalletBaseStub {
  WalletGetBlockHashesStub(System::Dispatcher& d) : IWalletBaseStub(d) {
  }

  virtual std::vector<Crypto::Hash> getBlockHashes(uint32_t blockIndex, size_t count) const override {
    return blockHashes;
  }

  std::vector<Crypto::Hash> blockHashes;
};

TEST_F(WalletServiceTest_getBlockHashes, returnsEmptyBlockHashes) {
  WalletGetBlockHashesStub wallet(dispatcher);
  auto service = createWalletService(wallet);

  std::vector<std::string> blockHashes;
  ASSERT_FALSE(service->getBlockHashes(0, 1, blockHashes));
  ASSERT_EQ(wallet.blockHashes, convertBlockHashes(blockHashes));
}

TEST_F(WalletServiceTest_getBlockHashes, returnsBlockHashes) {
  WalletGetBlockHashesStub wallet(dispatcher);
  std::generate_n(std::back_inserter(wallet.blockHashes), 10, [this] () { return generateRandomHash(); });
  auto service = createWalletService(wallet);

  std::vector<std::string> blockHashes;
  ASSERT_FALSE(service->getBlockHashes(0, 10, blockHashes));
  ASSERT_EQ(wallet.blockHashes, convertBlockHashes(blockHashes));
}

class WalletServiceTest_getViewKey : public WalletServiceTest {
};

struct WalletGetViewKeyStub: public IWalletBaseStub {
  WalletGetViewKeyStub(System::Dispatcher& d) : IWalletBaseStub(d) {
    Crypto::generate_keys(keyPair.publicKey, keyPair.secretKey);
  }

  virtual KeyPair getViewKey() const override {
    return keyPair;
  }

  KeyPair keyPair;
};

TEST_F(WalletServiceTest_getViewKey, returnsCorrectValue) {
  WalletGetViewKeyStub wallet(dispatcher);
  auto service = createWalletService(wallet);

  std::string viewSecretKey;
  ASSERT_FALSE(service->getViewKey(viewSecretKey));
  ASSERT_EQ(Common::podToHex(wallet.keyPair.secretKey), viewSecretKey);
}

class WalletTransactionBuilder {
public:
  WalletTransactionBuilder& hash(const Crypto::Hash& hash) {
    transaction.hash = hash;
    return *this;
  }

  WalletTransactionBuilder& extra(const std::string& extra) {
    transaction.extra = Common::asString(Common::fromHex(extra));
    return *this;
  }

  WalletTransactionBuilder& state(WalletTransactionState state) {
    transaction.state = state;
    return *this;
  }

  WalletTransaction build() const {
    return transaction;
  }

  WalletTransactionBuilder& timestamp(uint64_t t) {
    transaction.timestamp = t;
    return *this;
  }

  WalletTransactionBuilder& blockHeight(uint32_t height) {
    transaction.blockHeight = height;
    return *this;
  }

  WalletTransactionBuilder& totalAmount(int64_t amount) {
    transaction.totalAmount = amount;
    return *this;
  }

  WalletTransactionBuilder& fee(uint64_t fee) {
    transaction.fee = fee;
    return *this;
  }

  WalletTransactionBuilder& creationTime(uint64_t t) {
    transaction.creationTime = t;
    return *this;
  }

  WalletTransactionBuilder& unlockTime(uint64_t unlock) {
    transaction.unlockTime = unlock;
    return *this;
  }

  WalletTransactionBuilder& isBase(bool base) {
    transaction.isBase = base;
    return *this;
  }

private:
  WalletTransaction transaction;
};

class WalletTransactionWithTransfersBuilder {
public:
  WalletTransactionWithTransfersBuilder& transaction(const WalletTransaction& transaction) {
    tx.transaction = transaction;
    return *this;
  }

  WalletTransactionWithTransfersBuilder& addTransfer(const std::string& address, int64_t amount) {
    tx.transfers.push_back(WalletTransfer{WalletTransferType::USUAL, address, amount});
    return *this;
  }

  WalletTransactionWithTransfers build() const {
    return tx;
  }

private:
  WalletTransactionWithTransfers tx;
};

class WalletServiceTest_getTransactions : public WalletServiceTest {
protected:
  virtual void SetUp() override;
  std::vector<TransactionsInBlockInfo> testTransactions;
  std::string RANDOM_ADDRESS1;
  std::string RANDOM_ADDRESS2;
  std::string RANDOM_ADDRESS3;
  const std::string TRANSACTION_EXTRA = "022100dededededededededededededededededededededededededededededededede";
  const std::string PAYMENT_ID = "dededededededededededededededededededededededededededededededede";
};

void WalletServiceTest_getTransactions::SetUp() {
  auto makeAddress = [this]() {
    CryptoNote::AccountBase account;
    account.generate();
    return currency.accountAddressAsString(account);
  };

  RANDOM_ADDRESS1 = makeAddress();
  RANDOM_ADDRESS2 = makeAddress();
  RANDOM_ADDRESS3 = makeAddress();

  TransactionsInBlockInfo block;
  block.blockHash = generateRandomHash();
  block.transactions.push_back(
    WalletTransactionWithTransfersBuilder().addTransfer(RANDOM_ADDRESS1, 222).addTransfer(RANDOM_ADDRESS2, 33333).transaction(
          WalletTransactionBuilder().hash(generateRandomHash()).extra(TRANSACTION_EXTRA).build()
    ).build()
  );

  testTransactions.push_back(block);
}

class WalletGetTransactionsStub : public IWalletBaseStub {
public:
  WalletGetTransactionsStub(System::Dispatcher& d) : IWalletBaseStub(d) {}
  virtual std::vector<TransactionsInBlockInfo> getTransactions(const Crypto::Hash& blockHash, size_t count) const override {
    return transactions;
  }

  virtual std::vector<TransactionsInBlockInfo> getTransactions(uint32_t blockIndex, size_t count) const override {
    return transactions;
  }

  std::vector<TransactionsInBlockInfo> transactions;
};

TEST_F(WalletServiceTest_getTransactions, addressesFilter_emptyReturnsTransaction) {
  WalletGetTransactionsStub wallet(dispatcher);
  wallet.transactions = testTransactions;

  auto service = createWalletService(wallet);

  std::vector<TransactionsInBlockRpcInfo> transactions;
  auto ec = service->getTransactions({}, 0, 1, "", transactions);

  ASSERT_FALSE(ec);

  ASSERT_EQ(1, transactions.size());
  ASSERT_EQ(Common::podToHex(testTransactions[0].transactions[0].transaction.hash), transactions[0].transactions[0].transactionHash);
}

TEST_F(WalletServiceTest_getTransactions, addressesFilter_existentReturnsTransaction) {
  WalletGetTransactionsStub wallet(dispatcher);
  wallet.transactions = testTransactions;

  auto service = createWalletService(wallet);

  std::vector<TransactionsInBlockRpcInfo> transactions;
  auto ec = service->getTransactions({RANDOM_ADDRESS1}, 0, 1, "", transactions);

  ASSERT_FALSE(ec);

  ASSERT_EQ(1, transactions.size());
  ASSERT_EQ(Common::podToHex(testTransactions[0].transactions[0].transaction.hash), transactions[0].transactions[0].transactionHash);
}

TEST_F(WalletServiceTest_getTransactions, addressesFilter_nonExistentReturnsNoTransactions) {
  WalletGetTransactionsStub wallet(dispatcher);
  wallet.transactions = testTransactions;

  auto service = createWalletService(wallet);

  std::vector<TransactionsInBlockRpcInfo> transactions;
  auto ec = service->getTransactions({RANDOM_ADDRESS3}, 0, 1, "", transactions);

  ASSERT_FALSE(ec);

  ASSERT_EQ(1, transactions.size());
  ASSERT_TRUE(transactions[0].transactions.empty());
}

TEST_F(WalletServiceTest_getTransactions, addressesFilter_existentAndNonExistentReturnsTransaction) {
  WalletGetTransactionsStub wallet(dispatcher);
  wallet.transactions = testTransactions;

  auto service = createWalletService(wallet);

  std::vector<TransactionsInBlockRpcInfo> transactions;
  auto ec = service->getTransactions({RANDOM_ADDRESS1, RANDOM_ADDRESS3}, 0, 1, "", transactions);

  ASSERT_FALSE(ec);

  ASSERT_EQ(1, transactions.size());
  ASSERT_EQ(Common::podToHex(testTransactions[0].transactions[0].transaction.hash), transactions[0].transactions[0].transactionHash);
}

TEST_F(WalletServiceTest_getTransactions, paymentIdFilter_existentReturnsTransaction) {
  WalletGetTransactionsStub wallet(dispatcher);
  wallet.transactions = testTransactions;

  auto service = createWalletService(wallet);

  std::vector<TransactionsInBlockRpcInfo> transactions;
  auto ec = service->getTransactions({}, 0, 1, PAYMENT_ID, transactions);

  ASSERT_FALSE(ec);

  ASSERT_EQ(1, transactions.size());
  ASSERT_EQ(Common::podToHex(testTransactions[0].transactions[0].transaction.hash), transactions[0].transactions[0].transactionHash);
  ASSERT_EQ(PAYMENT_ID, transactions[0].transactions[0].paymentId);
}

TEST_F(WalletServiceTest_getTransactions, paymentIdFilter_nonExistentReturnsNoTransaction) {
  WalletGetTransactionsStub wallet(dispatcher);
  wallet.transactions = testTransactions;

  auto service = createWalletService(wallet);

  std::vector<TransactionsInBlockRpcInfo> transactions;
  auto ec = service->getTransactions({}, 0, 1, "dfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdf", transactions);

  ASSERT_FALSE(ec);

  ASSERT_EQ(1, transactions.size());
  ASSERT_TRUE(transactions[0].transactions.empty());
}

TEST_F(WalletServiceTest_getTransactions, invalidAddress) {
  WalletGetTransactionsStub wallet(dispatcher);
  wallet.transactions = testTransactions;

  auto service = createWalletService(wallet);

  std::vector<TransactionsInBlockRpcInfo> transactions;
  auto ec = service->getTransactions({"invalid address"}, 0, 1, "", transactions);
  ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), ec);
}

TEST_F(WalletServiceTest_getTransactions, invalidPaymentId) {
  WalletGetTransactionsStub wallet(dispatcher);
  wallet.transactions = testTransactions;

  auto service = createWalletService(wallet);

  std::vector<TransactionsInBlockRpcInfo> transactions;
  auto ec = service->getTransactions({}, 0, 1, "invalid payment id", transactions);
  ASSERT_EQ(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_PAYMENT_ID_FORMAT), ec);
}

TEST_F(WalletServiceTest_getTransactions, blockNotFound) {
  WalletGetTransactionsStub wallet(dispatcher);
  auto service = createWalletService(wallet);
  std::vector<TransactionsInBlockRpcInfo> transactions;

  auto ec = service->getTransactions({}, 0, 1, "", transactions);
  ASSERT_EQ(make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND), ec);
}

class WalletServiceTest_getTransaction : public WalletServiceTest_getTransactions {
};

struct WalletGetTransactionStub : public IWalletBaseStub {
  WalletGetTransactionStub (System::Dispatcher& dispatcher) : IWalletBaseStub(dispatcher) {
  }

  virtual WalletTransactionWithTransfers getTransaction(const Crypto::Hash& transactionHash) const override {
    return transaction;
  }

  WalletTransactionWithTransfers transaction;
};

TEST_F(WalletServiceTest_getTransaction, wrongHash) {
  auto service = createWalletService();

  TransactionRpcInfo transaction;
  auto ec = service->getTransaction("wrong hash", transaction);
  ASSERT_EQ(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_HASH_FORMAT), ec);
}

TEST_F(WalletServiceTest_getTransaction, returnsCorrectFields) {
  WalletGetTransactionStub wallet(dispatcher);
  wallet.transaction = WalletTransactionWithTransfersBuilder().transaction(
        WalletTransactionBuilder()
        .state(WalletTransactionState::FAILED)
        .hash(generateRandomHash())
        .creationTime(789123)
        .extra(TRANSACTION_EXTRA)
        .fee(293945)
        .isBase(false)
        .timestamp(929293847)
        .totalAmount(-200000)
        .unlockTime(23456)
        .build()
    ).addTransfer("address1", 231).addTransfer("address2", 883).build();

  auto service = createWalletService(wallet);

  TransactionRpcInfo transaction;
  auto ec = service->getTransaction(Common::podToHex(Crypto::Hash()), transaction);

  ASSERT_FALSE(ec);
  ASSERT_EQ(static_cast<uint8_t>(wallet.transaction.transaction.state), transaction.state);
  ASSERT_EQ(wallet.transaction.transaction.blockHeight, transaction.blockIndex);
  ASSERT_EQ(Common::toHex(Common::asBinaryArray(wallet.transaction.transaction.extra)), transaction.extra);
  ASSERT_EQ(PAYMENT_ID, transaction.paymentId);
  ASSERT_EQ(wallet.transaction.transaction.fee, transaction.fee);
  ASSERT_EQ(wallet.transaction.transaction.isBase, transaction.isBase);
  ASSERT_EQ(wallet.transaction.transaction.timestamp, transaction.timestamp);
  ASSERT_EQ(Common::podToHex(wallet.transaction.transaction.hash), transaction.transactionHash);
  ASSERT_EQ(wallet.transaction.transaction.unlockTime, transaction.unlockTime);

  ASSERT_EQ(wallet.transaction.transfers.size(), transaction.transfers.size());

  ASSERT_EQ(wallet.transaction.transfers[0].address, transaction.transfers[0].address);
  ASSERT_EQ(wallet.transaction.transfers[0].amount, transaction.transfers[0].amount);

  ASSERT_EQ(wallet.transaction.transfers[1].address, transaction.transfers[1].address);
  ASSERT_EQ(wallet.transaction.transfers[1].amount, transaction.transfers[1].amount);

}

struct WalletGetTransactionThrowStub : public IWalletBaseStub {
  WalletGetTransactionThrowStub (System::Dispatcher& dispatcher) : IWalletBaseStub(dispatcher) {
  }

  virtual WalletTransactionWithTransfers getTransaction(const Crypto::Hash& transactionHash) const override {
    throw std::system_error(make_error_code(error::OBJECT_NOT_FOUND));
  }
};

TEST_F(WalletServiceTest_getTransaction, transactionNotFound) {
  WalletGetTransactionThrowStub wallet(dispatcher);
  auto service = createWalletService(wallet);

  TransactionRpcInfo transaction;
  auto ec = service->getTransaction(Common::podToHex(Crypto::Hash()), transaction);

  ASSERT_EQ(make_error_code(error::OBJECT_NOT_FOUND), ec);
}

class WalletServiceTest_sendTransaction : public WalletServiceTest_getTransactions {
  virtual void SetUp() override;
protected:
  SendTransaction::Request request;
};

void WalletServiceTest_sendTransaction::SetUp() {
  WalletServiceTest_getTransactions::SetUp();
  request.sourceAddresses.insert(request.sourceAddresses.end(), {RANDOM_ADDRESS1, RANDOM_ADDRESS2});
  request.transfers.push_back(WalletRpcOrder {RANDOM_ADDRESS3, 11111});
  request.fee = 2021;
  request.anonymity = currency.minMixin();
  request.unlockTime = 848309;
}

struct WalletTransferStub : public IWalletBaseStub {
  WalletTransferStub(System::Dispatcher& dispatcher, const Crypto::Hash& hash) : IWalletBaseStub(dispatcher), hash(hash) {
  }

  virtual bool isMyAddress(const std::string& address) const override {
    return true;
  }

  virtual size_t transfer(const TransactionParameters& sendingTransaction, Crypto::SecretKey& txSecretKey) override {
    params = sendingTransaction;
    return 0;
  }

  virtual WalletTransaction getTransaction(size_t transactionIndex) const override {
    return WalletTransactionBuilder().hash(hash).build();
  }

  Crypto::Hash hash;
  TransactionParameters params;
};

bool isEquivalent(const SendTransaction::Request& request, const TransactionParameters& params) {
  std::string extra;
  if (!request.paymentId.empty()) {
    extra = "022100" + request.paymentId;
  } else {
    extra = request.extra;
  }

  std::vector<WalletOrder> orders;
  std::for_each(request.transfers.begin(), request.transfers.end(), [&orders] (const WalletRpcOrder& order) {
    orders.push_back( WalletOrder{order.address, order.amount});
  });

  return std::make_tuple(request.sourceAddresses, orders, request.fee, request.anonymity, extra, request.unlockTime)
      ==
      std::make_tuple(params.sourceAddresses, params.destinations, params.fee, params.mixIn, Common::toHex(Common::asBinaryArray(params.extra)), params.unlockTimestamp);
}

TEST_F(WalletServiceTest_sendTransaction, passesCorrectParameters) {
  WalletTransferStub wallet(dispatcher, generateRandomHash());
  auto service = createWalletService(wallet);

  std::string hash;
  std::string txSecretKey;
  auto ec = service->sendTransaction(request, hash, txSecretKey);

  ASSERT_FALSE(ec);
  ASSERT_EQ(Common::podToHex(wallet.hash), hash);
  ASSERT_TRUE(isEquivalent(request, wallet.params));
}

TEST_F(WalletServiceTest_sendTransaction, incorrectSourceAddress) {
  auto service = createWalletService();
  request.sourceAddresses.push_back("wrong address");

  std::string hash;
  std::string txSecretKey;
  auto ec = service->sendTransaction(request, hash, txSecretKey);
  ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), ec);
}

TEST_F(WalletServiceTest_sendTransaction, incorrectTransferAddress) {
  auto service = createWalletService();
  request.transfers.push_back(WalletRpcOrder{"wrong address", 12131});

  std::string hash;
  std::string txSecretKey;
  auto ec = service->sendTransaction(request, hash, txSecretKey);
  ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), ec);
}

class WalletServiceTest_createDelayedTransaction : public WalletServiceTest_getTransactions {
  virtual void SetUp() override;
protected:
  CreateDelayedTransaction::Request request;
};

void WalletServiceTest_createDelayedTransaction::SetUp() {
  WalletServiceTest_getTransactions::SetUp();
  request.addresses.insert(request.addresses.end(), {RANDOM_ADDRESS1, RANDOM_ADDRESS2});
  request.transfers.push_back(WalletRpcOrder {RANDOM_ADDRESS3, 11111});
  request.fee = 2021;
  request.anonymity = currency.minMixin();
  request.unlockTime = 848309;
}

struct WalletMakeTransactionStub : public IWalletBaseStub {
  WalletMakeTransactionStub(System::Dispatcher& dispatcher, const Crypto::Hash& hash) : IWalletBaseStub(dispatcher), hash(hash) {
  }

  virtual bool isMyAddress(const std::string& address) const override {
    return true;
  }

  virtual size_t makeTransaction(const TransactionParameters& sendingTransaction) override {
    params = sendingTransaction;
    return 0;
  }

  virtual WalletTransaction getTransaction(size_t transactionIndex) const override {
    return WalletTransactionBuilder().hash(hash).build();
  }

  Crypto::Hash hash;
  TransactionParameters params;
};

bool isEquivalent(const CreateDelayedTransaction::Request& request, const TransactionParameters& params) {
  std::string extra;
  if (!request.paymentId.empty()) {
    extra = "022100" + request.paymentId;
  } else {
    extra = request.extra;
  }

  std::vector<WalletOrder> orders;
  std::for_each(request.transfers.begin(), request.transfers.end(), [&orders] (const WalletRpcOrder& order) {
    orders.push_back( WalletOrder{order.address, order.amount});
  });

  return std::make_tuple(request.addresses, orders, request.fee, request.anonymity, extra, request.unlockTime)
      ==
      std::make_tuple(params.sourceAddresses, params.destinations, params.fee, params.mixIn, Common::toHex(Common::asBinaryArray(params.extra)), params.unlockTimestamp);
}

TEST_F(WalletServiceTest_createDelayedTransaction, passesCorrectParameters) {
  WalletMakeTransactionStub wallet(dispatcher, generateRandomHash());
  auto service = createWalletService(wallet);

  std::string hash;
  auto ec = service->createDelayedTransaction(request, hash);

  ASSERT_FALSE(ec);
  ASSERT_EQ(Common::podToHex(wallet.hash), hash);
  ASSERT_TRUE(isEquivalent(request, wallet.params));
}

TEST_F(WalletServiceTest_createDelayedTransaction, incorrectSourceAddress) {
  auto service = createWalletService();
  request.addresses.push_back("wrong address");

  std::string hash;
  auto ec = service->createDelayedTransaction(request, hash);
  ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), ec);
}

TEST_F(WalletServiceTest_createDelayedTransaction, incorrectTransferAddress) {
  auto service = createWalletService();
  request.transfers.push_back(WalletRpcOrder{"wrong address", 12131});

  std::string hash;
  auto ec = service->createDelayedTransaction(request, hash);
  ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), ec);
}

class WalletServiceTest_getDelayedTransactionHashes: public WalletServiceTest {
};

struct WalletGetDelayedTransactionIdsStub : public IWalletBaseStub {
  WalletGetDelayedTransactionIdsStub(System::Dispatcher& dispatcher, const Crypto::Hash& hash) : IWalletBaseStub(dispatcher), hash(hash) {
  }

  virtual std::vector<size_t> getDelayedTransactionIds() const override {
    return {0};
  }

  virtual WalletTransaction getTransaction(size_t transactionIndex) const override {
    return WalletTransactionBuilder().hash(hash).build();
  }

  const Crypto::Hash hash;
};

TEST_F(WalletServiceTest_getDelayedTransactionHashes, returnsCorrectResult) {
  WalletGetDelayedTransactionIdsStub wallet(dispatcher, generateRandomHash());
  auto service = createWalletService(wallet);

  std::vector<std::string> hashes;
  auto ec = service->getDelayedTransactionHashes(hashes);

  ASSERT_FALSE(ec);
  ASSERT_EQ(1, hashes.size());
  ASSERT_EQ(Common::podToHex(wallet.hash), hashes[0]);
}

class WalletServiceTest_getUnconfirmedTransactionHashes: public WalletServiceTest_getTransactions {
public:
  virtual void SetUp() override;
protected:
  std::vector<CryptoNote::WalletTransactionWithTransfers> transactions;
};

void WalletServiceTest_getUnconfirmedTransactionHashes::SetUp() {
  WalletServiceTest_getTransactions::SetUp();
  transactions = { WalletTransactionWithTransfersBuilder().transaction(
                    WalletTransactionBuilder().hash(generateRandomHash()).build()
                   ).addTransfer(RANDOM_ADDRESS1, 100).addTransfer(RANDOM_ADDRESS2, 333).build()
                  ,
                   WalletTransactionWithTransfersBuilder().transaction(
                     WalletTransactionBuilder().hash(generateRandomHash()).build()
                   ).addTransfer(RANDOM_ADDRESS3, 123).addTransfer(RANDOM_ADDRESS2, 4252).build()
  };
}

struct WalletGetUnconfirmedTransactionsStub : public IWalletBaseStub {
  WalletGetUnconfirmedTransactionsStub(System::Dispatcher& dispatcher) : IWalletBaseStub(dispatcher) {
  }

  virtual bool isMyAddress(const std::string& address) const override {
    return true;
  }

  virtual std::vector<WalletTransactionWithTransfers> getUnconfirmedTransactions() const override {
    return transactions;
  }

  std::vector<CryptoNote::WalletTransactionWithTransfers> transactions;
};

TEST_F(WalletServiceTest_getUnconfirmedTransactionHashes, returnsAllHashesWithoutAddresses) {
  WalletGetUnconfirmedTransactionsStub wallet(dispatcher);
  wallet.transactions = transactions;
  auto service = createWalletService(wallet);

  std::vector<std::string> hashes;
  auto ec = service->getUnconfirmedTransactionHashes({}, hashes);

  ASSERT_FALSE(ec);
  ASSERT_EQ(2, hashes.size());
  ASSERT_EQ(hashes[0], Common::podToHex(transactions[0].transaction.hash));
  ASSERT_EQ(hashes[1], Common::podToHex(transactions[1].transaction.hash));
}

TEST_F(WalletServiceTest_getUnconfirmedTransactionHashes, returnsOneTransactionWithAddressFilter) {
  WalletGetUnconfirmedTransactionsStub wallet(dispatcher);
  wallet.transactions = transactions;
  auto service = createWalletService(wallet);

  std::vector<std::string> hashes;
  auto ec = service->getUnconfirmedTransactionHashes({RANDOM_ADDRESS1}, hashes);

  ASSERT_FALSE(ec);
  ASSERT_EQ(1, hashes.size());
  ASSERT_EQ(hashes[0], Common::podToHex(transactions[0].transaction.hash));
}

TEST_F(WalletServiceTest_getUnconfirmedTransactionHashes, returnsTwoTransactionsWithAddressFilter) {
  WalletGetUnconfirmedTransactionsStub wallet(dispatcher);
  wallet.transactions = transactions;
  auto service = createWalletService(wallet);

  std::vector<std::string> hashes;
  auto ec = service->getUnconfirmedTransactionHashes({RANDOM_ADDRESS2}, hashes);

  ASSERT_FALSE(ec);
  ASSERT_EQ(2, hashes.size());
  ASSERT_EQ(hashes[0], Common::podToHex(transactions[0].transaction.hash));
  ASSERT_EQ(hashes[1], Common::podToHex(transactions[1].transaction.hash));
}

TEST_F(WalletServiceTest_getUnconfirmedTransactionHashes, wrongAddressFilter) {
  auto service = createWalletService();

  std::vector<std::string> hashes;
  auto ec = service->getUnconfirmedTransactionHashes({"wrong address"}, hashes);

  ASSERT_EQ(make_error_code(CryptoNote::error::BAD_ADDRESS), ec);
}

