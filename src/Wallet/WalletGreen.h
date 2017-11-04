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

#pragma once

#include "IWallet.h"

#include <queue>
#include <unordered_map>

#include "IFusionManager.h"
#include "WalletIndices.h"

#include "Logging/LoggerRef.h"
#include <System/Dispatcher.h>
#include <System/Event.h>
#include "Transfers/TransfersSynchronizer.h"
#include "Transfers/BlockchainSynchronizer.h"

namespace CryptoNote {

class WalletGreen : public IWallet,
                    ITransfersObserver,
                    IBlockchainSynchronizerObserver,
                    ITransfersSynchronizerObserver,
                    public IFusionManager {
public:
  WalletGreen(System::Dispatcher& dispatcher, const Currency& currency, INode& node, Logging::ILogger& logger, uint32_t transactionSoftLockTime = 1);
  virtual ~WalletGreen();

  virtual void initialize(const std::string& path, const std::string& password) override;
  virtual void initializeWithViewKey(const std::string& path, const std::string& password, const Crypto::SecretKey& viewSecretKey) override;
  virtual void load(const std::string& path, const std::string& password, std::string& extra) override;
  virtual void load(const std::string& path, const std::string& password) override;
  virtual void shutdown() override;

  virtual void changePassword(const std::string& oldPassword, const std::string& newPassword) override;
  virtual void save(WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL, const std::string& extra = "") override;
  virtual void exportWallet(const std::string& path, bool encrypt = true, WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL, const std::string& extra = "") override;

  virtual size_t getAddressCount() const override;
  virtual std::string getAddress(size_t index) const override;
  virtual KeyPair getAddressSpendKey(size_t index) const override;
  virtual KeyPair getAddressSpendKey(const std::string& address) const override;
  virtual KeyPair getViewKey() const override;
  virtual std::string createAddress() override;
  virtual std::string createAddress(const Crypto::SecretKey& spendSecretKey) override;
  virtual std::string createAddress(const Crypto::PublicKey& spendPublicKey) override;
  virtual std::vector<std::string> createAddressList(const std::vector<Crypto::SecretKey>& spendSecretKeys) override;
  virtual void deleteAddress(const std::string& address) override;

  virtual uint64_t getActualBalance() const override;
  virtual uint64_t getActualBalance(const std::string& address) const override;
  virtual uint64_t getPendingBalance() const override;
  virtual uint64_t getPendingBalance(const std::string& address) const override;

  virtual size_t getTransactionCount() const override;
  virtual WalletTransaction getTransaction(size_t transactionIndex) const override;
  virtual size_t getTransactionTransferCount(size_t transactionIndex) const override;
  virtual WalletTransfer getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const override;

  virtual WalletTransactionWithTransfers getTransaction(const Crypto::Hash& transactionHash) const override;
  virtual std::vector<TransactionsInBlockInfo> getTransactions(const Crypto::Hash& blockHash, size_t count) const override;
  virtual std::vector<TransactionsInBlockInfo> getTransactions(uint32_t blockIndex, size_t count) const override;
  virtual std::vector<Crypto::Hash> getBlockHashes(uint32_t blockIndex, size_t count) const override;
  virtual uint32_t getBlockCount() const override;
  virtual std::vector<WalletTransactionWithTransfers> getUnconfirmedTransactions() const override;
  virtual std::vector<size_t> getDelayedTransactionIds() const override;

  virtual size_t transfer(const TransactionParameters& sendingTransaction) override;

  virtual size_t makeTransaction(const TransactionParameters& sendingTransaction) override;
  virtual void commitTransaction(size_t) override;
  virtual void rollbackUncommitedTransaction(size_t) override;

  virtual void start() override;
  virtual void stop() override;
  virtual WalletEvent getEvent() override;

  virtual size_t createFusionTransaction(uint64_t threshold, uint16_t mixin,
    const std::vector<std::string>& sourceAddresses = {}, const std::string& destinationAddress = "") override;
  virtual bool isFusionTransaction(size_t transactionId) const override;
  virtual IFusionManager::EstimateResult estimate(uint64_t threshold, const std::vector<std::string>& sourceAddresses = {}) const override;

protected:
  struct NewAddressData {
    Crypto::PublicKey spendPublicKey;
    Crypto::SecretKey spendSecretKey;
    uint64_t creationTimestamp;
  };

  void throwIfNotInitialized() const;
  void throwIfStopped() const;
  void throwIfTrackingMode() const;
  void doShutdown();
  void clearCaches(bool clearTransactions, bool clearCachedData);
  void convertAndLoadWalletFile(const std::string& path, std::ifstream&& walletFileStream);
  static void decryptKeyPair(const EncryptedWalletRecord& cipher, Crypto::PublicKey& publicKey, Crypto::SecretKey& secretKey,
    uint64_t& creationTimestamp, const Crypto::chacha8_key& key);
  void decryptKeyPair(const EncryptedWalletRecord& cipher, Crypto::PublicKey& publicKey, Crypto::SecretKey& secretKey, uint64_t& creationTimestamp) const;
  static EncryptedWalletRecord encryptKeyPair(const Crypto::PublicKey& publicKey, const Crypto::SecretKey& secretKey, uint64_t creationTimestamp,
    const Crypto::chacha8_key& key, const Crypto::chacha8_iv& iv);
  EncryptedWalletRecord encryptKeyPair(const Crypto::PublicKey& publicKey, const Crypto::SecretKey& secretKey, uint64_t creationTimestamp) const;
  Crypto::chacha8_iv getNextIv() const;
  static void incIv(Crypto::chacha8_iv& iv);
  void incNextIv();
  void initWithKeys(const std::string& path, const std::string& password, const Crypto::PublicKey& viewPublicKey, const Crypto::SecretKey& viewSecretKey);
  std::string doCreateAddress(const Crypto::PublicKey& spendPublicKey, const Crypto::SecretKey& spendSecretKey, uint64_t creationTimestamp);
  std::vector<std::string> doCreateAddressList(const std::vector<NewAddressData>& addressDataList);

  struct InputInfo {
    TransactionTypes::InputKeyInfo keyInfo;
    WalletRecord* walletRecord = nullptr;
    KeyPair ephKeys;
  };

  struct OutputToTransfer {
    TransactionOutputInformation out;
    WalletRecord* wallet;
  };

  struct ReceiverAmounts {
    CryptoNote::AccountPublicAddress receiver;
    std::vector<uint64_t> amounts;
  };

  struct WalletOuts {
    WalletRecord* wallet;
    std::vector<TransactionOutputInformation> outs;
  };

  typedef std::pair<WalletTransfers::const_iterator, WalletTransfers::const_iterator> TransfersRange;

  struct AddressAmounts {
    int64_t input = 0;
    int64_t output = 0;
  };

  struct ContainerAmounts {
    ITransfersContainer* container;
    AddressAmounts amounts;
  };

#pragma pack(push, 1)
  struct ContainerStoragePrefix {
    uint8_t version;
    Crypto::chacha8_iv nextIv;
    EncryptedWalletRecord encryptedViewKeys;
  };
#pragma pack(pop)

  typedef std::unordered_map<std::string, AddressAmounts> TransfersMap;

  virtual void onError(ITransfersSubscription* object, uint32_t height, std::error_code ec) override;

  virtual void onTransactionUpdated(ITransfersSubscription* object, const Crypto::Hash& transactionHash) override;
  virtual void onTransactionUpdated(const Crypto::PublicKey& viewPublicKey, const Crypto::Hash& transactionHash,
    const std::vector<ITransfersContainer*>& containers) override;
  void transactionUpdated(const TransactionInformation& transactionInfo, const std::vector<ContainerAmounts>& containerAmountsList);

  virtual void onTransactionDeleted(ITransfersSubscription* object, const Crypto::Hash& transactionHash) override;
  void transactionDeleted(ITransfersSubscription* object, const Crypto::Hash& transactionHash);

  virtual void synchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount) override;
  virtual void synchronizationCompleted(std::error_code result) override;

  void onSynchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount);
  void onSynchronizationCompleted();

  virtual void onBlocksAdded(const Crypto::PublicKey& viewPublicKey, const std::vector<Crypto::Hash>& blockHashes) override;
  void blocksAdded(const std::vector<Crypto::Hash>& blockHashes);

  virtual void onBlockchainDetach(const Crypto::PublicKey& viewPublicKey, uint32_t blockIndex) override;
  void blocksRollback(uint32_t blockIndex);

  virtual void onTransactionDeleteBegin(const Crypto::PublicKey& viewPublicKey, Crypto::Hash transactionHash) override;
  void transactionDeleteBegin(Crypto::Hash transactionHash);

  virtual void onTransactionDeleteEnd(const Crypto::PublicKey& viewPublicKey, Crypto::Hash transactionHash) override;
  void transactionDeleteEnd(Crypto::Hash transactionHash);

  std::vector<WalletOuts> pickWalletsWithMoney() const;
  WalletOuts pickWallet(const std::string& address) const;
  std::vector<WalletOuts> pickWallets(const std::vector<std::string>& addresses) const;

  void updateBalance(CryptoNote::ITransfersContainer* container);
  void unlockBalances(uint32_t height);

  const WalletRecord& getWalletRecord(const Crypto::PublicKey& key) const;
  const WalletRecord& getWalletRecord(const std::string& address) const;
  const WalletRecord& getWalletRecord(CryptoNote::ITransfersContainer* container) const;

  CryptoNote::AccountPublicAddress parseAddress(const std::string& address) const;
  std::string addWallet(const Crypto::PublicKey& spendPublicKey, const Crypto::SecretKey& spendSecretKey, uint64_t creationTimestamp);
  AccountKeys makeAccountKeys(const WalletRecord& wallet) const;
  size_t getTransactionId(const Crypto::Hash& transactionHash) const;
  void pushEvent(const WalletEvent& event);
  bool isFusionTransaction(const WalletTransaction& walletTx) const;

  struct PreparedTransaction {
    std::unique_ptr<ITransaction> transaction;
    std::vector<WalletTransfer> destinations;
    uint64_t neededMoney;
    uint64_t changeAmount;
  };

  void prepareTransaction(std::vector<WalletOuts>&& wallets,
    const std::vector<WalletOrder>& orders,
    uint64_t fee,
    uint16_t mixIn,
    const std::string& extra,
    uint64_t unlockTimestamp,
    const DonationSettings& donation,
    const CryptoNote::AccountPublicAddress& changeDestinationAddress,
    PreparedTransaction& preparedTransaction);

  size_t doTransfer(const TransactionParameters& transactionParameters);

  void checkIfEnoughMixins(std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult, uint16_t mixIn) const;
  std::vector<WalletTransfer> convertOrdersToTransfers(const std::vector<WalletOrder>& orders) const;
  uint64_t countNeededMoney(const std::vector<CryptoNote::WalletTransfer>& destinations, uint64_t fee) const;
  CryptoNote::AccountPublicAddress parseAccountAddressString(const std::string& addressString) const;
  uint64_t pushDonationTransferIfPossible(const DonationSettings& donation, uint64_t freeAmount, uint64_t dustThreshold, std::vector<WalletTransfer>& destinations) const;
  void validateAddresses(const std::vector<std::string>& addresses) const;
  void validateOrders(const std::vector<WalletOrder>& orders) const;
  void validateChangeDestination(const std::vector<std::string>& sourceAddresses, const std::string& changeDestination, bool isFusion) const;
  void validateSourceAddresses(const std::vector<std::string>& sourceAddresses) const;
  void validateTransactionParameters(const TransactionParameters& transactionParameters) const;

  void requestMixinOuts(const std::vector<OutputToTransfer>& selectedTransfers,
    uint16_t mixIn,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult);

  void prepareInputs(const std::vector<OutputToTransfer>& selectedTransfers,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult,
    uint16_t mixIn,
    std::vector<InputInfo>& keysInfo);

  uint64_t selectTransfers(uint64_t needeMoney,
    bool dust,
    uint64_t dustThreshold,
    std::vector<WalletOuts>&& wallets,
    std::vector<OutputToTransfer>& selectedTransfers);

  std::vector<ReceiverAmounts> splitDestinations(const std::vector<WalletTransfer>& destinations,
    uint64_t dustThreshold, const Currency& currency);
  ReceiverAmounts splitAmount(uint64_t amount, const AccountPublicAddress& destination, uint64_t dustThreshold);

  std::unique_ptr<CryptoNote::ITransaction> makeTransaction(const std::vector<ReceiverAmounts>& decomposedOutputs,
    std::vector<InputInfo>& keysInfo, const std::string& extra, uint64_t unlockTimestamp);

  void sendTransaction(const CryptoNote::Transaction& cryptoNoteTransaction);
  size_t validateSaveAndSendTransaction(const ITransactionReader& transaction, const std::vector<WalletTransfer>& destinations, bool isFusion, bool send);

  size_t insertBlockchainTransaction(const TransactionInformation& info, int64_t txBalance);
  size_t insertOutgoingTransactionAndPushEvent(const Crypto::Hash& transactionHash, uint64_t fee, const BinaryArray& extra, uint64_t unlockTimestamp);
  void updateTransactionStateAndPushEvent(size_t transactionId, WalletTransactionState state);
  bool updateWalletTransactionInfo(size_t transactionId, const CryptoNote::TransactionInformation& info, int64_t totalAmount);
  bool updateTransactionTransfers(size_t transactionId, const std::vector<ContainerAmounts>& containerAmountsList,
    int64_t allInputsAmount, int64_t allOutputsAmount);
  TransfersMap getKnownTransfersMap(size_t transactionId, size_t firstTransferIdx) const;
  bool updateAddressTransfers(size_t transactionId, size_t firstTransferIdx, const std::string& address, int64_t knownAmount, int64_t targetAmount);
  bool updateUnknownTransfers(size_t transactionId, size_t firstTransferIdx, const std::unordered_set<std::string>& myAddresses,
    int64_t knownAmount, int64_t myAmount, int64_t totalAmount, bool isOutput);
  void appendTransfer(size_t transactionId, size_t firstTransferIdx, const std::string& address, int64_t amount);
  bool adjustTransfer(size_t transactionId, size_t firstTransferIdx, const std::string& address, int64_t amount);
  bool eraseTransfers(size_t transactionId, size_t firstTransferIdx, std::function<bool(bool, const std::string&)>&& predicate);
  bool eraseTransfersByAddress(size_t transactionId, size_t firstTransferIdx, const std::string& address, bool eraseOutputTransfers);
  bool eraseForeignTransfers(size_t transactionId, size_t firstTransferIdx, const std::unordered_set<std::string>& knownAddresses, bool eraseOutputTransfers);
  void pushBackOutgoingTransfers(size_t txId, const std::vector<WalletTransfer>& destinations);
  void insertUnlockTransactionJob(const Crypto::Hash& transactionHash, uint32_t blockHeight, CryptoNote::ITransfersContainer* container);
  void deleteUnlockTransactionJob(const Crypto::Hash& transactionHash);
  void startBlockchainSynchronizer();
  void stopBlockchainSynchronizer();
  void addUnconfirmedTransaction(const ITransactionReader& transaction);
  void removeUnconfirmedTransaction(const Crypto::Hash& transactionHash);

  void copyContainerStorageKeys(ContainerStorage& src, const Crypto::chacha8_key& srcKey, ContainerStorage& dst, const Crypto::chacha8_key& dstKey);
  static void copyContainerStoragePrefix(ContainerStorage& src, const Crypto::chacha8_key& srcKey, ContainerStorage& dst, const Crypto::chacha8_key& dstKey);
  void deleteOrphanTransactions(const std::unordered_set<Crypto::PublicKey>& deletedKeys);
  static void encryptAndSaveContainerData(ContainerStorage& storage, const Crypto::chacha8_key& key, const void* containerData, size_t containerDataSize);
  static void loadAndDecryptContainerData(ContainerStorage& storage, const Crypto::chacha8_key& key, BinaryArray& containerData);
  void initTransactionPool();
  void loadSpendKeys();
  void loadContainerStorage(const std::string& path);
  void loadWalletCache(std::unordered_set<Crypto::PublicKey>& addedKeys, std::unordered_set<Crypto::PublicKey>& deletedKeys, std::string& extra);
  void saveWalletCache(ContainerStorage& storage, const Crypto::chacha8_key& key, WalletSaveLevel saveLevel, const std::string& extra);
  void subscribeWallets();

  std::vector<OutputToTransfer> pickRandomFusionInputs(const std::vector<std::string>& addresses,
    uint64_t threshold, size_t minInputCount, size_t maxInputCount);
  static ReceiverAmounts decomposeFusionOutputs(const AccountPublicAddress& address, uint64_t inputsAmount);

  enum class WalletState {
    INITIALIZED,
    NOT_INITIALIZED
  };

  enum class WalletTrackingMode {
    TRACKING,
    NOT_TRACKING,
    NO_ADDRESSES
  };

  WalletTrackingMode getTrackingMode() const;

  TransfersRange getTransactionTransfersRange(size_t transactionIndex) const;
  std::vector<TransactionsInBlockInfo> getTransactionsInBlocks(uint32_t blockIndex, size_t count) const;
  Crypto::Hash getBlockHashByIndex(uint32_t blockIndex) const;

  std::vector<WalletTransfer> getTransactionTransfers(const WalletTransaction& transaction) const;
  void filterOutTransactions(WalletTransactions& transactions, WalletTransfers& transfers, std::function<bool (const WalletTransaction&)>&& pred) const;
  void initBlockchain(const Crypto::PublicKey& viewPublicKey);
  CryptoNote::AccountPublicAddress getChangeDestination(const std::string& changeDestinationAddress, const std::vector<std::string>& sourceAddresses) const;
  bool isMyAddress(const std::string& address) const;

  void deleteContainerFromUnlockTransactionJobs(const ITransfersContainer* container);
  std::vector<size_t> deleteTransfersForAddress(const std::string& address, std::vector<size_t>& deletedTransactions);
  void deleteFromUncommitedTransactions(const std::vector<size_t>& deletedTransactions);

  System::Dispatcher& m_dispatcher;
  const Currency& m_currency;
  INode& m_node;
  mutable Logging::LoggerRef m_logger;
  bool m_stopped;

  WalletsContainer m_walletsContainer;
  ContainerStorage m_containerStorage;
  UnlockTransactionJobs m_unlockTransactionsJob;
  WalletTransactions m_transactions;
  WalletTransfers m_transfers; //sorted
  mutable std::unordered_map<size_t, bool> m_fusionTxsCache; // txIndex -> isFusion
  UncommitedTransactions m_uncommitedTransactions;

  bool m_blockchainSynchronizerStarted;
  BlockchainSynchronizer m_blockchainSynchronizer;
  TransfersSyncronizer m_synchronizer;

  System::Event m_eventOccurred;
  std::queue<WalletEvent> m_events;
  mutable System::Event m_readyEvent;

  WalletState m_state;

  std::string m_password;
  Crypto::chacha8_key m_key;
  std::string m_path;
  std::string m_extra; // workaround for wallet reset

  Crypto::PublicKey m_viewPublicKey;
  Crypto::SecretKey m_viewSecretKey;

  uint64_t m_actualBalance;
  uint64_t m_pendingBalance;

  uint64_t m_upperTransactionSizeLimit;
  uint32_t m_totalBlockCount;
  uint32_t m_transactionSoftLockTime;

  BlockHashesContainer m_blockchain;

  friend std::ostream& operator<<(std::ostream& os, CryptoNote::WalletGreen::WalletState state);
  friend std::ostream& operator<<(std::ostream& os, CryptoNote::WalletGreen::WalletTrackingMode mode);
  friend class TransferListFormatter;
};

} //namespace CryptoNote
