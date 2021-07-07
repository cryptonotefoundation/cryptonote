// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, Karbo developers
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

#pragma once

#include <limits>
#include <string>
#include <vector>
#include <boost/optional.hpp>
#include "CryptoNote.h"
#include "ITransfersContainer.h"

namespace CryptoNote {

const size_t WALLET_INVALID_TRANSACTION_ID = std::numeric_limits<size_t>::max();
const size_t WALLET_INVALID_TRANSFER_ID = std::numeric_limits<size_t>::max();
const uint32_t WALLET_UNCONFIRMED_TRANSACTION_HEIGHT = std::numeric_limits<uint32_t>::max();

enum class WalletTransactionState : uint8_t {
  SUCCEEDED = 0,
  FAILED,
  CANCELLED,
  CREATED,
  DELETED
};

enum WalletEventType {
  TRANSACTION_CREATED,
  TRANSACTION_UPDATED,
  BALANCE_UNLOCKED,
  SYNC_PROGRESS_UPDATED,
  SYNC_COMPLETED,
};

enum class WalletSaveLevel : uint8_t {
  SAVE_KEYS_ONLY,
  SAVE_KEYS_AND_TRANSACTIONS,
  SAVE_ALL
};

struct WalletTransactionCreatedData {
  size_t transactionIndex;
};

struct WalletTransactionUpdatedData {
  size_t transactionIndex;
};

struct WalletSynchronizationProgressUpdated {
  uint32_t processedBlockCount;
  uint32_t totalBlockCount;
};

struct WalletEvent {
  WalletEventType type;
  union {
    WalletTransactionCreatedData transactionCreated;
    WalletTransactionUpdatedData transactionUpdated;
    WalletSynchronizationProgressUpdated synchronizationProgressUpdated;
  };
};

struct WalletTransaction {
  WalletTransactionState state;
  uint64_t timestamp;
  uint32_t blockHeight;
  Crypto::Hash hash;
  boost::optional<Crypto::SecretKey> secretKey;
  int64_t totalAmount;
  uint64_t fee;
  uint64_t creationTime;
  uint64_t unlockTime;
  std::string extra;
  bool isBase;
};

enum class WalletTransferType : uint8_t {
  USUAL = 0,
  DONATION,
  CHANGE
};

struct WalletOrder {
  std::string address;
  uint64_t amount;
};

struct WalletTransfer {
  WalletTransferType type;
  std::string address;
  int64_t amount;
};

struct DonationSettings {
  std::string address;
  uint64_t threshold = 0;
};

struct TransactionParameters {
  std::vector<std::string> sourceAddresses;
  std::vector<WalletOrder> destinations;
  uint64_t fee = 0;
  uint64_t mixIn = 0;
  std::string extra;
  uint64_t unlockTimestamp = 0;
  DonationSettings donation;
  std::string changeDestination;
};

struct WalletTransactionWithTransfers {
  WalletTransaction transaction;
  std::vector<WalletTransfer> transfers;
};

struct TransactionsInBlockInfo {
  Crypto::Hash blockHash;
  std::vector<WalletTransactionWithTransfers> transactions;
};

class IWallet {
public:
  virtual ~IWallet() {}

  virtual void initialize(const std::string& path, const std::string& password) = 0;
  virtual void initializeWithViewKey(const std::string& path, const std::string& password, const Crypto::SecretKey& viewSecretKey) = 0;
  virtual void initializeWithViewKey(const std::string& path, const std::string& password, const Crypto::SecretKey& viewSecretKey, const uint64_t& creationTimestamp) = 0;
  virtual void initializeWithViewKey(const std::string& path, const std::string& password, const Crypto::SecretKey& viewSecretKey, const uint32_t scanHeight) = 0;
  virtual void load(const std::string& path, const std::string& password, std::string& extra) = 0;
  virtual void load(const std::string& path, const std::string& password) = 0;
  virtual void shutdown() = 0;

  virtual void changePassword(const std::string& oldPassword, const std::string& newPassword) = 0;
  virtual void save(WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL, const std::string& extra = "") = 0;
  virtual void reset(const uint64_t scanHeight) = 0;
  virtual void exportWallet(const std::string& path, bool encrypt = true, WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL, const std::string& extra = "") = 0;

  virtual size_t getAddressCount() const = 0;
  virtual std::string getAddress(size_t index) const = 0;
  virtual AccountPublicAddress getAccountPublicAddress(size_t index) const = 0;
  virtual KeyPair getAddressSpendKey(size_t index) const = 0;
  virtual KeyPair getAddressSpendKey(const std::string& address) const = 0;
  virtual KeyPair getViewKey() const = 0;
  virtual std::string createAddress() = 0;
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey, bool reset = true) = 0;
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey, bool reset = true) = 0;
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey, const uint64_t& creationTimestamp) = 0;
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey, const uint64_t& creationTimestamp) = 0;
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey, const uint32_t scanHeight) = 0;
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey, const uint32_t scanHeight) = 0;
  virtual std::vector<std::string> createAddressList(const std::vector<Crypto::SecretKey>& spendSecretKeys, bool reset = true) = 0;
  virtual std::vector<std::string> createAddressList(const std::vector<Crypto::SecretKey>& spendSecretKeys, const std::vector<uint64_t>& creationTimestamps) = 0;
  virtual std::vector<std::string> createAddressList(const std::vector<Crypto::SecretKey>& spendSecretKeys, const std::vector<uint32_t>& scanHeights) = 0;
  virtual void deleteAddress(const std::string& address) = 0;

  virtual uint64_t getActualBalance() const = 0;
  virtual uint64_t getActualBalance(const std::string& address) const = 0;
  virtual uint64_t getPendingBalance() const = 0;
  virtual uint64_t getPendingBalance(const std::string& address) const = 0;

  virtual size_t getTransactionCount() const = 0;
  virtual WalletTransaction getTransaction(size_t transactionIndex) const = 0;
  virtual Crypto::SecretKey getTransactionSecretKey(size_t transactionIndex) const = 0;
  virtual Crypto::SecretKey getTransactionSecretKey(Crypto::Hash& transactionHash) const = 0;
  virtual bool getTransactionProof(const Crypto::Hash& transactionHash, const CryptoNote::AccountPublicAddress& destinationAddress, const Crypto::SecretKey& txKey, std::string& transactionProof) = 0;
  virtual size_t getTransactionTransferCount(size_t transactionIndex) const = 0;
  virtual WalletTransfer getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const = 0;

  virtual WalletTransactionWithTransfers getTransaction(const Crypto::Hash& transactionHash) const = 0;
  virtual std::vector<TransactionsInBlockInfo> getTransactions(const Crypto::Hash& blockHash, size_t count) const = 0;
  virtual std::vector<TransactionsInBlockInfo> getTransactions(uint32_t blockIndex, size_t count) const = 0;
  virtual std::vector<Crypto::Hash> getBlockHashes(uint32_t blockIndex, size_t count) const = 0;
  virtual uint32_t getBlockCount() const  = 0;
  virtual std::vector<WalletTransactionWithTransfers> getUnconfirmedTransactions() const = 0;
  virtual std::vector<size_t> getDelayedTransactionIds() const = 0;
  virtual std::vector<TransactionOutputInformation> getTransfers(size_t index, uint32_t flags) const = 0;

  virtual std::string getReserveProof(const uint64_t &reserve, const std::string& address, const std::string &message) = 0;

  virtual std::string signMessage(const std::string &message, const std::string& address) = 0;
  virtual bool verifyMessage(const std::string &message, const std::string& address, const std::string &signature) = 0;

  virtual size_t transfer(const TransactionParameters& sendingTransaction, Crypto::SecretKey &txSecretKey) = 0;

  virtual size_t makeTransaction(const TransactionParameters& sendingTransaction) = 0;
  virtual void commitTransaction(size_t transactionId) = 0;
  virtual void rollbackUncommitedTransaction(size_t transactionId) = 0;

  virtual void start() = 0;
  virtual void stop() = 0;

  //blocks until an event occurred
  virtual WalletEvent getEvent() = 0;
};

}
