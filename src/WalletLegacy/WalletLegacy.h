// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2016, The Monero Project
// Copyright (c) 2016-2020, Karbo developers
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <list>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "CryptoNote.h"
#include "IWalletLegacy.h"
#include "INode.h"
#include "Wallet/WalletErrors.h"
#include "Wallet/WalletAsyncContextCounter.h"
#include "Common/ObserverManager.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "WalletLegacy/WalletUnconfirmedTransactions.h"

#include "WalletLegacy/WalletTransactionSender.h"
#include "WalletLegacy/WalletRequest.h"

#include "Transfers/BlockchainSynchronizer.h"
#include "Transfers/TransfersSynchronizer.h"

#include <Logging/LoggerRef.h>

namespace CryptoNote {

class SyncStarter;

class WalletLegacy :
  public IWalletLegacy,
  IBlockchainSynchronizerObserver,
  ITransfersObserver {

public:
  WalletLegacy(const CryptoNote::Currency& currency, INode& node, Logging::ILogger& log);
  virtual ~WalletLegacy();

  virtual void addObserver(IWalletLegacyObserver* observer) override;
  virtual void removeObserver(IWalletLegacyObserver* observer) override;

  virtual void initAndGenerate(const std::string& password) override;
  virtual void initAndGenerateDeterministic(const std::string& password) override;
  virtual void initAndLoad(std::istream& source, const std::string& password) override;
  virtual void initWithKeys(const AccountKeys& accountKeys, const std::string& password) override;
  virtual void initWithKeys(const AccountKeys& accountKeys, const std::string& password, const uint32_t scanHeight) override;
  virtual void shutdown() override;
  virtual void reset() override;
  virtual bool tryLoadWallet(std::istream& source, const std::string& password) override;

  virtual void save(std::ostream& destination, bool saveDetailed = true, bool saveCache = true) override;

  virtual std::error_code changePassword(const std::string& oldPassword, const std::string& newPassword) override;

  virtual std::string getAddress() override;

  virtual uint64_t actualBalance() override;
  virtual uint64_t pendingBalance() override;
  virtual uint64_t unmixableBalance() override;

  virtual size_t getTransactionCount() override;
  virtual size_t getTransferCount() override;
  virtual size_t getUnlockedOutputsCount() override;

  virtual TransactionId findTransactionByTransferId(TransferId transferId) override;

  virtual bool getTransaction(TransactionId transactionId, WalletLegacyTransaction& transaction) override;
  virtual bool getTransfer(TransferId transferId, WalletLegacyTransfer& transfer) override;
  virtual std::vector<Payments> getTransactionsByPaymentIds(const std::vector<PaymentId>& paymentIds) const override;
  virtual bool getTxProof(Crypto::Hash& txid, CryptoNote::AccountPublicAddress& address, Crypto::SecretKey& tx_key, std::string& sig_str) override;
  virtual std::string getReserveProof(const uint64_t &reserve, const std::string &message) override;
  virtual Crypto::SecretKey getTxKey(Crypto::Hash& txid) override;
  virtual bool get_tx_key(Crypto::Hash& txid, Crypto::SecretKey& txSecretKey) override;
  virtual void getAccountKeys(AccountKeys& keys) override;
  virtual bool getSeed(std::string& electrum_words) override;

  virtual std::vector<TransactionOutputInformation> getOutputs() override;
  virtual std::vector<TransactionOutputInformation> getLockedOutputs() override;
  virtual std::vector<TransactionOutputInformation> getUnlockedOutputs() override;
  virtual std::vector<TransactionSpentOutputInformation> getSpentOutputs() override;

  virtual TransactionId sendTransaction(const WalletLegacyTransfer& transfer, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) override;
  virtual TransactionId sendTransaction(const std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) override;
  virtual TransactionId sendTransaction(const std::vector<WalletLegacyTransfer>& transfers, const std::list<TransactionOutputInformation>& selectedOuts, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) override;
  virtual TransactionId sendFusionTransaction(const std::list<TransactionOutputInformation>& fusionInputs, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) override;
  virtual std::string prepareRawTransaction(TransactionId& transactionId, const std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) override;
  virtual std::string prepareRawTransaction(TransactionId& transactionId, const std::vector<WalletLegacyTransfer>& transfers, const std::list<TransactionOutputInformation>& selectedOuts, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) override;
  virtual std::string prepareRawTransaction(TransactionId& transactionId, const WalletLegacyTransfer& transfer, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) override;
  virtual std::error_code cancelTransaction(size_t transactionId) override;

  virtual size_t estimateFusion(const uint64_t& threshold) override;
  virtual std::list<TransactionOutputInformation> selectFusionTransfersToSend(uint64_t threshold, size_t minInputCount, size_t maxInputCount) override;

  virtual bool getTransactionInformation(const Crypto::Hash& transactionHash, TransactionInformation& info,
      uint64_t* amountIn = nullptr, uint64_t* amountOut = nullptr) const override;
  virtual std::vector<TransactionOutputInformation> getTransactionOutputs(const Crypto::Hash& transactionHash, uint32_t flags = ITransfersContainer::IncludeDefault) const override;
  virtual std::vector<TransactionOutputInformation> getTransactionInputs(const Crypto::Hash& transactionHash, uint32_t flags) const override;
  virtual bool isFusionTransaction(const WalletLegacyTransaction& walletTx) const override;

  virtual std::string sign_message(const std::string &message) override;
  virtual bool verify_message(const std::string &message, const CryptoNote::AccountPublicAddress &address, const std::string &signature) override;

  virtual bool isTrackingWallet() override;

private:

  // IBlockchainSynchronizerObserver
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override;
  virtual void synchronizationCompleted(std::error_code result) override;

  // ITransfersObserver
  virtual void onTransactionUpdated(ITransfersSubscription* object, const Crypto::Hash& transactionHash) override;
  virtual void onTransactionDeleted(ITransfersSubscription* object, const Crypto::Hash& transactionHash) override;

  void initSync();
  void throwIfNotInitialised();

  void doSave(std::ostream& destination, bool saveDetailed, bool saveCache);
  void doLoad(std::istream& source);

  void synchronizationCallback(WalletRequest::Callback callback, std::error_code ec);
  void sendTransactionCallback(WalletRequest::Callback callback, std::error_code ec);
  void notifyClients(std::deque<std::shared_ptr<WalletLegacyEvent> >& events);
  void notifyIfBalanceChanged();

  std::vector<TransactionId> deleteOutdatedUnconfirmedTransactions();

  uint64_t scanHeightToTimestamp(const uint32_t scanHeight);
  uint64_t getBlockTimestamp(const uint32_t blockHeight);

  enum WalletState
  {
    NOT_INITIALIZED = 0,
    INITIALIZED,
    LOADING,
    SAVING
  };

  WalletState m_state;
  std::mutex m_cacheMutex;
  CryptoNote::AccountBase m_account;
  std::string m_password;
  const CryptoNote::Currency& m_currency;
  INode& m_node;
  Logging::LoggerRef m_logger;
  bool m_isStopping;

  std::atomic<uint64_t> m_lastNotifiedActualBalance;
  std::atomic<uint64_t> m_lastNotifiedPendingBalance;
  std::atomic<uint64_t> m_lastNotifiedUnmixableBalance;

  BlockchainSynchronizer m_blockchainSync;
  TransfersSyncronizer m_transfersSync;
  ITransfersContainer* m_transferDetails;

  WalletUserTransactionsCache m_transactionsCache;
  std::unique_ptr<WalletTransactionSender> m_sender;

  WalletAsyncContextCounter m_asyncContextCounter;
  Tools::ObserverManager<CryptoNote::IWalletLegacyObserver> m_observerManager;

  std::unique_ptr<SyncStarter> m_onInitSyncStarter;
};

} //namespace CryptoNote
