// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2016, The Monero Project
// Copyright (c) 2016-2020, Karbo developers
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

#include <istream>
#include <ostream>
#include <limits>
#include <string>
#include <list>
#include <system_error>
#include <boost/optional.hpp>
#include "CryptoNote.h"
#include "CryptoTypes.h"
#include "CryptoNote.h"
#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "ITransfersContainer.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

namespace CryptoNote {

typedef size_t TransactionId;
typedef size_t TransferId;

struct WalletLegacyTransfer {
  std::string address;
  int64_t amount;
};

const TransactionId WALLET_LEGACY_INVALID_TRANSACTION_ID    = std::numeric_limits<TransactionId>::max();
const TransferId WALLET_LEGACY_INVALID_TRANSFER_ID          = std::numeric_limits<TransferId>::max();
const uint32_t WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT = std::numeric_limits<uint32_t>::max();

enum class WalletLegacyTransactionState : uint8_t {
  Active,    // --> {Deleted}
  Deleted,   // --> {Active}

  Sending,   // --> {Active, Cancelled, Failed}
  Cancelled, // --> {}
  Failed     // --> {}
};

struct WalletLegacyTransaction {
  TransferId       firstTransferId;
  size_t           transferCount;
  int64_t          totalAmount;
  uint64_t         fee;
  uint64_t         sentTime;
  uint64_t         unlockTime;
  Crypto::Hash     hash;
  boost::optional<Crypto::SecretKey> secretKey = CryptoNote::NULL_SECRET_KEY;
  bool             isCoinbase;
  uint32_t         blockHeight;
  uint64_t         timestamp;
  std::string      extra;
  WalletLegacyTransactionState state;
};

using PaymentId = Crypto::Hash;
struct Payments {
  PaymentId paymentId;
  std::vector<WalletLegacyTransaction> transactions;
};

static_assert(std::is_move_constructible<Payments>::value, "Payments is not move constructible");

class IWalletLegacyObserver {
public:
  virtual ~IWalletLegacyObserver() {}

  virtual void initCompleted(std::error_code result) {}
  virtual void saveCompleted(std::error_code result) {}
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) {}
  virtual void synchronizationCompleted(std::error_code result) {}
  virtual void actualBalanceUpdated(uint64_t actualBalance) {}
  virtual void pendingBalanceUpdated(uint64_t pendingBalance) {}
  virtual void unmixableBalanceUpdated(uint64_t unmixableBalance) {}
  virtual void externalTransactionCreated(TransactionId transactionId) {}
  virtual void sendTransactionCompleted(TransactionId transactionId, std::error_code result) {}
  virtual void transactionUpdated(TransactionId transactionId) {}
};

class IWalletLegacy {
public:
  virtual ~IWalletLegacy() {} ;
  virtual void addObserver(IWalletLegacyObserver* observer) = 0;
  virtual void removeObserver(IWalletLegacyObserver* observer) = 0;

  virtual void initAndGenerate(const std::string& password) = 0;
  virtual void initAndGenerateDeterministic(const std::string& password) = 0;
  virtual void initAndLoad(std::istream& source, const std::string& password) = 0;
  virtual void initWithKeys(const AccountKeys& accountKeys, const std::string& password) = 0;
  virtual void initWithKeys(const AccountKeys& accountKeys, const std::string& password, const uint32_t scanHeight) = 0;
  virtual void shutdown() = 0;
  virtual void reset() = 0;
  virtual bool tryLoadWallet(std::istream& source, const std::string& password) = 0;

  virtual void save(std::ostream& destination, bool saveDetailed = true, bool saveCache = true) = 0;

  virtual std::error_code changePassword(const std::string& oldPassword, const std::string& newPassword) = 0;

  virtual std::string getAddress() = 0;

  virtual uint64_t actualBalance() = 0;
  virtual uint64_t pendingBalance() = 0;
  virtual uint64_t unmixableBalance() = 0;

  virtual size_t getTransactionCount() = 0;
  virtual size_t getTransferCount() = 0;
  virtual size_t getUnlockedOutputsCount() = 0;

  virtual TransactionId findTransactionByTransferId(TransferId transferId) = 0;
  
  virtual bool getTransaction(TransactionId transactionId, WalletLegacyTransaction& transaction) = 0;
  virtual bool getTransfer(TransferId transferId, WalletLegacyTransfer& transfer) = 0;
  virtual std::vector<Payments> getTransactionsByPaymentIds(const std::vector<PaymentId>& paymentIds) const = 0;
  virtual bool getTxProof(Crypto::Hash& txid, CryptoNote::AccountPublicAddress& address, Crypto::SecretKey& tx_key, std::string& sig_str) = 0;
  virtual std::string getReserveProof(const uint64_t &reserve, const std::string &message) = 0;
  virtual Crypto::SecretKey getTxKey(Crypto::Hash& txid) = 0;
  virtual bool get_tx_key(Crypto::Hash& txid, Crypto::SecretKey& txSecretKey) = 0;
  virtual void getAccountKeys(AccountKeys& keys) = 0;
  virtual bool getSeed(std::string& electrum_words) = 0;

  virtual std::vector<TransactionOutputInformation> getOutputs() = 0;
  virtual std::vector<TransactionOutputInformation> getLockedOutputs() = 0;
  virtual std::vector<TransactionOutputInformation> getUnlockedOutputs() = 0;
  virtual std::vector<TransactionSpentOutputInformation> getSpentOutputs() = 0;

  virtual TransactionId sendTransaction(const WalletLegacyTransfer& transfer, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) = 0;
  virtual TransactionId sendTransaction(const std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) = 0;
  virtual TransactionId sendTransaction(const std::vector<WalletLegacyTransfer>& transfers, const std::list<TransactionOutputInformation>& selectedOuts, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) = 0;
  virtual TransactionId sendFusionTransaction(const std::list<TransactionOutputInformation>& fusionInputs, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) = 0;
  virtual std::string prepareRawTransaction(TransactionId& transactionId, const std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) = 0;
  virtual std::string prepareRawTransaction(TransactionId& transactionId, const std::vector<WalletLegacyTransfer>& transfers, const std::list<TransactionOutputInformation>& selectedOuts, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) = 0;
  virtual std::string prepareRawTransaction(TransactionId& transactionId, const WalletLegacyTransfer& transfer, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) = 0;
  virtual std::error_code cancelTransaction(size_t transferId) = 0;

  virtual size_t estimateFusion(const uint64_t& threshold) = 0;
  virtual std::list<TransactionOutputInformation> selectFusionTransfersToSend(uint64_t threshold, size_t minInputCount, size_t maxInputCount) = 0;

  virtual bool getTransactionInformation(const Crypto::Hash& transactionHash, TransactionInformation& info,
      uint64_t* amountIn = nullptr, uint64_t* amountOut = nullptr) const = 0;
  virtual std::vector<TransactionOutputInformation> getTransactionOutputs(const Crypto::Hash& transactionHash, uint32_t flags = ITransfersContainer::IncludeDefault) const = 0;
  virtual std::vector<TransactionOutputInformation> getTransactionInputs(const Crypto::Hash& transactionHash, uint32_t flags) const = 0;
  virtual bool isFusionTransaction(const WalletLegacyTransaction& walletTx) const = 0;

  virtual std::string sign_message(const std::string &message) = 0;
  virtual bool verify_message(const std::string &message, const CryptoNote::AccountPublicAddress &address, const std::string &signature) = 0;

  virtual bool isTrackingWallet() = 0;
};

}
