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

#include "WalletGreen.h"

#include <algorithm>
#include <ctime>
#include <cassert>
#include <fstream>
#include <numeric>
#include <random>
#include <set>
#include <tuple>
#include <utility>

#include <System/EventLock.h>
#include <System/RemoteContext.h>

#include "ITransaction.h"

#include "Common/ScopeExit.h"
#include "Common/ShuffleGenerator.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Common/StreamTools.h"
#include "Common/StringOutputStream.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "crypto/crypto.h"
#include "Transfers/TransfersContainer.h"
#include "WalletSerializationV1.h"
#include "WalletSerializationV2.h"
#include "WalletErrors.h"
#include "WalletUtils.h"

using namespace Common;
using namespace Crypto;
using namespace CryptoNote;
using namespace Logging;

namespace {

void asyncRequestCompletion(System::Event& requestFinished) {
  requestFinished.set();
}

CryptoNote::WalletEvent makeTransactionUpdatedEvent(size_t id) {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::TRANSACTION_UPDATED;
  event.transactionUpdated.transactionIndex = id;

  return event;
}

CryptoNote::WalletEvent makeTransactionCreatedEvent(size_t id) {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::TRANSACTION_CREATED;
  event.transactionCreated.transactionIndex = id;

  return event;
}

CryptoNote::WalletEvent makeMoneyUnlockedEvent() {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::BALANCE_UNLOCKED;

  return event;
}

CryptoNote::WalletEvent makeSyncProgressUpdatedEvent(uint32_t current, uint32_t total) {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::SYNC_PROGRESS_UPDATED;
  event.synchronizationProgressUpdated.processedBlockCount = current;
  event.synchronizationProgressUpdated.totalBlockCount = total;

  return event;
}

CryptoNote::WalletEvent makeSyncCompletedEvent() {
  CryptoNote::WalletEvent event;
  event.type = CryptoNote::WalletEventType::SYNC_COMPLETED;

  return event;
}

size_t getTransactionSize(const ITransactionReader& transaction) {
  return transaction.getTransactionData().size();
}

uint64_t calculateDonationAmount(uint64_t freeAmount, uint64_t donationThreshold, uint64_t dustThreshold) {
  std::vector<uint64_t> decomposedAmounts;
  decomposeAmount(freeAmount, dustThreshold, decomposedAmounts);

  std::sort(decomposedAmounts.begin(), decomposedAmounts.end(), std::greater<uint64_t>());

  uint64_t donationAmount = 0;
  for (auto amount: decomposedAmounts) {
    if (amount > donationThreshold - donationAmount) {
      continue;
    }

    donationAmount += amount;
  }

  assert(donationAmount <= freeAmount);
  return donationAmount;
}

}

namespace CryptoNote {

WalletGreen::WalletGreen(System::Dispatcher& dispatcher, const Currency& currency, INode& node, Logging::ILogger& logger, uint32_t transactionSoftLockTime) :
  m_dispatcher(dispatcher),
  m_currency(currency),
  m_node(node),
  m_logger(logger, "WalletGreen/empty"),
  m_stopped(false),
  m_blockchainSynchronizerStarted(false),
  m_blockchainSynchronizer(node, logger, currency.genesisBlockHash()),
  m_synchronizer(currency, logger, m_blockchainSynchronizer, node),
  m_eventOccurred(m_dispatcher),
  m_readyEvent(m_dispatcher),
  m_state(WalletState::NOT_INITIALIZED),
  m_actualBalance(0),
  m_pendingBalance(0),
  m_transactionSoftLockTime(transactionSoftLockTime)
{
  m_upperTransactionSizeLimit = m_currency.maxTransactionSizeLimit();
  m_readyEvent.set();
}

WalletGreen::~WalletGreen() {
  if (m_state == WalletState::INITIALIZED) {
    doShutdown();
  }

  m_dispatcher.yield(); //let remote spawns finish
}

void WalletGreen::initialize(const std::string& path, const std::string& password) {
  Crypto::PublicKey viewPublicKey;
  Crypto::SecretKey viewSecretKey;
  Crypto::generate_keys(viewPublicKey, viewSecretKey);

  initWithKeys(path, password, viewPublicKey, viewSecretKey);
  m_logger(INFO, BRIGHT_WHITE) << "New container initialized, public view key " << viewPublicKey;
}

void WalletGreen::initializeWithViewKey(const std::string& path, const std::string& password, const Crypto::SecretKey& viewSecretKey) {
  Crypto::PublicKey viewPublicKey;
  if (!Crypto::secret_key_to_public_key(viewSecretKey, viewPublicKey)) {
    m_logger(ERROR, BRIGHT_RED) << "initializeWithViewKey(" << viewSecretKey << ") Failed to convert secret key to public key";
    throw std::system_error(make_error_code(CryptoNote::error::KEY_GENERATION_ERROR));
  }

  initWithKeys(path, password, viewPublicKey, viewSecretKey);
  m_logger(INFO, BRIGHT_WHITE) << "Container initialized with view secret key, public view key " << viewPublicKey;
}

void WalletGreen::shutdown() {
  throwIfNotInitialized();
  doShutdown();

  m_dispatcher.yield(); //let remote spawns finish
  m_logger(INFO, BRIGHT_WHITE) << "Container shut down";
  m_logger = Logging::LoggerRef(m_logger.getLogger(), "WalletGreen/empty");
}

void WalletGreen::doShutdown() {
  if (m_walletsContainer.size() != 0) {
    m_synchronizer.unsubscribeConsumerNotifications(m_viewPublicKey, this);
  }

  stopBlockchainSynchronizer();
  m_blockchainSynchronizer.removeObserver(this);

  m_containerStorage.close();
  m_walletsContainer.clear();
  clearCaches(true, true);

  std::queue<WalletEvent> noEvents;
  std::swap(m_events, noEvents);

  m_state = WalletState::NOT_INITIALIZED;
}

void WalletGreen::clearCaches(bool clearTransactions, bool clearCachedData) {
  if (clearTransactions) {
    m_transactions.clear();
    m_transfers.clear();
  }

  if (clearCachedData) {
    size_t walletIndex = 0;
    for (auto it = m_walletsContainer.begin(); it != m_walletsContainer.end(); ++it) {
      m_walletsContainer.modify(it, [&walletIndex](WalletRecord& wallet) {
        wallet.actualBalance = 0;
        wallet.pendingBalance = 0;
        wallet.container = reinterpret_cast<CryptoNote::ITransfersContainer*>(walletIndex++); //dirty hack. container field must be unique
      });
    }

    if (!clearTransactions) {
      for (auto it = m_transactions.begin(); it != m_transactions.end(); ++it) {
        m_transactions.modify(it, [](WalletTransaction& tx) {
          tx.state = WalletTransactionState::CANCELLED;
          tx.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
        });
      }
    }

    std::vector<AccountPublicAddress> subscriptions;
    m_synchronizer.getSubscriptions(subscriptions);
    std::for_each(subscriptions.begin(), subscriptions.end(), [this](const AccountPublicAddress& address) { m_synchronizer.removeSubscription(address); });

    m_uncommitedTransactions.clear();
    m_unlockTransactionsJob.clear();
    m_actualBalance = 0;
    m_pendingBalance = 0;
    m_fusionTxsCache.clear();
    m_blockchain.clear();
  }
}

void WalletGreen::decryptKeyPair(const EncryptedWalletRecord& cipher, PublicKey& publicKey, SecretKey& secretKey,
  uint64_t& creationTimestamp, const Crypto::chacha8_key& key) {

  std::array<char, sizeof(cipher.data)> buffer;
  chacha8(cipher.data, sizeof(cipher.data), key, cipher.iv, buffer.data());

  MemoryInputStream stream(buffer.data(), buffer.size());
  BinaryInputStreamSerializer serializer(stream);

  serializer(publicKey, "publicKey");
  serializer(secretKey, "secretKey");
  serializer.binary(&creationTimestamp, sizeof(uint64_t), "creationTimestamp");
}

void WalletGreen::decryptKeyPair(const EncryptedWalletRecord& cipher, PublicKey& publicKey, SecretKey& secretKey, uint64_t& creationTimestamp) const {
  decryptKeyPair(cipher, publicKey, secretKey, creationTimestamp, m_key);
}

EncryptedWalletRecord WalletGreen::encryptKeyPair(const PublicKey& publicKey, const SecretKey& secretKey, uint64_t creationTimestamp,
  const Crypto::chacha8_key& key, const Crypto::chacha8_iv& iv) {

  EncryptedWalletRecord result;

  std::string serializedKeys;
  StringOutputStream outputStream(serializedKeys);
  BinaryOutputStreamSerializer serializer(outputStream);

  serializer(const_cast<PublicKey&>(publicKey), "publicKey");
  serializer(const_cast<SecretKey&>(secretKey), "secretKey");
  serializer.binary(&creationTimestamp, sizeof(uint64_t), "creationTimestamp");

  assert(serializedKeys.size() == sizeof(result.data));

  result.iv = iv;
  chacha8(serializedKeys.data(), serializedKeys.size(), key, result.iv, reinterpret_cast<char*>(result.data));

  return result;
}

EncryptedWalletRecord WalletGreen::encryptKeyPair(const PublicKey& publicKey, const SecretKey& secretKey, uint64_t creationTimestamp) const {
  return encryptKeyPair(publicKey, secretKey, creationTimestamp, m_key, getNextIv());
}

Crypto::chacha8_iv WalletGreen::getNextIv() const {
  const auto* prefix = reinterpret_cast<const ContainerStoragePrefix*>(m_containerStorage.prefix());
  return prefix->nextIv;
}

void WalletGreen::incIv(Crypto::chacha8_iv& iv) {
  static_assert(sizeof(uint64_t) == sizeof(Crypto::chacha8_iv), "Bad Crypto::chacha8_iv size");
  uint64_t* i = reinterpret_cast<uint64_t*>(&iv);
  if (*i < std::numeric_limits<uint64_t>::max()) {
    ++(*i);
  } else {
    *i = 0;
  }
}

void WalletGreen::incNextIv() {
  static_assert(sizeof(uint64_t) == sizeof(Crypto::chacha8_iv), "Bad Crypto::chacha8_iv size");
  auto* prefix = reinterpret_cast<ContainerStoragePrefix*>(m_containerStorage.prefix());
  incIv(prefix->nextIv);
}

void WalletGreen::initWithKeys(const std::string& path, const std::string& password,
  const Crypto::PublicKey& viewPublicKey, const Crypto::SecretKey& viewSecretKey) {

  if (m_state != WalletState::NOT_INITIALIZED) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to initialize with keys: already initialized. Current state: " << m_state;
    throw std::system_error(make_error_code(CryptoNote::error::ALREADY_INITIALIZED));
  }

  throwIfStopped();

  ContainerStorage newStorage(path, Common::FileMappedVectorOpenMode::CREATE, sizeof(ContainerStoragePrefix));
  ContainerStoragePrefix* prefix = reinterpret_cast<ContainerStoragePrefix*>(newStorage.prefix());
  prefix->version = static_cast<uint8_t>(WalletSerializerV2::SERIALIZATION_VERSION);
  prefix->nextIv = Crypto::rand<Crypto::chacha8_iv>();

  Crypto::cn_context cnContext;
  Crypto::generate_chacha8_key(cnContext, password, m_key);

  uint64_t creationTimestamp = time(nullptr);
  prefix->encryptedViewKeys = encryptKeyPair(viewPublicKey, viewSecretKey, creationTimestamp, m_key, prefix->nextIv);

  newStorage.flush();
  m_containerStorage.swap(newStorage);
  incNextIv();

  m_viewPublicKey = viewPublicKey;
  m_viewSecretKey = viewSecretKey;
  m_password = password;
  m_path = path;
  m_logger = Logging::LoggerRef(m_logger.getLogger(), "WalletGreen/" + podToHex(m_viewPublicKey).substr(0, 5));

  assert(m_blockchain.empty());
  m_blockchain.push_back(m_currency.genesisBlockHash());

  m_blockchainSynchronizer.addObserver(this);

  m_state = WalletState::INITIALIZED;
}

void WalletGreen::save(WalletSaveLevel saveLevel, const std::string& extra) {
  m_logger(INFO, BRIGHT_WHITE) << "Saving container...";

  throwIfNotInitialized();
  throwIfStopped();

  stopBlockchainSynchronizer();

  try {
    saveWalletCache(m_containerStorage, m_key, saveLevel, extra);
  } catch (const std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to save container: " << e.what();
    startBlockchainSynchronizer();
    throw;
  }

  startBlockchainSynchronizer();
  m_logger(INFO, BRIGHT_WHITE) << "Container saved";
}

void WalletGreen::exportWallet(const std::string& path, bool encrypt, WalletSaveLevel saveLevel, const std::string& extra) {
  m_logger(INFO, BRIGHT_WHITE) << "Exporting container...";

  throwIfNotInitialized();
  throwIfStopped();

  stopBlockchainSynchronizer();

  try {
    bool storageCreated = false;
    Tools::ScopeExit failExitHandler([path, &storageCreated] {
      // Don't delete file if it has existed
      if (storageCreated) {
        boost::system::error_code ignore;
        boost::filesystem::remove(path, ignore);
      }
    });

    ContainerStorage newStorage(path, FileMappedVectorOpenMode::CREATE, m_containerStorage.prefixSize());
    storageCreated = true;

    chacha8_key newStorageKey;
    if (encrypt) {
      newStorageKey = m_key;
    } else {
      cn_context cnContext;
      generate_chacha8_key(cnContext, "", newStorageKey);
    }

    copyContainerStoragePrefix(m_containerStorage, m_key, newStorage, newStorageKey);
    copyContainerStorageKeys(m_containerStorage, m_key, newStorage, newStorageKey);
    saveWalletCache(newStorage, newStorageKey, saveLevel, extra);

    failExitHandler.cancel();

    m_logger(DEBUGGING) << "Container export finished";
  } catch (const std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to export container: " << e.what();
    startBlockchainSynchronizer();
    throw;
  }

  startBlockchainSynchronizer();
  m_logger(INFO, BRIGHT_WHITE) << "Container exported";
}

void WalletGreen::load(const std::string& path, const std::string& password, std::string& extra) {
  m_logger(INFO, BRIGHT_WHITE) << "Loading container...";

  if (m_state != WalletState::NOT_INITIALIZED) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to load: already initialized. Current state: " << m_state;
    throw std::system_error(make_error_code(error::WRONG_STATE));
  }

  throwIfStopped();

  stopBlockchainSynchronizer();

  Crypto::cn_context cnContext;
  generate_chacha8_key(cnContext, password, m_key);

  std::ifstream walletFileStream(path, std::ios_base::binary);
  int version = walletFileStream.peek();
  if (version == EOF) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to read wallet version";
    throw std::system_error(make_error_code(error::WRONG_VERSION), "Failed to read wallet version");
  }

  if (version < WalletSerializerV2::MIN_VERSION) {
    convertAndLoadWalletFile(path, std::move(walletFileStream));
  } else {
    walletFileStream.close();

    if (version > WalletSerializerV2::SERIALIZATION_VERSION) {
      m_logger(ERROR, BRIGHT_RED) << "Unsupported wallet version: " << version;
      throw std::system_error(make_error_code(error::WRONG_VERSION), "Unsupported wallet version");
    }

    loadContainerStorage(path);
    subscribeWallets();

    if (m_containerStorage.suffixSize() > 0) {
      try {
        std::unordered_set<Crypto::PublicKey> addedSpendKeys;
        std::unordered_set<Crypto::PublicKey> deletedSpendKeys;
        loadWalletCache(addedSpendKeys, deletedSpendKeys, extra);

        if (!addedSpendKeys.empty()) {
          m_logger(WARNING, BRIGHT_YELLOW) << "Found addresses not saved in container cache. Resynchronize container";
          clearCaches(false, true);
          subscribeWallets();
        }

        if (!deletedSpendKeys.empty()) {
          m_logger(WARNING, BRIGHT_YELLOW) << "Found deleted addresses saved in container cache. Remove its transactions";
          deleteOrphanTransactions(deletedSpendKeys);
        }

        if (!addedSpendKeys.empty() || !deletedSpendKeys.empty()) {
          saveWalletCache(m_containerStorage, m_key, WalletSaveLevel::SAVE_ALL, extra);
        }
      } catch (const std::exception& e) {
        m_logger(ERROR, BRIGHT_RED) << "Failed to load cache: " << e.what() << ", reset wallet data";
        clearCaches(true, true);
        subscribeWallets();
      }
    }
  }

  m_blockchainSynchronizer.addObserver(this);

  initTransactionPool();

  assert(m_blockchain.empty());
  if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
    m_synchronizer.subscribeConsumerNotifications(m_viewPublicKey, this);
    initBlockchain(m_viewPublicKey);

    startBlockchainSynchronizer();
  } else {
    m_blockchain.push_back(m_currency.genesisBlockHash());
    m_logger(DEBUGGING) << "Add genesis block hash to blockchain";
  }

  m_password = password;
  m_path = path;
  m_extra = extra;

  m_state = WalletState::INITIALIZED;
  m_logger(INFO, BRIGHT_WHITE) << "Container loaded, view public key " << m_viewPublicKey <<
    ", wallet count " << m_walletsContainer.size() <<
    ", actual balance " << m_currency.formatAmount(m_actualBalance) <<
    ", pending balance " << m_currency.formatAmount(m_pendingBalance);
}

void WalletGreen::load(const std::string& path, const std::string& password) {
  std::string extra;
  load(path, password, extra);
}

void WalletGreen::loadContainerStorage(const std::string& path) {
  try {
    m_containerStorage.open(path, FileMappedVectorOpenMode::OPEN, sizeof(ContainerStoragePrefix));

    ContainerStoragePrefix* prefix = reinterpret_cast<ContainerStoragePrefix*>(m_containerStorage.prefix());
    assert(prefix->version >= WalletSerializerV2::MIN_VERSION);

    uint64_t creationTimestamp;
    decryptKeyPair(prefix->encryptedViewKeys, m_viewPublicKey, m_viewSecretKey, creationTimestamp);
    throwIfKeysMismatch(m_viewSecretKey, m_viewPublicKey, "Restored view public key doesn't correspond to secret key");
    m_logger = Logging::LoggerRef(m_logger.getLogger(), "WalletGreen/" + podToHex(m_viewPublicKey).substr(0, 5));

    loadSpendKeys();

    m_logger(DEBUGGING) << "Container keys were successfully loaded";
  } catch (const std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to load container keys: " << e.what();

    m_walletsContainer.clear();
    m_containerStorage.close();

    throw;
  }
}

void WalletGreen::loadWalletCache(std::unordered_set<Crypto::PublicKey>& addedKeys, std::unordered_set<Crypto::PublicKey>& deletedKeys, std::string& extra) {
  assert(m_containerStorage.isOpened());

  BinaryArray contanerData;
  loadAndDecryptContainerData(m_containerStorage, m_key, contanerData);

  WalletSerializerV2 s(
    *this,
    m_viewPublicKey,
    m_viewSecretKey,
    m_actualBalance,
    m_pendingBalance,
    m_walletsContainer,
    m_synchronizer,
    m_unlockTransactionsJob,
    m_transactions,
    m_transfers,
    m_uncommitedTransactions,
    extra,
    m_transactionSoftLockTime
  );

  Common::MemoryInputStream containerStream(contanerData.data(), contanerData.size());
  s.load(containerStream, reinterpret_cast<const ContainerStoragePrefix*>(m_containerStorage.prefix())->version);
  addedKeys = std::move(s.addedKeys());
  deletedKeys = std::move(s.deletedKeys());

  m_logger(DEBUGGING) << "Container cache loaded";
}

void WalletGreen::saveWalletCache(ContainerStorage& storage, const Crypto::chacha8_key& key, WalletSaveLevel saveLevel, const std::string& extra) {
  m_logger(DEBUGGING) << "Saving cache...";

  WalletTransactions transactions;
  WalletTransfers transfers;

  if (saveLevel == WalletSaveLevel::SAVE_KEYS_AND_TRANSACTIONS) {
    filterOutTransactions(transactions, transfers, [](const WalletTransaction& tx) {
      return tx.state == WalletTransactionState::CREATED || tx.state == WalletTransactionState::DELETED;
    });

    for (auto it = transactions.begin(); it != transactions.end(); ++it) {
      transactions.modify(it, [](WalletTransaction& tx) {
        tx.state = WalletTransactionState::CANCELLED;
        tx.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
      });
    }
  } else if (saveLevel == WalletSaveLevel::SAVE_ALL) {
    filterOutTransactions(transactions, transfers, [](const WalletTransaction& tx) {
      return tx.state == WalletTransactionState::DELETED;
    });
  }

  std::string containerData;
  Common::StringOutputStream containerStream(containerData);

  WalletSerializerV2 s(
    *this,
    m_viewPublicKey,
    m_viewSecretKey,
    m_actualBalance,
    m_pendingBalance,
    m_walletsContainer,
    m_synchronizer,
    m_unlockTransactionsJob,
    transactions,
    transfers,
    m_uncommitedTransactions,
    const_cast<std::string&>(extra),
    m_transactionSoftLockTime
  );

  s.save(containerStream, saveLevel);

  encryptAndSaveContainerData(storage, key, containerData.data(), containerData.size());
  storage.flush();

  m_extra = extra;

  m_logger(DEBUGGING) << "Container saving finished";
}

void WalletGreen::copyContainerStorageKeys(ContainerStorage& src, const chacha8_key& srcKey, ContainerStorage& dst, const chacha8_key& dstKey) {
  m_logger(DEBUGGING) << "Copying wallet keys...";
  dst.reserve(src.size());

  dst.setAutoFlush(false);
  Tools::ScopeExit exitHandler([&dst] {
    dst.setAutoFlush(true);
    dst.flush();
  });

  size_t counter = 0;
  for (auto& encryptedSpendKeys : src) {
    Crypto::PublicKey publicKey;
    Crypto::SecretKey secretKey;
    uint64_t creationTimestamp;
    decryptKeyPair(encryptedSpendKeys, publicKey, secretKey, creationTimestamp, srcKey);

    // push_back() can resize container, and dstPrefix address can be changed, so it is requested for each key pair
    ContainerStoragePrefix* dstPrefix = reinterpret_cast<ContainerStoragePrefix*>(dst.prefix());
    Crypto::chacha8_iv keyPairIv = dstPrefix->nextIv;
    incIv(dstPrefix->nextIv);

    dst.push_back(encryptKeyPair(publicKey, secretKey, creationTimestamp, dstKey, keyPairIv));

    ++counter;
    if (counter % 100 == 0) {
      m_logger(DEBUGGING) << "Copied keys: " << counter << " / " << src.size();
    }
  }

  m_logger(DEBUGGING) << "Keys copied";
}

void WalletGreen::copyContainerStoragePrefix(ContainerStorage& src, const chacha8_key& srcKey, ContainerStorage& dst, const chacha8_key& dstKey) {
  ContainerStoragePrefix* srcPrefix = reinterpret_cast<ContainerStoragePrefix*>(src.prefix());
  ContainerStoragePrefix* dstPrefix = reinterpret_cast<ContainerStoragePrefix*>(dst.prefix());
  dstPrefix->version = srcPrefix->version;
  dstPrefix->nextIv = Crypto::rand<chacha8_iv>();

  Crypto::PublicKey publicKey;
  Crypto::SecretKey secretKey;
  uint64_t creationTimestamp;
  decryptKeyPair(srcPrefix->encryptedViewKeys, publicKey, secretKey, creationTimestamp, srcKey);
  dstPrefix->encryptedViewKeys = encryptKeyPair(publicKey, secretKey, creationTimestamp, dstKey, dstPrefix->nextIv);
  incIv(dstPrefix->nextIv);
}

void WalletGreen::encryptAndSaveContainerData(ContainerStorage& storage, const Crypto::chacha8_key& key, const void* containerData, size_t containerDataSize) {
  ContainerStoragePrefix* prefix = reinterpret_cast<ContainerStoragePrefix*>(storage.prefix());

  Crypto::chacha8_iv suffixIv = prefix->nextIv;
  incIv(prefix->nextIv);

  BinaryArray encryptedContainer;
  encryptedContainer.resize(containerDataSize);
  chacha8(containerData, containerDataSize, key, suffixIv, reinterpret_cast<char*>(encryptedContainer.data()));

  std::string suffix;
  Common::StringOutputStream suffixStream(suffix);
  BinaryOutputStreamSerializer suffixSerializer(suffixStream);
  suffixSerializer(suffixIv, "suffixIv");
  suffixSerializer(encryptedContainer, "encryptedContainer");

  storage.resizeSuffix(suffix.size());
  std::copy(suffix.begin(), suffix.end(), storage.suffix());
}

void WalletGreen::loadAndDecryptContainerData(ContainerStorage& storage, const Crypto::chacha8_key& key, BinaryArray& containerData) {
  Common::MemoryInputStream suffixStream(storage.suffix(), storage.suffixSize());
  BinaryInputStreamSerializer suffixSerializer(suffixStream);
  Crypto::chacha8_iv suffixIv;
  BinaryArray encryptedContainer;
  suffixSerializer(suffixIv, "suffixIv");
  suffixSerializer(encryptedContainer, "encryptedContainer");

  containerData.resize(encryptedContainer.size());
  chacha8(encryptedContainer.data(), encryptedContainer.size(), key, suffixIv, reinterpret_cast<char*>(containerData.data()));
}

void WalletGreen::initTransactionPool() {
  std::unordered_set<Crypto::Hash> uncommitedTransactionsSet;
  std::transform(m_uncommitedTransactions.begin(), m_uncommitedTransactions.end(), std::inserter(uncommitedTransactionsSet, uncommitedTransactionsSet.end()),
    [](const UncommitedTransactions::value_type& pair) {
      return getObjectHash(pair.second);
    });
  m_synchronizer.initTransactionPool(uncommitedTransactionsSet);
}

void WalletGreen::deleteOrphanTransactions(const std::unordered_set<Crypto::PublicKey>& deletedKeys) {
  for (auto spendPublicKey : deletedKeys) {
    AccountPublicAddress deletedAccountAddress;
    deletedAccountAddress.spendPublicKey = spendPublicKey;
    deletedAccountAddress.viewPublicKey = m_viewPublicKey;
    auto deletedAddressString = m_currency.accountAddressAsString(deletedAccountAddress);

    std::vector<size_t> deletedTransactions;
    std::vector<size_t> updatedTransactions = deleteTransfersForAddress(deletedAddressString, deletedTransactions);
    deleteFromUncommitedTransactions(deletedTransactions);
  }
}

void WalletGreen::loadSpendKeys() {
  bool isTrackingMode;
  for (size_t i = 0; i < m_containerStorage.size(); ++i) {
    WalletRecord wallet;
    uint64_t creationTimestamp;
    decryptKeyPair(m_containerStorage[i], wallet.spendPublicKey, wallet.spendSecretKey, creationTimestamp);
    wallet.creationTimestamp = creationTimestamp;

    if (i == 0) {
      isTrackingMode = wallet.spendSecretKey == NULL_SECRET_KEY;
    } else if ((isTrackingMode && wallet.spendSecretKey != NULL_SECRET_KEY) || (!isTrackingMode && wallet.spendSecretKey == NULL_SECRET_KEY)) {
      throw std::system_error(make_error_code(error::BAD_ADDRESS), "All addresses must be whether tracking or not");
    }

    if (wallet.spendSecretKey != NULL_SECRET_KEY) {
      throwIfKeysMismatch(wallet.spendSecretKey, wallet.spendPublicKey, "Restored spend public key doesn't correspond to secret key");
    } else {
      if (!Crypto::check_key(wallet.spendPublicKey)) {
        throw std::system_error(make_error_code(error::WRONG_PASSWORD), "Public spend key is incorrect");
      }
    }

    wallet.actualBalance = 0;
    wallet.pendingBalance = 0;
    wallet.container = reinterpret_cast<CryptoNote::ITransfersContainer*>(i); //dirty hack. container field must be unique

    m_walletsContainer.emplace_back(std::move(wallet));
  }
}

void WalletGreen::subscribeWallets() {
  m_logger(DEBUGGING) << "Subscribing wallets...";

  try {
    auto& index = m_walletsContainer.get<RandomAccessIndex>();

    size_t counter = 0;
    for (auto it = index.begin(); it != index.end(); ++it) {
      const auto& wallet = *it;

      AccountSubscription sub;
      sub.keys.address.viewPublicKey = m_viewPublicKey;
      sub.keys.address.spendPublicKey = wallet.spendPublicKey;
      sub.keys.viewSecretKey = m_viewSecretKey;
      sub.keys.spendSecretKey = wallet.spendSecretKey;
      sub.transactionSpendableAge = m_transactionSoftLockTime;
    sub.syncStart.height = m_totalBlockCount;
      sub.syncStart.timestamp = std::max(static_cast<uint64_t>(wallet.creationTimestamp), ACCOUNT_CREATE_TIME_ACCURACY) - ACCOUNT_CREATE_TIME_ACCURACY;

      auto& subscription = m_synchronizer.addSubscription(sub);
      bool r = index.modify(it, [&subscription](WalletRecord& rec) { rec.container = &subscription.getContainer(); });
      assert(r);

      subscription.addObserver(this);

      ++counter;
      if (counter % 100 == 0) {
        m_logger(DEBUGGING) << "Subscribed " << counter << " wallets of " << m_walletsContainer.size();
      }
    }
  } catch (const std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to subscribe wallets: " << e.what();

    std::vector<AccountPublicAddress> subscriptionList;
    m_synchronizer.getSubscriptions(subscriptionList);
    for (auto& subscription : subscriptionList) {
      m_synchronizer.removeSubscription(subscription);
    }

    throw;
  }
}

void WalletGreen::convertAndLoadWalletFile(const std::string& path, std::ifstream&& walletFileStream) {
  WalletSerializerV1 s(
    *this,
    m_viewPublicKey,
    m_viewSecretKey,
    m_actualBalance,
    m_pendingBalance,
    m_walletsContainer,
    m_synchronizer,
    m_unlockTransactionsJob,
    m_transactions,
    m_transfers,
    m_uncommitedTransactions,
    m_transactionSoftLockTime
  );

  StdInputStream stream(walletFileStream);
  s.load(m_key, stream);
  walletFileStream.close();

  boost::filesystem::path bakPath = path + ".backup";
  boost::filesystem::path tmpPath = boost::filesystem::unique_path(path + ".tmp.%%%%-%%%%");

  if (boost::filesystem::exists(bakPath)) {
    throw std::system_error(make_error_code(std::errc::file_exists), ".backup file already exists");
  }

  Tools::ScopeExit tmpFileDeleter([&tmpPath] {
    boost::system::error_code ignore;
    boost::filesystem::remove(tmpPath, ignore);
  });

  m_containerStorage.open(tmpPath.string(), Common::FileMappedVectorOpenMode::CREATE, sizeof(ContainerStoragePrefix));
  ContainerStoragePrefix* prefix = reinterpret_cast<ContainerStoragePrefix*>(m_containerStorage.prefix());
  prefix->version = WalletSerializerV2::SERIALIZATION_VERSION;
  prefix->nextIv = Crypto::rand<Crypto::chacha8_iv>();

  uint64_t creationTimestamp = time(nullptr);
  prefix->encryptedViewKeys = encryptKeyPair(m_viewPublicKey, m_viewSecretKey, creationTimestamp);

  for (auto spendKeys : m_walletsContainer.get<RandomAccessIndex>()) {
    m_containerStorage.push_back(encryptKeyPair(spendKeys.spendPublicKey, spendKeys.spendSecretKey, spendKeys.creationTimestamp));
    incNextIv();
  }

  saveWalletCache(m_containerStorage, m_key, WalletSaveLevel::SAVE_ALL, "");

  boost::filesystem::rename(path, bakPath);
  std::error_code ec;
  m_containerStorage.rename(path, ec);
  if (ec) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to rename " << tmpPath << " to " << path;

    boost::system::error_code ignore;
    boost::filesystem::rename(bakPath, path, ignore);
    throw std::system_error(ec, "Failed to replace wallet file");
  }

  tmpFileDeleter.cancel();
  m_logger(INFO, BRIGHT_WHITE) << "Wallet file converted! Previous version: " << bakPath;
}

void WalletGreen::changePassword(const std::string& oldPassword, const std::string& newPassword) {
  throwIfNotInitialized();
  throwIfStopped();

  if (m_password.compare(oldPassword)) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to change password: the old password is wrong";
    throw std::system_error(make_error_code(error::WRONG_PASSWORD));
  }

  if (oldPassword == newPassword) {
    return;
  }

  Crypto::cn_context cnContext;
  Crypto::chacha8_key newKey;
  Crypto::generate_chacha8_key(cnContext, newPassword, newKey);

  m_containerStorage.atomicUpdate([this, newKey](ContainerStorage& newStorage) {
    copyContainerStoragePrefix(m_containerStorage, m_key, newStorage, newKey);
    copyContainerStorageKeys(m_containerStorage, m_key, newStorage, newKey);

    if (m_containerStorage.suffixSize() > 0) {
      BinaryArray containerData;
      loadAndDecryptContainerData(m_containerStorage, m_key, containerData);
      encryptAndSaveContainerData(newStorage, newKey, containerData.data(), containerData.size());
    }
  });

  m_key = newKey;
  m_password = newPassword;

  m_logger(INFO, BRIGHT_WHITE) << "Container password changed";
}

size_t WalletGreen::getAddressCount() const {
  throwIfNotInitialized();
  throwIfStopped();

  return m_walletsContainer.get<RandomAccessIndex>().size();
}

std::string WalletGreen::getAddress(size_t index) const {
  throwIfNotInitialized();
  throwIfStopped();

  if (index >= m_walletsContainer.get<RandomAccessIndex>().size()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get address: invalid address index " << index;
    throw std::system_error(make_error_code(std::errc::invalid_argument));
  }

  const WalletRecord& wallet = m_walletsContainer.get<RandomAccessIndex>()[index];
  return m_currency.accountAddressAsString({ wallet.spendPublicKey, m_viewPublicKey });
}

KeyPair WalletGreen::getAddressSpendKey(size_t index) const {
  throwIfNotInitialized();
  throwIfStopped();

  if (index >= m_walletsContainer.get<RandomAccessIndex>().size()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get address spend key: invalid address index " << index;
    throw std::system_error(make_error_code(std::errc::invalid_argument));
  }

  const WalletRecord& wallet = m_walletsContainer.get<RandomAccessIndex>()[index];
  return {wallet.spendPublicKey, wallet.spendSecretKey};
}

KeyPair WalletGreen::getAddressSpendKey(const std::string& address) const {
  throwIfNotInitialized();
  throwIfStopped();

  CryptoNote::AccountPublicAddress pubAddr = parseAddress(address);

  auto it = m_walletsContainer.get<KeysIndex>().find(pubAddr.spendPublicKey);
  if (it == m_walletsContainer.get<KeysIndex>().end()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get address spend key: address not found " << address;
    throw std::system_error(make_error_code(error::OBJECT_NOT_FOUND));
  }

  return {it->spendPublicKey, it->spendSecretKey};
}

KeyPair WalletGreen::getViewKey() const {
  throwIfNotInitialized();
  throwIfStopped();

  return {m_viewPublicKey, m_viewSecretKey};
}

std::string WalletGreen::createAddress() {
  KeyPair spendKey;
  Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);
  uint64_t creationTimestamp = static_cast<uint64_t>(time(nullptr));

  return doCreateAddress(spendKey.publicKey, spendKey.secretKey, creationTimestamp);
}

std::string WalletGreen::createAddress(const Crypto::SecretKey& spendSecretKey) {
  Crypto::PublicKey spendPublicKey;
  if (!Crypto::secret_key_to_public_key(spendSecretKey, spendPublicKey)) {
    m_logger(ERROR, BRIGHT_RED) << "createAddress(" << spendSecretKey << ") Failed to convert secret key to public key";
    throw std::system_error(make_error_code(CryptoNote::error::KEY_GENERATION_ERROR));
  }

  uint64_t creationTimestamp = static_cast<uint64_t>(time(nullptr));
  return doCreateAddress(spendPublicKey, spendSecretKey, creationTimestamp);
}

std::string WalletGreen::createAddress(const Crypto::PublicKey& spendPublicKey) {
  if (!Crypto::check_key(spendPublicKey)) {
    m_logger(ERROR, BRIGHT_RED) << "createAddress(" << spendPublicKey << ") Wrong public key format";
    throw std::system_error(make_error_code(error::WRONG_PARAMETERS), "Wrong public key format");
  }

  uint64_t creationTimestamp = static_cast<uint64_t>(time(nullptr));
  return doCreateAddress(spendPublicKey, NULL_SECRET_KEY, creationTimestamp);
}

std::vector<std::string> WalletGreen::createAddressList(const std::vector<Crypto::SecretKey>& spendSecretKeys) {
  std::vector<NewAddressData> addressDataList(spendSecretKeys.size());
  for (size_t i = 0; i < spendSecretKeys.size(); ++i) {
    Crypto::PublicKey spendPublicKey;
    if (!Crypto::secret_key_to_public_key(spendSecretKeys[i], spendPublicKey)) {
      m_logger(ERROR, BRIGHT_RED) << "createAddressList(): failed to convert secret key to public key, secret key " << spendSecretKeys[i];
      throw std::system_error(make_error_code(CryptoNote::error::KEY_GENERATION_ERROR));
    }

    addressDataList[i].spendSecretKey = spendSecretKeys[i];
    addressDataList[i].spendPublicKey = spendPublicKey;
    addressDataList[i].creationTimestamp = 0;
  }

  return doCreateAddressList(addressDataList);
}

std::string WalletGreen::doCreateAddress(const Crypto::PublicKey& spendPublicKey, const Crypto::SecretKey& spendSecretKey, uint64_t creationTimestamp) {
  assert(creationTimestamp <= std::numeric_limits<uint64_t>::max() - m_currency.blockFutureTimeLimit());

  std::vector<NewAddressData> addressDataList;
  addressDataList.push_back(NewAddressData{ spendPublicKey, spendSecretKey, creationTimestamp });
  std::vector<std::string> addresses = doCreateAddressList(addressDataList);
  assert(addresses.size() == 1);

  return addresses.front();
}

std::vector<std::string> WalletGreen::doCreateAddressList(const std::vector<NewAddressData>& addressDataList) {
  throwIfNotInitialized();
  throwIfStopped();

  stopBlockchainSynchronizer();

  std::vector<std::string> addresses;
  try {
    uint64_t minCreationTimestamp = std::numeric_limits<uint64_t>::max();

    {
      if (addressDataList.size() > 1) {
        m_containerStorage.setAutoFlush(false);
      }

      Tools::ScopeExit exitHandler([this] {
        if (!m_containerStorage.getAutoFlush()) {
          m_containerStorage.setAutoFlush(true);
          m_containerStorage.flush();
        }
      });

      for (auto& addressData : addressDataList) {
        assert(addressData.creationTimestamp <= std::numeric_limits<uint64_t>::max() - m_currency.blockFutureTimeLimit());
        std::string address = addWallet(addressData.spendPublicKey, addressData.spendSecretKey, addressData.creationTimestamp);
        m_logger(INFO, BRIGHT_WHITE) << "New wallet added " << address << ", creation timestamp " << addressData.creationTimestamp;
        addresses.push_back(std::move(address));

        minCreationTimestamp = std::min(minCreationTimestamp, addressData.creationTimestamp);
      }
    }

    m_containerStorage.setAutoFlush(true);
    auto currentTime = static_cast<uint64_t>(time(nullptr));
    if (minCreationTimestamp + m_currency.blockFutureTimeLimit() < currentTime) {
      m_logger(DEBUGGING) << "Reset is required";
      save(WalletSaveLevel::SAVE_KEYS_AND_TRANSACTIONS, m_extra);
      shutdown();
      load(m_path, m_password);
    }
  } catch (const std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to add wallets: " << e.what();
    startBlockchainSynchronizer();
    throw;
  }

  startBlockchainSynchronizer();

  return addresses;
}

std::string WalletGreen::addWallet(const Crypto::PublicKey& spendPublicKey, const Crypto::SecretKey& spendSecretKey, uint64_t creationTimestamp) {
  auto& index = m_walletsContainer.get<KeysIndex>();

  auto trackingMode = getTrackingMode();

  if ((trackingMode == WalletTrackingMode::TRACKING && spendSecretKey != NULL_SECRET_KEY) ||
      (trackingMode == WalletTrackingMode::NOT_TRACKING && spendSecretKey == NULL_SECRET_KEY)) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to add wallet: incompatible tracking mode and spend secret key, tracking mode=" << trackingMode <<
      ", spendSecretKey " << (spendSecretKey == NULL_SECRET_KEY ? "is null" : "is not null");
    throw std::system_error(make_error_code(error::WRONG_PARAMETERS));
  }

  auto insertIt = index.find(spendPublicKey);
  if (insertIt != index.end()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to add wallet: address already exists, " <<
      m_currency.accountAddressAsString(AccountPublicAddress{spendPublicKey, m_viewPublicKey});
    throw std::system_error(make_error_code(error::ADDRESS_ALREADY_EXISTS));
  }

  m_containerStorage.push_back(encryptKeyPair(spendPublicKey, spendSecretKey, creationTimestamp));
  incNextIv();

  try {
    AccountSubscription sub;
    sub.keys.address.viewPublicKey = m_viewPublicKey;
    sub.keys.address.spendPublicKey = spendPublicKey;
    sub.keys.viewSecretKey = m_viewSecretKey;
    sub.keys.spendSecretKey = spendSecretKey;
    sub.transactionSpendableAge = m_transactionSoftLockTime;
    sub.syncStart.height = m_totalBlockCount;
    sub.syncStart.timestamp = std::max(creationTimestamp, ACCOUNT_CREATE_TIME_ACCURACY) - ACCOUNT_CREATE_TIME_ACCURACY;

    auto& trSubscription = m_synchronizer.addSubscription(sub);
    ITransfersContainer* container = &trSubscription.getContainer();

    WalletRecord wallet;
    wallet.spendPublicKey = spendPublicKey;
    wallet.spendSecretKey = spendSecretKey;
    wallet.container = container;
    wallet.creationTimestamp = static_cast<time_t>(creationTimestamp);
    trSubscription.addObserver(this);

    index.insert(insertIt, std::move(wallet));
    m_logger(DEBUGGING) << "Wallet count " << m_walletsContainer.size();

    if (index.size() == 1) {
      m_synchronizer.subscribeConsumerNotifications(m_viewPublicKey, this);
      initBlockchain(m_viewPublicKey);
    }

    auto address = m_currency.accountAddressAsString({ spendPublicKey, m_viewPublicKey });
    m_logger(DEBUGGING) << "Wallet added " << address << ", creation timestamp " << creationTimestamp;
    return address;
  } catch (const std::exception& e) {
    m_logger(ERROR) << "Failed to add wallet: " << e.what();

    try {
      m_containerStorage.pop_back();
    } catch (...) {
      m_logger(ERROR) << "Failed to rollback adding wallet to storage";
    }

    throw;
  }
}

void WalletGreen::deleteAddress(const std::string& address) {
  throwIfNotInitialized();
  throwIfStopped();

  CryptoNote::AccountPublicAddress pubAddr = parseAddress(address);

  auto it = m_walletsContainer.get<KeysIndex>().find(pubAddr.spendPublicKey);
  if (it == m_walletsContainer.get<KeysIndex>().end()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to delete wallet: address not found " << address;
    throw std::system_error(make_error_code(error::OBJECT_NOT_FOUND));
  }

  stopBlockchainSynchronizer();

  m_actualBalance -= it->actualBalance;
  m_pendingBalance -= it->pendingBalance;

  if (it->actualBalance != 0 || it->pendingBalance != 0) {
    m_logger(INFO, BRIGHT_WHITE) << "Container balance updated, actual " << m_currency.formatAmount(m_actualBalance) <<
      ", pending " << m_currency.formatAmount(m_pendingBalance);
  }

  auto addressIndex = std::distance(m_walletsContainer.get<RandomAccessIndex>().begin(), m_walletsContainer.project<RandomAccessIndex>(it));

#if !defined(NDEBUG)
  Crypto::PublicKey publicKey;
  Crypto::SecretKey secretKey;
  uint64_t creationTimestamp;
  decryptKeyPair(m_containerStorage[addressIndex], publicKey, secretKey, creationTimestamp);
  assert(publicKey == it->spendPublicKey);
  assert(secretKey == it->spendSecretKey);
  assert(creationTimestamp == static_cast<uint64_t>(it->creationTimestamp));
#endif

  m_containerStorage.erase(std::next(m_containerStorage.begin(), addressIndex));

  m_synchronizer.removeSubscription(pubAddr);

  deleteContainerFromUnlockTransactionJobs(it->container);
  std::vector<size_t> deletedTransactions;
  std::vector<size_t> updatedTransactions = deleteTransfersForAddress(address, deletedTransactions);
  deleteFromUncommitedTransactions(deletedTransactions);

  m_walletsContainer.get<KeysIndex>().erase(it);
  m_logger(DEBUGGING) << "Wallet count " << m_walletsContainer.size();

  if (m_walletsContainer.get<RandomAccessIndex>().size() != 0) {
    startBlockchainSynchronizer();
  } else {
    m_blockchain.clear();
    m_blockchain.push_back(m_currency.genesisBlockHash());
  }

  for (auto transactionId: updatedTransactions) {
    pushEvent(makeTransactionUpdatedEvent(transactionId));
  }

  m_logger(INFO, BRIGHT_WHITE) << "Wallet deleted " << address;
}

uint64_t WalletGreen::getActualBalance() const {
  throwIfNotInitialized();
  throwIfStopped();

  return m_actualBalance;
}

uint64_t WalletGreen::getActualBalance(const std::string& address) const {
  throwIfNotInitialized();
  throwIfStopped();

  const auto& wallet = getWalletRecord(address);
  return wallet.actualBalance;
}

uint64_t WalletGreen::getPendingBalance() const {
  throwIfNotInitialized();
  throwIfStopped();

  return m_pendingBalance;
}

uint64_t WalletGreen::getPendingBalance(const std::string& address) const {
  throwIfNotInitialized();
  throwIfStopped();

  const auto& wallet = getWalletRecord(address);
  return wallet.pendingBalance;
}

size_t WalletGreen::getTransactionCount() const {
  throwIfNotInitialized();
  throwIfStopped();

  return m_transactions.get<RandomAccessIndex>().size();
}

WalletTransaction WalletGreen::getTransaction(size_t transactionIndex) const {
  throwIfNotInitialized();
  throwIfStopped();

  if (m_transactions.size() <= transactionIndex) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get transaction: invalid index " << transactionIndex << ". Number of transactions: " << m_transactions.size();
    throw std::system_error(make_error_code(CryptoNote::error::INDEX_OUT_OF_RANGE));
  }

  return m_transactions.get<RandomAccessIndex>()[transactionIndex];
}

size_t WalletGreen::getTransactionTransferCount(size_t transactionIndex) const {
  throwIfNotInitialized();
  throwIfStopped();

  auto bounds = getTransactionTransfersRange(transactionIndex);
  return static_cast<size_t>(std::distance(bounds.first, bounds.second));
}

WalletTransfer WalletGreen::getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const {
  throwIfNotInitialized();
  throwIfStopped();

  auto bounds = getTransactionTransfersRange(transactionIndex);

  if (transferIndex >= static_cast<size_t>(std::distance(bounds.first, bounds.second))) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get transfer: invalid transfer index " << transferIndex << ". Transaction index " << transactionIndex <<
      " transfer count " << std::distance(bounds.first, bounds.second);
    throw std::system_error(make_error_code(std::errc::invalid_argument));
  }

  return std::next(bounds.first, transferIndex)->second;
}

WalletGreen::TransfersRange WalletGreen::getTransactionTransfersRange(size_t transactionIndex) const {
  auto val = std::make_pair(transactionIndex, WalletTransfer());

  auto bounds = std::equal_range(m_transfers.begin(), m_transfers.end(), val, [] (const TransactionTransferPair& a, const TransactionTransferPair& b) {
    return a.first < b.first;
  });

  return bounds;
}

size_t WalletGreen::transfer(const TransactionParameters& transactionParameters) {
  size_t id = WALLET_INVALID_TRANSACTION_ID;
  Tools::ScopeExit releaseContext([this, &id] {
    m_dispatcher.yield();

    if (id != WALLET_INVALID_TRANSACTION_ID) {
      auto& tx = m_transactions[id];
      m_logger(INFO, BRIGHT_WHITE) << "Transaction created and send, ID " << id <<
        ", hash " << m_transactions[id].hash <<
        ", state " << tx.state <<
        ", totalAmount " << m_currency.formatAmount(tx.totalAmount) <<
        ", fee " << m_currency.formatAmount(tx.fee) <<
        ", transfers: " << TransferListFormatter(m_currency, getTransactionTransfersRange(id));
    }
  });

  System::EventLock lk(m_readyEvent);

  throwIfNotInitialized();
  throwIfTrackingMode();
  throwIfStopped();

  m_logger(INFO, BRIGHT_WHITE) << "transfer" <<
    ", from " << Common::makeContainerFormatter(transactionParameters.sourceAddresses) <<
    ", to " << WalletOrderListFormatter(m_currency, transactionParameters.destinations) <<
    ", change address '" << transactionParameters.changeDestination << '\'' <<
    ", fee " << m_currency.formatAmount(transactionParameters.fee) <<
    ", mixin " << transactionParameters.mixIn <<
    ", unlockTimestamp " << transactionParameters.unlockTimestamp;

  id = doTransfer(transactionParameters);
  return id;
}

void WalletGreen::prepareTransaction(std::vector<WalletOuts>&& wallets,
  const std::vector<WalletOrder>& orders,
  uint64_t fee,
  uint16_t mixIn,
  const std::string& extra,
  uint64_t unlockTimestamp,
  const DonationSettings& donation,
  const CryptoNote::AccountPublicAddress& changeDestination,
  PreparedTransaction& preparedTransaction) {

  preparedTransaction.destinations = convertOrdersToTransfers(orders);
  preparedTransaction.neededMoney = countNeededMoney(preparedTransaction.destinations, fee);

  std::vector<OutputToTransfer> selectedTransfers;
  uint64_t foundMoney = selectTransfers(preparedTransaction.neededMoney, mixIn == 0, m_currency.defaultDustThreshold(), std::move(wallets), selectedTransfers);

  if (foundMoney < preparedTransaction.neededMoney) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to create transaction: not enough money. Needed " << m_currency.formatAmount(preparedTransaction.neededMoney) <<
      ", found " << m_currency.formatAmount(foundMoney);
    throw std::system_error(make_error_code(error::WRONG_AMOUNT), "Not enough money");
  }

  typedef CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount outs_for_amount;
  std::vector<outs_for_amount> mixinResult;

  if (mixIn != 0) {
    requestMixinOuts(selectedTransfers, mixIn, mixinResult);
  }

  std::vector<InputInfo> keysInfo;
  prepareInputs(selectedTransfers, mixinResult, mixIn, keysInfo);

  uint64_t donationAmount = pushDonationTransferIfPossible(donation, foundMoney - preparedTransaction.neededMoney, m_currency.defaultDustThreshold(), preparedTransaction.destinations);
  preparedTransaction.changeAmount = foundMoney - preparedTransaction.neededMoney - donationAmount;

  std::vector<ReceiverAmounts> decomposedOutputs = splitDestinations(preparedTransaction.destinations, m_currency.defaultDustThreshold(), m_currency);
  if (preparedTransaction.changeAmount != 0) {
    WalletTransfer changeTransfer;
    changeTransfer.type = WalletTransferType::CHANGE;
    changeTransfer.address = m_currency.accountAddressAsString(changeDestination);
    changeTransfer.amount = static_cast<int64_t>(preparedTransaction.changeAmount);
    preparedTransaction.destinations.emplace_back(std::move(changeTransfer));

    auto splittedChange = splitAmount(preparedTransaction.changeAmount, changeDestination, m_currency.defaultDustThreshold());
    decomposedOutputs.emplace_back(std::move(splittedChange));
  }

  preparedTransaction.transaction = makeTransaction(decomposedOutputs, keysInfo, extra, unlockTimestamp);
}

void WalletGreen::validateSourceAddresses(const std::vector<std::string>& sourceAddresses) const {
  validateAddresses(sourceAddresses);

  auto badAddr = std::find_if(sourceAddresses.begin(), sourceAddresses.end(), [this](const std::string& addr) {
    return !isMyAddress(addr);
  });

  if (badAddr != sourceAddresses.end()) {
    m_logger(ERROR, BRIGHT_RED) << "Source address isn't belong to the container: " << *badAddr;
    throw std::system_error(make_error_code(error::BAD_ADDRESS), "Source address must belong to current container: " + *badAddr);
  }
}

void WalletGreen::checkIfEnoughMixins(std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult, uint16_t mixIn) const {
  assert(mixIn != 0);

  auto notEnoughIt = std::find_if(mixinResult.begin(), mixinResult.end(),
    [mixIn](const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& ofa) { return ofa.outs.size() < mixIn; });

  if (notEnoughIt != mixinResult.end()) {
    m_logger(ERROR, BRIGHT_RED) << "Mixin is too big: " << mixIn;
    throw std::system_error(make_error_code(CryptoNote::error::MIXIN_COUNT_TOO_BIG));
  }
}

std::vector<WalletTransfer> WalletGreen::convertOrdersToTransfers(const std::vector<WalletOrder>& orders) const {
  std::vector<WalletTransfer> transfers;
  transfers.reserve(orders.size());

  for (const auto& order : orders) {
    WalletTransfer transfer;

    if (order.amount > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      std::string message = "Order amount must not exceed " + m_currency.formatAmount(std::numeric_limits<decltype(transfer.amount)>::max());
      m_logger(ERROR, BRIGHT_RED) << message;
      throw std::system_error(make_error_code(CryptoNote::error::WRONG_AMOUNT), message);
    }

    transfer.type = WalletTransferType::USUAL;
    transfer.address = order.address;
    transfer.amount = static_cast<int64_t>(order.amount);

    transfers.emplace_back(std::move(transfer));
  }

  return transfers;
}

uint64_t WalletGreen::countNeededMoney(const std::vector<CryptoNote::WalletTransfer>& destinations, uint64_t fee) const {
  uint64_t neededMoney = 0;
  for (const auto& transfer : destinations) {
    if (transfer.amount == 0) {
      m_logger(ERROR, BRIGHT_RED) << "Bad destination: zero amount, address " << transfer.address;
      throw std::system_error(make_error_code(CryptoNote::error::ZERO_DESTINATION));
    } else if (transfer.amount < 0) {
      m_logger(ERROR, BRIGHT_RED) << "Bad destination: negative amount, address " << transfer.address;
      throw std::system_error(make_error_code(std::errc::invalid_argument));
    }

    //to suppress warning
    uint64_t uamount = static_cast<uint64_t>(transfer.amount);
    if (neededMoney <= std::numeric_limits<uint64_t>::max() - uamount) {
      neededMoney += uamount;
    } else {
      m_logger(ERROR, BRIGHT_RED) << "Bad destinations: integer overflow";
      throw std::system_error(make_error_code(CryptoNote::error::SUM_OVERFLOW));
    }
  }

  if (neededMoney <= std::numeric_limits<uint64_t>::max() - fee) {
    neededMoney += fee;
  } else {
    m_logger(ERROR, BRIGHT_RED) << "Bad fee: integer overflow, fee=" << fee;
    throw std::system_error(make_error_code(CryptoNote::error::SUM_OVERFLOW));
  }

  return neededMoney;
}

CryptoNote::AccountPublicAddress WalletGreen::parseAccountAddressString(const std::string& addressString) const {
  CryptoNote::AccountPublicAddress address;

  if (!m_currency.parseAccountAddressString(addressString, address)) {
    m_logger(ERROR, BRIGHT_RED) << "Bad address: " << addressString;
    throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
  }

  return address;
}

uint64_t WalletGreen::pushDonationTransferIfPossible(const DonationSettings& donation, uint64_t freeAmount,
  uint64_t dustThreshold, std::vector<WalletTransfer>& destinations) const {

  uint64_t donationAmount = 0;
  if (!donation.address.empty() && donation.threshold != 0) {
    if (donation.threshold > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      std::string message = "Donation threshold must not exceed " + m_currency.formatAmount(std::numeric_limits<int64_t>::max());
      m_logger(ERROR, BRIGHT_RED) << message;
      throw std::system_error(make_error_code(error::WRONG_AMOUNT), message);
    }

    donationAmount = calculateDonationAmount(freeAmount, donation.threshold, dustThreshold);
    if (donationAmount != 0) {
      destinations.emplace_back(WalletTransfer{ WalletTransferType::DONATION, donation.address, static_cast<int64_t>(donationAmount) });
      m_logger(DEBUGGING) << "Added donation: address " << donation.address << ", amount " << m_currency.formatAmount(donationAmount);
    }
  }

  return donationAmount;
}

void WalletGreen::validateAddresses(const std::vector<std::string>& addresses) const {
  for (const auto& address : addresses) {
    if (!CryptoNote::validateAddress(address, m_currency)) {
      m_logger(ERROR, BRIGHT_RED) << "Bad address: " << address;
      throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
    }
  }
}

void WalletGreen::validateOrders(const std::vector<WalletOrder>& orders) const {
  for (const auto& order : orders) {
    if (!CryptoNote::validateAddress(order.address, m_currency)) {
      m_logger(ERROR, BRIGHT_RED) << "Bad order address: " << order.address;
      throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
    }

    if (order.amount >= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      std::string message = "Order amount must not exceed " + m_currency.formatAmount(std::numeric_limits<int64_t>::max());
      m_logger(ERROR, BRIGHT_RED) << message;
      throw std::system_error(make_error_code(CryptoNote::error::WRONG_AMOUNT), message);
    }
  }
}

void WalletGreen::validateChangeDestination(const std::vector<std::string>& sourceAddresses, const std::string& changeDestination, bool isFusion) const {
  std::string message;
  if (changeDestination.empty()) {
    if (sourceAddresses.size() > 1 || (sourceAddresses.empty() && m_walletsContainer.size() > 1)) {
      message = std::string(isFusion ? "Destination" : "Change destination") + " address is necessary";
      m_logger(ERROR, BRIGHT_RED) << message << ". Source addresses size=" << sourceAddresses.size() << ", wallets count=" << m_walletsContainer.size();
      throw std::system_error(make_error_code(isFusion ? error::DESTINATION_ADDRESS_REQUIRED : error::CHANGE_ADDRESS_REQUIRED), message);
    }
  } else {
    if (!CryptoNote::validateAddress(changeDestination, m_currency)) {
      message = std::string("Bad ") + (isFusion ? "destination" : "change destination") + " address: " + changeDestination;
      m_logger(ERROR, BRIGHT_RED) << message;
      throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS), message);
    }

    if (!isMyAddress(changeDestination)) {
      message = std::string(isFusion ? "Destination" : "Change destination") + " address is not found in current container: " + changeDestination;
      m_logger(ERROR, BRIGHT_RED) << message;
      throw std::system_error(make_error_code(isFusion ? error::DESTINATION_ADDRESS_NOT_FOUND : error::CHANGE_ADDRESS_NOT_FOUND), message);
    }
  }
}

void WalletGreen::validateTransactionParameters(const TransactionParameters& transactionParameters) const {
  if (transactionParameters.destinations.empty()) {
    m_logger(ERROR, BRIGHT_RED) << "No destinations";
    throw std::system_error(make_error_code(error::ZERO_DESTINATION));
  }

  if (transactionParameters.fee < m_currency.minimumFee()) {
    std::string message = "Fee is too small. Fee " + m_currency.formatAmount(transactionParameters.fee) +
      ", minimum fee " + m_currency.formatAmount(m_currency.minimumFee());
    m_logger(ERROR, BRIGHT_RED) << message;
    throw std::system_error(make_error_code(error::FEE_TOO_SMALL), message);
  }

  if (transactionParameters.donation.address.empty() != (transactionParameters.donation.threshold == 0)) {
    std::string message = "DonationSettings must have both address and threshold parameters filled. Address '" + transactionParameters.donation.address + "'" +
      ", threshold " + m_currency.formatAmount(transactionParameters.donation.threshold);
    m_logger(ERROR, BRIGHT_RED) << message;
    throw std::system_error(make_error_code(error::WRONG_PARAMETERS), message);
  }

  validateSourceAddresses(transactionParameters.sourceAddresses);
  validateChangeDestination(transactionParameters.sourceAddresses, transactionParameters.changeDestination, false);
  validateOrders(transactionParameters.destinations);
}

size_t WalletGreen::doTransfer(const TransactionParameters& transactionParameters) {
  validateTransactionParameters(transactionParameters);
  CryptoNote::AccountPublicAddress changeDestination = getChangeDestination(transactionParameters.changeDestination, transactionParameters.sourceAddresses);
  m_logger(DEBUGGING) << "Change address " << m_currency.accountAddressAsString(changeDestination);

  std::vector<WalletOuts> wallets;
  if (!transactionParameters.sourceAddresses.empty()) {
    wallets = pickWallets(transactionParameters.sourceAddresses);
  } else {
    wallets = pickWalletsWithMoney();
  }

  PreparedTransaction preparedTransaction;
  prepareTransaction(std::move(wallets),
    transactionParameters.destinations,
    transactionParameters.fee,
    transactionParameters.mixIn,
    transactionParameters.extra,
    transactionParameters.unlockTimestamp,
    transactionParameters.donation,
    changeDestination,
    preparedTransaction);

  return validateSaveAndSendTransaction(*preparedTransaction.transaction, preparedTransaction.destinations, false, true);
}

size_t WalletGreen::makeTransaction(const TransactionParameters& sendingTransaction) {
  size_t id = WALLET_INVALID_TRANSACTION_ID;
  Tools::ScopeExit releaseContext([this, &id] {
    m_dispatcher.yield();

    if (id != WALLET_INVALID_TRANSACTION_ID) {
      auto& tx = m_transactions[id];
      m_logger(INFO, BRIGHT_WHITE) << "Delayed transaction created, ID " << id <<
        ", hash " << m_transactions[id].hash <<
        ", state " << tx.state <<
        ", totalAmount " << m_currency.formatAmount(tx.totalAmount) <<
        ", fee " << m_currency.formatAmount(tx.fee) <<
        ", transfers: " << TransferListFormatter(m_currency, getTransactionTransfersRange(id));
    }
  });

  System::EventLock lk(m_readyEvent);

  throwIfNotInitialized();
  throwIfTrackingMode();
  throwIfStopped();

  m_logger(INFO, BRIGHT_WHITE) << "makeTransaction" <<
    ", from " << Common::makeContainerFormatter(sendingTransaction.sourceAddresses) <<
    ", to " << WalletOrderListFormatter(m_currency, sendingTransaction.destinations) <<
    ", change address '" << sendingTransaction.changeDestination << '\'' <<
    ", fee " << m_currency.formatAmount(sendingTransaction.fee) <<
    ", mixin " << sendingTransaction.mixIn <<
    ", unlockTimestamp " << sendingTransaction.unlockTimestamp;

  validateTransactionParameters(sendingTransaction);
  CryptoNote::AccountPublicAddress changeDestination = getChangeDestination(sendingTransaction.changeDestination, sendingTransaction.sourceAddresses);
  m_logger(DEBUGGING) << "Change address " << m_currency.accountAddressAsString(changeDestination);

  std::vector<WalletOuts> wallets;
  if (!sendingTransaction.sourceAddresses.empty()) {
    wallets = pickWallets(sendingTransaction.sourceAddresses);
  } else {
    wallets = pickWalletsWithMoney();
  }

  PreparedTransaction preparedTransaction;
  prepareTransaction(
    std::move(wallets),
    sendingTransaction.destinations,
    sendingTransaction.fee,
    sendingTransaction.mixIn,
    sendingTransaction.extra,
    sendingTransaction.unlockTimestamp,
    sendingTransaction.donation,
    changeDestination,
    preparedTransaction);

  id = validateSaveAndSendTransaction(*preparedTransaction.transaction, preparedTransaction.destinations, false, false);
  return id;
}

void WalletGreen::commitTransaction(size_t transactionId) {
  System::EventLock lk(m_readyEvent);

  throwIfNotInitialized();
  throwIfStopped();
  throwIfTrackingMode();

  if (transactionId >= m_transactions.size()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to commit transaction: invalid index " << transactionId << ". Number of transactions: " << m_transactions.size();
    throw std::system_error(make_error_code(CryptoNote::error::INDEX_OUT_OF_RANGE));
  }

  auto txIt = std::next(m_transactions.get<RandomAccessIndex>().begin(), transactionId);
  if (m_uncommitedTransactions.count(transactionId) == 0 || txIt->state != WalletTransactionState::CREATED) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to commit transaction: bad transaction state. Transaction index " << transactionId << ", state " << txIt->state;
    throw std::system_error(make_error_code(error::TX_TRANSFER_IMPOSSIBLE));
  }

  System::Event completion(m_dispatcher);
  std::error_code ec;

  System::RemoteContext<void> relayTransactionContext(m_dispatcher, [this, transactionId, &ec, &completion] () {
    m_node.relayTransaction(m_uncommitedTransactions[transactionId], [&ec, &completion, this](std::error_code error) {
      ec = error;
      this->m_dispatcher.remoteSpawn(std::bind(asyncRequestCompletion, std::ref(completion)));
    });
  });
  relayTransactionContext.get();
  completion.wait();

  if (!ec) {
    updateTransactionStateAndPushEvent(transactionId, WalletTransactionState::SUCCEEDED);
    m_uncommitedTransactions.erase(transactionId);
  } else {
    m_logger(ERROR, BRIGHT_RED) << "Failed to relay transaction: " << ec << ", " << ec.message() << ". Transaction index " << transactionId;
    throw std::system_error(ec);
  }

  m_logger(INFO, BRIGHT_WHITE) << "Delayed transaction sent, ID " << transactionId << ", hash " << m_transactions[transactionId].hash;
}

void WalletGreen::rollbackUncommitedTransaction(size_t transactionId) {
  Tools::ScopeExit releaseContext([this] {
    m_dispatcher.yield();
  });

  System::EventLock lk(m_readyEvent);

  throwIfNotInitialized();
  throwIfStopped();
  throwIfTrackingMode();

  if (transactionId >= m_transactions.size()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to rollback transaction: invalid index " << transactionId << ". Number of transactions: " << m_transactions.size();
    throw std::system_error(make_error_code(CryptoNote::error::INDEX_OUT_OF_RANGE));
  }

  auto txIt = m_transactions.get<RandomAccessIndex>().begin();
  std::advance(txIt, transactionId);
  if (m_uncommitedTransactions.count(transactionId) == 0 || txIt->state != WalletTransactionState::CREATED) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to rollback transaction: bad transaction state. Transaction index " << transactionId << ", state " << txIt->state;
    throw std::system_error(make_error_code(error::TX_CANCEL_IMPOSSIBLE));
  }

  removeUnconfirmedTransaction(getObjectHash(m_uncommitedTransactions[transactionId]));
  m_uncommitedTransactions.erase(transactionId);

  m_logger(INFO, BRIGHT_WHITE) << "Delayed transaction rolled back, ID " << transactionId << ", hash " << m_transactions[transactionId].hash;
}

void WalletGreen::pushBackOutgoingTransfers(size_t txId, const std::vector<WalletTransfer>& destinations) {

  for (const auto& dest: destinations) {
    WalletTransfer d;
    d.type = dest.type;
    d.address = dest.address;
    d.amount = dest.amount;

    m_transfers.emplace_back(txId, std::move(d));
  }
}

size_t WalletGreen::insertOutgoingTransactionAndPushEvent(const Hash& transactionHash, uint64_t fee, const BinaryArray& extra, uint64_t unlockTimestamp) {
  WalletTransaction insertTx;
  insertTx.state = WalletTransactionState::CREATED;
  insertTx.creationTime = static_cast<uint64_t>(time(nullptr));
  insertTx.unlockTime = unlockTimestamp;
  insertTx.blockHeight = CryptoNote::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
  insertTx.extra.assign(reinterpret_cast<const char*>(extra.data()), extra.size());
  insertTx.fee = fee;
  insertTx.hash = transactionHash;
  insertTx.totalAmount = 0; // 0 until transactionHandlingEnd() is called
  insertTx.timestamp = 0; //0 until included in a block
  insertTx.isBase = false;

  size_t txId = m_transactions.get<RandomAccessIndex>().size();
  m_transactions.get<RandomAccessIndex>().push_back(std::move(insertTx));

  pushEvent(makeTransactionCreatedEvent(txId));

  return txId;
}

void WalletGreen::updateTransactionStateAndPushEvent(size_t transactionId, WalletTransactionState state) {
  auto it = std::next(m_transactions.get<RandomAccessIndex>().begin(), transactionId);

  if (it->state != state) {
    m_transactions.get<RandomAccessIndex>().modify(it, [state](WalletTransaction& tx) {
      tx.state = state;
    });

    pushEvent(makeTransactionUpdatedEvent(transactionId));
    m_logger(DEBUGGING) << "Transaction state changed, ID " << transactionId << ", hash " << it->hash << ", new state " << it->state;
  }
}

bool WalletGreen::updateWalletTransactionInfo(size_t transactionId, const CryptoNote::TransactionInformation& info, int64_t totalAmount) {
  auto& txIdIndex = m_transactions.get<RandomAccessIndex>();
  assert(transactionId < txIdIndex.size());
  auto it = std::next(txIdIndex.begin(), transactionId);

  bool updated = false;
  bool r = txIdIndex.modify(it, [this, transactionId, &info, totalAmount, &updated](WalletTransaction& transaction) {
    if (transaction.blockHeight != info.blockHeight) {
      transaction.blockHeight = info.blockHeight;
      updated = true;
    }

    if (transaction.timestamp != info.timestamp) {
      transaction.timestamp = info.timestamp;
      updated = true;
    }

    bool isSucceeded = transaction.state == WalletTransactionState::SUCCEEDED;
    // If transaction was sent to daemon, it can not have CREATED and FAILED states, its state can be SUCCEEDED, CANCELLED or DELETED
    bool wasSent = transaction.state != WalletTransactionState::CREATED && transaction.state != WalletTransactionState::FAILED;
    bool isConfirmed = transaction.blockHeight != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
    if (!isSucceeded && (wasSent || isConfirmed)) {
      //transaction may be deleted first then added again
      transaction.state = WalletTransactionState::SUCCEEDED;
      updated = true;
    }

    if (transaction.totalAmount != totalAmount) {
      transaction.totalAmount = totalAmount;
      updated = true;
    }

    // Fix LegacyWallet error. Some old versions didn't fill extra field
    if (transaction.extra.empty() && !info.extra.empty()) {
      transaction.extra = Common::asString(info.extra);
      updated = true;
    }

    bool isBase = info.totalAmountIn == 0;
    if (transaction.isBase != isBase) {
      transaction.isBase = isBase;
      updated = true;
    }
  });

  assert(r);

  if (updated) {
    m_logger(DEBUGGING) << "Transaction updated, ID " << transactionId <<
      ", hash " << it->hash <<
      ", block " << it->blockHeight <<
      ", state " << it->state;
  }

  return updated;
}

size_t WalletGreen::insertBlockchainTransaction(const TransactionInformation& info, int64_t txBalance) {
  auto& index = m_transactions.get<RandomAccessIndex>();

  WalletTransaction tx;
  tx.state = WalletTransactionState::SUCCEEDED;
  tx.timestamp = info.timestamp;
  tx.blockHeight = info.blockHeight;
  tx.hash = info.transactionHash;
  tx.isBase = info.totalAmountIn == 0;
  if (tx.isBase) {
    tx.fee = 0;
  } else {
    tx.fee = info.totalAmountIn - info.totalAmountOut;
  }

  tx.unlockTime = info.unlockTime;
  tx.extra.assign(reinterpret_cast<const char*>(info.extra.data()), info.extra.size());
  tx.totalAmount = txBalance;
  tx.creationTime = info.timestamp;

  size_t txId = index.size();
  index.push_back(std::move(tx));

  m_logger(DEBUGGING) << "Transaction added, ID " << txId <<
    ", hash " << tx.hash <<
    ", block " << tx.blockHeight <<
    ", state " << tx.state;

  return txId;
}

bool WalletGreen::updateTransactionTransfers(size_t transactionId, const std::vector<ContainerAmounts>& containerAmountsList,
  int64_t allInputsAmount, int64_t allOutputsAmount) {

  assert(allInputsAmount <= 0);
  assert(allOutputsAmount >= 0);

  bool updated = false;

  auto transfersRange = getTransactionTransfersRange(transactionId);
  // Iterators can be invalidated, so the first transfer is addressed by its index
  size_t firstTransferIdx = std::distance(m_transfers.cbegin(), transfersRange.first);

  TransfersMap initialTransfers = getKnownTransfersMap(transactionId, firstTransferIdx);

  std::unordered_set<std::string> myInputAddresses;
  std::unordered_set<std::string> myOutputAddresses;
  int64_t myInputsAmount = 0;
  int64_t myOutputsAmount = 0;
  for (auto containerAmount : containerAmountsList) {
    AccountPublicAddress address{ getWalletRecord(containerAmount.container).spendPublicKey, m_viewPublicKey };
    std::string addressString = m_currency.accountAddressAsString(address);

    updated |= updateAddressTransfers(transactionId, firstTransferIdx, addressString, initialTransfers[addressString].input, containerAmount.amounts.input);
    updated |= updateAddressTransfers(transactionId, firstTransferIdx, addressString, initialTransfers[addressString].output, containerAmount.amounts.output);

    myInputsAmount += containerAmount.amounts.input;
    myOutputsAmount += containerAmount.amounts.output;

    if (containerAmount.amounts.input != 0) {
      myInputAddresses.emplace(addressString);
    }

    if (containerAmount.amounts.output != 0) {
      myOutputAddresses.emplace(addressString);
    }
  }

  assert(myInputsAmount >= allInputsAmount);
  assert(myOutputsAmount <= allOutputsAmount);

  int64_t knownInputsAmount = 0;
  int64_t knownOutputsAmount = 0;
  auto updatedTransfers = getKnownTransfersMap(transactionId, firstTransferIdx);
  for (const auto& pair : updatedTransfers) {
    knownInputsAmount += pair.second.input;
    knownOutputsAmount += pair.second.output;
  }

  assert(myInputsAmount >= knownInputsAmount);
  assert(myOutputsAmount <= knownOutputsAmount);

  updated |= updateUnknownTransfers(transactionId, firstTransferIdx, myInputAddresses, knownInputsAmount, myInputsAmount, allInputsAmount, false);
  updated |= updateUnknownTransfers(transactionId, firstTransferIdx, myOutputAddresses, knownOutputsAmount, myOutputsAmount, allOutputsAmount, true);

  return updated;
}

WalletGreen::TransfersMap WalletGreen::getKnownTransfersMap(size_t transactionId, size_t firstTransferIdx) const {
  TransfersMap result;

  for (auto it = std::next(m_transfers.begin(), firstTransferIdx); it != m_transfers.end() && it->first == transactionId; ++it) {
    const auto& address = it->second.address;

    if (!address.empty()) {
      if (it->second.amount < 0) {
        result[address].input += it->second.amount;
      } else {
        assert(it->second.amount > 0);
        result[address].output += it->second.amount;
      }
    }
  }

  return result;
}

bool WalletGreen::updateAddressTransfers(size_t transactionId, size_t firstTransferIdx, const std::string& address, int64_t knownAmount, int64_t targetAmount) {
  assert((knownAmount > 0 && targetAmount > 0) || (knownAmount < 0 && targetAmount < 0) || knownAmount == 0 || targetAmount == 0);

  bool updated = false;

  if (knownAmount != targetAmount) {
    if (knownAmount == 0) {
      appendTransfer(transactionId, firstTransferIdx, address, targetAmount);
      updated = true;
    } else if (targetAmount == 0) {
      assert(knownAmount != 0);
      updated |= eraseTransfersByAddress(transactionId, firstTransferIdx, address, knownAmount > 0);
    } else {
      updated |= adjustTransfer(transactionId, firstTransferIdx, address, targetAmount);
    }
  }

  return updated;
}

bool WalletGreen::updateUnknownTransfers(size_t transactionId, size_t firstTransferIdx, const std::unordered_set<std::string>& myAddresses,
  int64_t knownAmount, int64_t myAmount, int64_t totalAmount, bool isOutput) {

  bool updated = false;

  if (std::abs(knownAmount) > std::abs(totalAmount)) {
    updated |= eraseForeignTransfers(transactionId, firstTransferIdx, myAddresses, isOutput);
    if (totalAmount == myAmount) {
      updated |= eraseTransfersByAddress(transactionId, firstTransferIdx, std::string(), isOutput);
    } else {
      assert(std::abs(totalAmount) > std::abs(myAmount));
      updated |= adjustTransfer(transactionId, firstTransferIdx, std::string(), totalAmount - myAmount);
    }
  } else if (knownAmount == totalAmount) {
    updated |= eraseTransfersByAddress(transactionId, firstTransferIdx, std::string(), isOutput);
  } else {
    assert(std::abs(totalAmount) > std::abs(knownAmount));
    updated |= adjustTransfer(transactionId, firstTransferIdx, std::string(), totalAmount - knownAmount);
  }

  return updated;
}

void WalletGreen::appendTransfer(size_t transactionId, size_t firstTransferIdx, const std::string& address, int64_t amount) {
  auto it = std::next(m_transfers.begin(), firstTransferIdx);
  auto insertIt = std::upper_bound(it, m_transfers.end(), transactionId, [](size_t transactionId, const TransactionTransferPair& pair) {
    return transactionId < pair.first;
  });

  WalletTransfer transfer{ WalletTransferType::USUAL, address, amount };
  m_transfers.emplace(insertIt, std::piecewise_construct, std::forward_as_tuple(transactionId), std::forward_as_tuple(transfer));
}

bool WalletGreen::adjustTransfer(size_t transactionId, size_t firstTransferIdx, const std::string& address, int64_t amount) {
  assert(amount != 0);

  bool updated = false;
  bool updateOutputTransfers = amount > 0;
  bool firstAddressTransferFound = false;
  auto it = std::next(m_transfers.begin(), firstTransferIdx);
  while (it != m_transfers.end() && it->first == transactionId) {
    assert(it->second.amount != 0);
    bool transferIsOutput = it->second.amount > 0;
    if (transferIsOutput == updateOutputTransfers && it->second.address == address) {
      if (firstAddressTransferFound) {
        it = m_transfers.erase(it);
        updated = true;
      } else {
        if (it->second.amount != amount) {
          it->second.amount = amount;
          updated = true;
        }
        
        firstAddressTransferFound = true;
        ++it;
      }
    } else {
      ++it;
    }
  }

  if (!firstAddressTransferFound) {
    WalletTransfer transfer{ WalletTransferType::USUAL, address, amount };
    m_transfers.emplace(it, std::piecewise_construct, std::forward_as_tuple(transactionId), std::forward_as_tuple(transfer));
    updated = true;
  }

  return updated;
}

bool WalletGreen::eraseTransfers(size_t transactionId, size_t firstTransferIdx, std::function<bool(bool, const std::string&)>&& predicate) {
  bool erased = false;
  auto it = std::next(m_transfers.begin(), firstTransferIdx);
  while (it != m_transfers.end() && it->first == transactionId) {
    bool transferIsOutput = it->second.amount > 0;
    if (predicate(transferIsOutput, it->second.address)) {
      it = m_transfers.erase(it);
      erased = true;
    } else {
      ++it;
    }
  }

  return erased;
}

bool WalletGreen::eraseTransfersByAddress(size_t transactionId, size_t firstTransferIdx, const std::string& address, bool eraseOutputTransfers) {
  return eraseTransfers(transactionId, firstTransferIdx, [&address, eraseOutputTransfers](bool isOutput, const std::string& transferAddress) {
    return eraseOutputTransfers == isOutput && address == transferAddress;
  });
}

bool WalletGreen::eraseForeignTransfers(size_t transactionId, size_t firstTransferIdx, const std::unordered_set<std::string>& knownAddresses,
  bool eraseOutputTransfers) {

  return eraseTransfers(transactionId, firstTransferIdx, [this, &knownAddresses, eraseOutputTransfers](bool isOutput, const std::string& transferAddress) {
    return eraseOutputTransfers == isOutput && knownAddresses.count(transferAddress) == 0;
  });
}

std::unique_ptr<CryptoNote::ITransaction> WalletGreen::makeTransaction(const std::vector<ReceiverAmounts>& decomposedOutputs,
  std::vector<InputInfo>& keysInfo, const std::string& extra, uint64_t unlockTimestamp) {

  std::unique_ptr<ITransaction> tx = createTransaction();

  typedef std::pair<const AccountPublicAddress*, uint64_t> AmountToAddress;
  std::vector<AmountToAddress> amountsToAddresses;
  for (const auto& output: decomposedOutputs) {
    for (auto amount: output.amounts) {
      amountsToAddresses.emplace_back(AmountToAddress{&output.receiver, amount});
    }
  }

  std::shuffle(amountsToAddresses.begin(), amountsToAddresses.end(), std::default_random_engine{Crypto::rand<std::default_random_engine::result_type>()});
  std::sort(amountsToAddresses.begin(), amountsToAddresses.end(), [] (const AmountToAddress& left, const AmountToAddress& right) {
    return left.second < right.second;
  });

  for (const auto& amountToAddress: amountsToAddresses) {
    tx->addOutput(amountToAddress.second, *amountToAddress.first);
  }

  tx->setUnlockTime(unlockTimestamp);
  tx->appendExtra(Common::asBinaryArray(extra));

  for (auto& input: keysInfo) {
    tx->addInput(makeAccountKeys(*input.walletRecord), input.keyInfo, input.ephKeys);
  }

  size_t i = 0;
  for(auto& input: keysInfo) {
    tx->signInputKey(i++, input.keyInfo, input.ephKeys);
  }

  m_logger(DEBUGGING) << "Transaction created, hash " << tx->getTransactionHash() <<
    ", inputs " << m_currency.formatAmount(tx->getInputTotalAmount()) <<
    ", outputs " << m_currency.formatAmount(tx->getOutputTotalAmount()) <<
    ", fee " << m_currency.formatAmount(tx->getInputTotalAmount() - tx->getOutputTotalAmount());
    return tx;
}

void WalletGreen::sendTransaction(const CryptoNote::Transaction& cryptoNoteTransaction) {
  System::Event completion(m_dispatcher);
  std::error_code ec;

  throwIfStopped();

  System::RemoteContext<void> relayTransactionContext(m_dispatcher, [this, &cryptoNoteTransaction, &ec, &completion] () {
    m_node.relayTransaction(cryptoNoteTransaction, [&ec, &completion, this](std::error_code error) {
      ec = error;
      this->m_dispatcher.remoteSpawn(std::bind(asyncRequestCompletion, std::ref(completion)));
    });
  });
  relayTransactionContext.get();
  completion.wait();

  if (ec) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to relay transaction: " << ec << ", " << ec.message() <<
      ". Transaction hash " << getObjectHash(cryptoNoteTransaction);
    throw std::system_error(ec);
  }
}

size_t WalletGreen::validateSaveAndSendTransaction(const ITransactionReader& transaction, const std::vector<WalletTransfer>& destinations, bool isFusion, bool send) {
  BinaryArray transactionData = transaction.getTransactionData();

  if (transactionData.size() > m_upperTransactionSizeLimit) {
    m_logger(ERROR, BRIGHT_RED) << "Transaction is too big. Transaction hash " << transaction.getTransactionHash() <<
      ", size " << transactionData.size() << ", size limit " << m_upperTransactionSizeLimit;
    throw std::system_error(make_error_code(error::TRANSACTION_SIZE_TOO_BIG));
  }

  CryptoNote::Transaction cryptoNoteTransaction;
  if (!fromBinaryArray(cryptoNoteTransaction, transactionData)) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to deserialize created transaction. Transaction hash " << transaction.getTransactionHash();
    throw std::system_error(make_error_code(error::INTERNAL_WALLET_ERROR), "Failed to deserialize created transaction");
  }

  uint64_t fee = transaction.getInputTotalAmount() - transaction.getOutputTotalAmount();
  size_t transactionId = insertOutgoingTransactionAndPushEvent(transaction.getTransactionHash(), fee, transaction.getExtra(), transaction.getUnlockTime());
  m_logger(DEBUGGING) << "Transaction added to container, ID " << transactionId <<
    ", hash " << transaction.getTransactionHash() <<
    ", block " << m_transactions[transactionId].blockHeight <<
    ", state " << m_transactions[transactionId].state;
  Tools::ScopeExit rollbackTransactionInsertion([this, transactionId] {
    updateTransactionStateAndPushEvent(transactionId, WalletTransactionState::FAILED);
  });

  m_fusionTxsCache.emplace(transactionId, isFusion);
  pushBackOutgoingTransfers(transactionId, destinations);

  addUnconfirmedTransaction(transaction);
  Tools::ScopeExit rollbackAddingUnconfirmedTransaction([this, &transaction] {
    try {
      removeUnconfirmedTransaction(transaction.getTransactionHash());
    } catch (...) {
      // Ignore any exceptions. If rollback fails then the transaction is stored as unconfirmed and will be deleted after wallet relaunch
      // during transaction pool synchronization
      m_logger(ERROR, BRIGHT_RED) << "Unknown exception while removing unconfirmed transaction " << transaction.getTransactionHash();
    }
  });

  if (send) {
    sendTransaction(cryptoNoteTransaction);
    m_logger(DEBUGGING) << "Transaction sent to node, ID " << transactionId << ", hash " << transaction.getTransactionHash();
    updateTransactionStateAndPushEvent(transactionId, WalletTransactionState::SUCCEEDED);
  } else {
    assert(m_uncommitedTransactions.count(transactionId) == 0);
    m_uncommitedTransactions.emplace(transactionId, std::move(cryptoNoteTransaction));
    m_logger(DEBUGGING) << "Transaction delayed, ID " << transactionId << ", hash " << transaction.getTransactionHash();
  }

  rollbackAddingUnconfirmedTransaction.cancel();
  rollbackTransactionInsertion.cancel();

  return transactionId;
}

AccountKeys WalletGreen::makeAccountKeys(const WalletRecord& wallet) const {
  AccountKeys keys;
  keys.address.spendPublicKey = wallet.spendPublicKey;
  keys.address.viewPublicKey = m_viewPublicKey;
  keys.spendSecretKey = wallet.spendSecretKey;
  keys.viewSecretKey = m_viewSecretKey;

  return keys;
}

void WalletGreen::requestMixinOuts(
  const std::vector<OutputToTransfer>& selectedTransfers,
  uint16_t mixIn,
  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult) {

  std::vector<uint64_t> amounts;
  for (const auto& out: selectedTransfers) {
    amounts.push_back(out.out.amount);
  }

  System::Event requestFinished(m_dispatcher);
  std::error_code mixinError;

  throwIfStopped();

  uint16_t requestMixinCount = mixIn + 1; //+1 to allow to skip real output

  m_logger(DEBUGGING) << "Requesting random outputs";
  System::RemoteContext<void> getOutputsContext(m_dispatcher, [this, amounts, requestMixinCount, &mixinResult, &requestFinished, &mixinError] () mutable {
    m_node.getRandomOutsByAmounts(std::move(amounts), requestMixinCount, mixinResult, [&requestFinished, &mixinError, this] (std::error_code ec) mutable {
      mixinError = ec;
      m_dispatcher.remoteSpawn(std::bind(asyncRequestCompletion, std::ref(requestFinished)));
    });
  });
  getOutputsContext.get();
  requestFinished.wait();

  checkIfEnoughMixins(mixinResult, requestMixinCount);

  if (mixinError) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get random outputs: " << mixinError << ", " << mixinError.message();
    throw std::system_error(mixinError);
  }

  m_logger(DEBUGGING) << "Random outputs received";
}

uint64_t WalletGreen::selectTransfers(
  uint64_t neededMoney,
  bool dust,
  uint64_t dustThreshold,
  std::vector<WalletOuts>&& wallets,
  std::vector<OutputToTransfer>& selectedTransfers) {

  uint64_t foundMoney = 0;

  typedef std::pair<WalletRecord*, TransactionOutputInformation> OutputData;
  std::vector<OutputData> dustOutputs;
  std::vector<OutputData> walletOuts;
  for (auto walletIt = wallets.begin(); walletIt != wallets.end(); ++walletIt) {
    for (auto outIt = walletIt->outs.begin(); outIt != walletIt->outs.end(); ++outIt) {
      if (outIt->amount > dustThreshold) {
        walletOuts.emplace_back(std::piecewise_construct, std::forward_as_tuple(walletIt->wallet), std::forward_as_tuple(*outIt));
      } else if (dust) {
        dustOutputs.emplace_back(std::piecewise_construct, std::forward_as_tuple(walletIt->wallet), std::forward_as_tuple(*outIt));
      }
    }
  }

  ShuffleGenerator<size_t, Crypto::random_engine<size_t>> indexGenerator(walletOuts.size());
  while (foundMoney < neededMoney && !indexGenerator.empty()) {
    auto& out = walletOuts[indexGenerator()];
    foundMoney += out.second.amount;
    selectedTransfers.emplace_back(OutputToTransfer{ std::move(out.second), std::move(out.first) });
  }

  if (dust && !dustOutputs.empty()) {
    ShuffleGenerator<size_t, Crypto::random_engine<size_t>> dustIndexGenerator(dustOutputs.size());
    do {
      auto& out = dustOutputs[dustIndexGenerator()];
      foundMoney += out.second.amount;
      selectedTransfers.emplace_back(OutputToTransfer{ std::move(out.second), std::move(out.first) });
    } while (foundMoney < neededMoney && !dustIndexGenerator.empty());
  }

  return foundMoney;
};

std::vector<WalletGreen::WalletOuts> WalletGreen::pickWalletsWithMoney() const {
  auto& walletsIndex = m_walletsContainer.get<RandomAccessIndex>();

  std::vector<WalletOuts> walletOuts;
  for (const auto& wallet: walletsIndex) {
    if (wallet.actualBalance == 0) {
      continue;
    }

    ITransfersContainer* container = wallet.container;

    WalletOuts outs;
    container->getOutputs(outs.outs, ITransfersContainer::IncludeKeyUnlocked);
    outs.wallet = const_cast<WalletRecord *>(&wallet);

    walletOuts.push_back(std::move(outs));
  };

  return walletOuts;
}

WalletGreen::WalletOuts WalletGreen::pickWallet(const std::string& address) const {
  const auto& wallet = getWalletRecord(address);

  ITransfersContainer* container = wallet.container;
  WalletOuts outs;
  container->getOutputs(outs.outs, ITransfersContainer::IncludeKeyUnlocked);
  outs.wallet = const_cast<WalletRecord *>(&wallet);

  return outs;
}

std::vector<WalletGreen::WalletOuts> WalletGreen::pickWallets(const std::vector<std::string>& addresses) const {
  std::vector<WalletOuts> wallets;
  wallets.reserve(addresses.size());

  for (const auto& address: addresses) {
    WalletOuts wallet = pickWallet(address);
    if (!wallet.outs.empty()) {
      wallets.emplace_back(std::move(wallet));
    }
  }

  return wallets;
}

std::vector<CryptoNote::WalletGreen::ReceiverAmounts> WalletGreen::splitDestinations(const std::vector<CryptoNote::WalletTransfer>& destinations,
  uint64_t dustThreshold,
  const CryptoNote::Currency& currency) {

  std::vector<ReceiverAmounts> decomposedOutputs;
  for (const auto& destination: destinations) {
    AccountPublicAddress address = parseAccountAddressString(destination.address);
    decomposedOutputs.push_back(splitAmount(destination.amount, address, dustThreshold));
  }

  return decomposedOutputs;
}

CryptoNote::WalletGreen::ReceiverAmounts WalletGreen::splitAmount(
  uint64_t amount,
  const AccountPublicAddress& destination,
  uint64_t dustThreshold) {

  ReceiverAmounts receiverAmounts;

  receiverAmounts.receiver = destination;
  decomposeAmount(amount, dustThreshold, receiverAmounts.amounts);
  return receiverAmounts;
}

void WalletGreen::prepareInputs(
  const std::vector<OutputToTransfer>& selectedTransfers,
  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& mixinResult,
  uint16_t mixIn,
  std::vector<InputInfo>& keysInfo) {

  typedef CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry out_entry;

  size_t i = 0;
  for (const auto& input: selectedTransfers) {
    TransactionTypes::InputKeyInfo keyInfo;
    keyInfo.amount = input.out.amount;

    if(mixinResult.size()) {
      std::sort(mixinResult[i].outs.begin(), mixinResult[i].outs.end(),
        [] (const out_entry& a, const out_entry& b) { return a.global_amount_index < b.global_amount_index; });
      for (auto& fakeOut: mixinResult[i].outs) {

        if (input.out.globalOutputIndex == fakeOut.global_amount_index) {
          continue;
        }

        TransactionTypes::GlobalOutput globalOutput;
        globalOutput.outputIndex = static_cast<uint32_t>(fakeOut.global_amount_index);
        globalOutput.targetKey = reinterpret_cast<PublicKey&>(fakeOut.out_key);
        keyInfo.outputs.push_back(std::move(globalOutput));
        if(keyInfo.outputs.size() >= mixIn)
          break;
      }
    }

    //paste real transaction to the random index
    auto insertIn = std::find_if(keyInfo.outputs.begin(), keyInfo.outputs.end(), [&](const TransactionTypes::GlobalOutput& a) {
      return a.outputIndex >= input.out.globalOutputIndex;
    });

    TransactionTypes::GlobalOutput realOutput;
    realOutput.outputIndex = input.out.globalOutputIndex;
    realOutput.targetKey = reinterpret_cast<const PublicKey&>(input.out.outputKey);

    auto insertedIn = keyInfo.outputs.insert(insertIn, realOutput);

    keyInfo.realOutput.transactionPublicKey = reinterpret_cast<const PublicKey&>(input.out.transactionPublicKey);
    keyInfo.realOutput.transactionIndex = static_cast<size_t>(insertedIn - keyInfo.outputs.begin());
    keyInfo.realOutput.outputInTransaction = input.out.outputInTransaction;

    //Important! outputs in selectedTransfers and in keysInfo must have the same order!
    InputInfo inputInfo;
    inputInfo.keyInfo = std::move(keyInfo);
    inputInfo.walletRecord = input.wallet;
    keysInfo.push_back(std::move(inputInfo));
    ++i;
  }
}

WalletTransactionWithTransfers WalletGreen::getTransaction(const Crypto::Hash& transactionHash) const {
  throwIfNotInitialized();
  throwIfStopped();

  auto& hashIndex = m_transactions.get<TransactionIndex>();
  auto it = hashIndex.find(transactionHash);
  if (it == hashIndex.end()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get transaction: not found. Transaction hash " << transactionHash;
    throw std::system_error(make_error_code(error::OBJECT_NOT_FOUND), "Transaction not found");
  }

  WalletTransactionWithTransfers walletTransaction;
  walletTransaction.transaction = *it;
  walletTransaction.transfers = getTransactionTransfers(*it);

  return walletTransaction;
}

std::vector<TransactionsInBlockInfo> WalletGreen::getTransactions(const Crypto::Hash& blockHash, size_t count) const {
  throwIfNotInitialized();
  throwIfStopped();

  auto& hashIndex = m_blockchain.get<BlockHashIndex>();
  auto it = hashIndex.find(blockHash);
  if (it == hashIndex.end()) {
    return std::vector<TransactionsInBlockInfo>();
  }

  auto heightIt = m_blockchain.project<BlockHeightIndex>(it);

  uint32_t blockIndex = static_cast<uint32_t>(std::distance(m_blockchain.get<BlockHeightIndex>().begin(), heightIt));
  return getTransactionsInBlocks(blockIndex, count);
}

std::vector<TransactionsInBlockInfo> WalletGreen::getTransactions(uint32_t blockIndex, size_t count) const {
  throwIfNotInitialized();
  throwIfStopped();

  return getTransactionsInBlocks(blockIndex, count);
}

std::vector<Crypto::Hash> WalletGreen::getBlockHashes(uint32_t blockIndex, size_t count) const {
  throwIfNotInitialized();
  throwIfStopped();

  auto& index = m_blockchain.get<BlockHeightIndex>();

  if (blockIndex >= index.size()) {
    return std::vector<Crypto::Hash>();
  }

  auto start = std::next(index.begin(), blockIndex);
  auto end = std::next(index.begin(), std::min(index.size(), blockIndex + count));
  return std::vector<Crypto::Hash>(start, end);
}

uint32_t WalletGreen::getBlockCount() const {
  throwIfNotInitialized();
  throwIfStopped();

  uint32_t blockCount = static_cast<uint32_t>(m_blockchain.size());
  assert(blockCount != 0);

  return blockCount;
}

std::vector<WalletTransactionWithTransfers> WalletGreen::getUnconfirmedTransactions() const {
  throwIfNotInitialized();
  throwIfStopped();

  std::vector<WalletTransactionWithTransfers> result;
  auto lowerBound = m_transactions.get<BlockHeightIndex>().lower_bound(WALLET_UNCONFIRMED_TRANSACTION_HEIGHT);
  for (auto it = lowerBound; it != m_transactions.get<BlockHeightIndex>().end(); ++it) {
    if (it->state != WalletTransactionState::SUCCEEDED) {
      continue;
    }

    WalletTransactionWithTransfers transaction;
    transaction.transaction = *it;
    transaction.transfers = getTransactionTransfers(*it);

    result.push_back(transaction);
  }

  return result;
}

std::vector<size_t> WalletGreen::getDelayedTransactionIds() const {
  throwIfNotInitialized();
  throwIfStopped();
  throwIfTrackingMode();

  std::vector<size_t> result;
  result.reserve(m_uncommitedTransactions.size());

  for (const auto& kv: m_uncommitedTransactions) {
    result.push_back(kv.first);
  }

  return result;
}

void WalletGreen::start() {
  m_logger(INFO, BRIGHT_WHITE) << "Starting container";
  m_stopped = false;
}

void WalletGreen::stop() {
  m_logger(INFO, BRIGHT_WHITE) << "Stopping container";
  m_stopped = true;
  m_eventOccurred.set();
}

WalletEvent WalletGreen::getEvent() {
  throwIfNotInitialized();
  throwIfStopped();

  while (m_events.empty()) {
    m_eventOccurred.wait();
    m_eventOccurred.clear();
    throwIfStopped();
  }

  WalletEvent event = std::move(m_events.front());
  m_events.pop();

  return event;
}

void WalletGreen::throwIfNotInitialized() const {
  if (m_state != WalletState::INITIALIZED) {
    m_logger(ERROR, BRIGHT_RED) << "WalletGreen is not initialized. Current state: " << m_state;
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
}

void WalletGreen::onError(ITransfersSubscription* object, uint32_t height, std::error_code ec) {
  m_logger(ERROR, BRIGHT_RED) << "Synchronization error: " << ec << ", " << ec.message() << ", height " << height;
}

void WalletGreen::synchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount) {
  m_dispatcher.remoteSpawn([processedBlockCount, totalBlockCount, this]() { onSynchronizationProgressUpdated(processedBlockCount, totalBlockCount); });
}

void WalletGreen::synchronizationCompleted(std::error_code result) {
  m_dispatcher.remoteSpawn([this]() { onSynchronizationCompleted(); });
}

void WalletGreen::onSynchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount) {
  m_totalBlockCount = totalBlockCount;
  assert(processedBlockCount > 0);

  System::EventLock lk(m_readyEvent);

  m_logger(TRACE) << "onSynchronizationProgressUpdated processedBlockCount " << processedBlockCount << ", totalBlockCount " << totalBlockCount;

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  pushEvent(makeSyncProgressUpdatedEvent(processedBlockCount, totalBlockCount));

  uint32_t currentHeight = processedBlockCount - 1;
  unlockBalances(currentHeight);
}

void WalletGreen::onSynchronizationCompleted() {
  System::EventLock lk(m_readyEvent);

  m_logger(TRACE) << "onSynchronizationCompleted";

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  pushEvent(makeSyncCompletedEvent());
}

void WalletGreen::onBlocksAdded(const Crypto::PublicKey& viewPublicKey, const std::vector<Crypto::Hash>& blockHashes) {
  m_dispatcher.remoteSpawn([this, blockHashes] () { blocksAdded(blockHashes); } );
}

void WalletGreen::blocksAdded(const std::vector<Crypto::Hash>& blockHashes) {
  System::EventLock lk(m_readyEvent);

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  m_blockchain.insert(m_blockchain.end(), blockHashes.begin(), blockHashes.end());
}

void WalletGreen::onBlockchainDetach(const Crypto::PublicKey& viewPublicKey, uint32_t blockIndex) {
  m_dispatcher.remoteSpawn([this, blockIndex] () { blocksRollback(blockIndex); } );
}

void WalletGreen::blocksRollback(uint32_t blockIndex) {
  System::EventLock lk(m_readyEvent);

  m_logger(TRACE) << "blocksRollback " << blockIndex;

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  auto& blockHeightIndex = m_blockchain.get<BlockHeightIndex>();
  blockHeightIndex.erase(std::next(blockHeightIndex.begin(), blockIndex), blockHeightIndex.end());
}

void WalletGreen::onTransactionDeleteBegin(const Crypto::PublicKey& viewPublicKey, Crypto::Hash transactionHash) {
  m_dispatcher.remoteSpawn([=]() { transactionDeleteBegin(transactionHash); });
}

// TODO remove
void WalletGreen::transactionDeleteBegin(Crypto::Hash transactionHash) {
  m_logger(TRACE) << "transactionDeleteBegin " << transactionHash;
}

void WalletGreen::onTransactionDeleteEnd(const Crypto::PublicKey& viewPublicKey, Crypto::Hash transactionHash) {
  m_dispatcher.remoteSpawn([=]() { transactionDeleteEnd(transactionHash); });
}

// TODO remove
void WalletGreen::transactionDeleteEnd(Crypto::Hash transactionHash) {
  m_logger(TRACE) << "transactionDeleteEnd " << transactionHash;
}

void WalletGreen::unlockBalances(uint32_t height) {
  auto& index = m_unlockTransactionsJob.get<BlockHeightIndex>();
  auto upper = index.upper_bound(height);

  if (index.begin() != upper) {
    for (auto it = index.begin(); it != upper; ++it) {
      updateBalance(it->container);
    }

    index.erase(index.begin(), upper);
    pushEvent(makeMoneyUnlockedEvent());
  }
}

void WalletGreen::onTransactionUpdated(ITransfersSubscription* /*object*/, const Crypto::Hash& /*transactionHash*/) {
  // Deprecated, ignore it. New event handler is onTransactionUpdated(const Crypto::PublicKey&, const Crypto::Hash&, const std::vector<ITransfersContainer*>&)
}

void WalletGreen::onTransactionUpdated(const Crypto::PublicKey&, const Crypto::Hash& transactionHash, const std::vector<ITransfersContainer*>& containers) {
  assert(!containers.empty());

  TransactionInformation info;
  std::vector<ContainerAmounts> containerAmountsList;
  containerAmountsList.reserve(containers.size());
  for (auto container : containers) {
    uint64_t inputsAmount;
    // Don't move this code to the following remote spawn, because it guarantees that the container has the transaction
    uint64_t outputsAmount;
    bool found = container->getTransactionInformation(transactionHash, info, &inputsAmount, &outputsAmount);
    assert(found);

    ContainerAmounts containerAmounts;
    containerAmounts.container = container;
    containerAmounts.amounts.input = -static_cast<int64_t>(inputsAmount);
    containerAmounts.amounts.output = static_cast<int64_t>(outputsAmount);
    containerAmountsList.emplace_back(std::move(containerAmounts));
  }

  m_dispatcher.remoteSpawn([this, info, containerAmountsList] {
    this->transactionUpdated(info, containerAmountsList);
  });
}

void WalletGreen::transactionUpdated(const TransactionInformation& transactionInfo, const std::vector<ContainerAmounts>& containerAmountsList) {
  System::EventLock lk(m_readyEvent);

  m_logger(DEBUGGING) << "transactionUpdated event, hash " << transactionInfo.transactionHash <<
    ", block " << transactionInfo.blockHeight <<
    ", totalAmountIn " << m_currency.formatAmount(transactionInfo.totalAmountIn) <<
    ", totalAmountOut " << m_currency.formatAmount(transactionInfo.totalAmountOut) <<
    (transactionInfo.paymentId == NULL_HASH ? "" : ", paymentId " + podToHex(transactionInfo.paymentId));

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  bool updated = false;
  bool isNew = false;

  int64_t totalAmount = std::accumulate(containerAmountsList.begin(), containerAmountsList.end(), static_cast<int64_t>(0),
    [](int64_t sum, const ContainerAmounts& containerAmounts) { return sum + containerAmounts.amounts.input + containerAmounts.amounts.output; });

  size_t transactionId;
  auto& hashIndex = m_transactions.get<TransactionIndex>();
  auto it = hashIndex.find(transactionInfo.transactionHash);
  if (it != hashIndex.end()) {
    transactionId = std::distance(m_transactions.get<RandomAccessIndex>().begin(), m_transactions.project<RandomAccessIndex>(it));
    updated |= updateWalletTransactionInfo(transactionId, transactionInfo, totalAmount);
  } else {
    isNew = true;
    transactionId = insertBlockchainTransaction(transactionInfo, totalAmount);
    m_fusionTxsCache.emplace(transactionId, isFusionTransaction(*it));
  }

  if (transactionInfo.blockHeight != CryptoNote::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
    // In some cases a transaction can be included to a block but not removed from m_uncommitedTransactions. Fix it
    m_uncommitedTransactions.erase(transactionId);
  }

  // Update cached balance
  for (auto containerAmounts : containerAmountsList) {
    updateBalance(containerAmounts.container);

    if (transactionInfo.blockHeight != CryptoNote::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
      uint32_t unlockHeight = std::max(transactionInfo.blockHeight + m_transactionSoftLockTime, static_cast<uint32_t>(transactionInfo.unlockTime));
      insertUnlockTransactionJob(transactionInfo.transactionHash, unlockHeight, containerAmounts.container);
    }
  }

  bool transfersUpdated = updateTransactionTransfers(transactionId, containerAmountsList, -static_cast<int64_t>(transactionInfo.totalAmountIn),
    static_cast<int64_t>(transactionInfo.totalAmountOut));
  updated |= transfersUpdated;

  if (isNew) {
    const auto& tx = m_transactions[transactionId];
    m_logger(INFO, BRIGHT_WHITE) << "New transaction received, ID " << transactionId <<
      ", hash " << tx.hash <<
      ", state " << tx.state <<
      ", totalAmount " << m_currency.formatAmount(tx.totalAmount) <<
      ", fee " << m_currency.formatAmount(tx.fee) <<
      ", transfers: " << TransferListFormatter(m_currency, getTransactionTransfersRange(transactionId));

    pushEvent(makeTransactionCreatedEvent(transactionId));
  } else if (updated) {
    if (transfersUpdated) {
      m_logger(DEBUGGING) << "Transaction transfers updated, ID " << transactionId << ", hash " << m_transactions[transactionId].hash <<
        ", transfers: " << TransferListFormatter(m_currency, getTransactionTransfersRange(transactionId));
    }

    pushEvent(makeTransactionUpdatedEvent(transactionId));
  }
}

void WalletGreen::pushEvent(const WalletEvent& event) {
  m_events.push(event);
  m_eventOccurred.set();
}

size_t WalletGreen::getTransactionId(const Hash& transactionHash) const {
  auto it = m_transactions.get<TransactionIndex>().find(transactionHash);

  if (it == m_transactions.get<TransactionIndex>().end()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get transaction ID: hash not found. Transaction hash " << transactionHash;
    throw std::system_error(make_error_code(std::errc::invalid_argument));
  }

  auto rndIt = m_transactions.project<RandomAccessIndex>(it);
  auto txId = std::distance(m_transactions.get<RandomAccessIndex>().begin(), rndIt);

  return txId;
}

void WalletGreen::onTransactionDeleted(ITransfersSubscription* object, const Hash& transactionHash) {
  m_dispatcher.remoteSpawn([object, transactionHash, this] () {
    this->transactionDeleted(object, transactionHash); });
}

void WalletGreen::transactionDeleted(ITransfersSubscription* object, const Hash& transactionHash) {
  System::EventLock lk(m_readyEvent);

  m_logger(DEBUGGING) << "transactionDeleted event, hash " << transactionHash;

  if (m_state == WalletState::NOT_INITIALIZED) {
    return;
  }

  auto it = m_transactions.get<TransactionIndex>().find(transactionHash);
  if (it == m_transactions.get<TransactionIndex>().end()) {
    return;
  }

  CryptoNote::ITransfersContainer* container = &object->getContainer();
  updateBalance(container);
  deleteUnlockTransactionJob(transactionHash);

  bool updated = false;
  m_transactions.get<TransactionIndex>().modify(it, [this, &transactionHash, &updated](CryptoNote::WalletTransaction& tx) {
    if (tx.state == WalletTransactionState::CREATED || tx.state == WalletTransactionState::SUCCEEDED) {
      tx.state = WalletTransactionState::CANCELLED;
      updated = true;
    }

    if (tx.blockHeight != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
      tx.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
      updated = true;
    }
  });

  if (updated) {
    auto transactionId = getTransactionId(transactionHash);
    auto tx = m_transactions[transactionId];
    m_logger(INFO, BRIGHT_WHITE) << "Transaction deleted, ID " << transactionId <<
      ", hash " << transactionHash <<
      ", state " << tx.state <<
      ", block " << tx.blockHeight <<
      ", totalAmount " << m_currency.formatAmount(tx.totalAmount) <<
      ", fee " << m_currency.formatAmount(tx.fee);
    pushEvent(makeTransactionUpdatedEvent(transactionId));
  }
}

void WalletGreen::insertUnlockTransactionJob(const Hash& transactionHash, uint32_t blockHeight, CryptoNote::ITransfersContainer* container) {
  auto& index = m_unlockTransactionsJob.get<BlockHeightIndex>();
  index.insert( { blockHeight, container, transactionHash } );
}

void WalletGreen::deleteUnlockTransactionJob(const Hash& transactionHash) {
  auto& index = m_unlockTransactionsJob.get<TransactionHashIndex>();
  index.erase(transactionHash);
}

void WalletGreen::startBlockchainSynchronizer() {
  if (!m_walletsContainer.empty() && !m_blockchainSynchronizerStarted) {
    m_logger(DEBUGGING) << "Starting BlockchainSynchronizer";
    m_blockchainSynchronizer.start();
    m_blockchainSynchronizerStarted = true;
  }
}

void WalletGreen::stopBlockchainSynchronizer() {
  if (m_blockchainSynchronizerStarted) {
    m_logger(DEBUGGING) << "Stopping BlockchainSynchronizer";
    System::RemoteContext<void> stopContext(m_dispatcher, [this] () {
      m_blockchainSynchronizer.stop();
    });
    stopContext.get();

    m_blockchainSynchronizerStarted = false;
  }
}

void WalletGreen::addUnconfirmedTransaction(const ITransactionReader& transaction) {
  System::RemoteContext<std::error_code> context(m_dispatcher, [this, &transaction] {
    return m_blockchainSynchronizer.addUnconfirmedTransaction(transaction).get();
  });

  auto ec = context.get();
  if (ec) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to add unconfirmed transaction: " << ec << ", " << ec.message();
    throw std::system_error(ec, "Failed to add unconfirmed transaction");
  }

  m_logger(DEBUGGING) << "Unconfirmed transaction added to BlockchainSynchronizer, hash " << transaction.getTransactionHash();
}

void WalletGreen::removeUnconfirmedTransaction(const Crypto::Hash& transactionHash) {
  System::RemoteContext<void> context(m_dispatcher, [this, &transactionHash] {
    m_blockchainSynchronizer.removeUnconfirmedTransaction(transactionHash).get();
  });

  context.get();
  m_logger(DEBUGGING) << "Unconfirmed transaction removed from BlockchainSynchronizer, hash " << transactionHash;
}

void WalletGreen::updateBalance(CryptoNote::ITransfersContainer* container) {
  auto it = m_walletsContainer.get<TransfersContainerIndex>().find(container);

  if (it == m_walletsContainer.get<TransfersContainerIndex>().end()) {
    return;
  }

  uint64_t actual = container->balance(ITransfersContainer::IncludeAllUnlocked);
  uint64_t pending = container->balance(ITransfersContainer::IncludeAllLocked);

  bool updated = false;

  if (it->actualBalance < actual) {
    m_actualBalance += actual - it->actualBalance;
    updated = true;
  } else if (it->actualBalance > actual) {
    m_actualBalance -= it->actualBalance - actual;
    updated = true;
  }

  if (it->pendingBalance < pending) {
    m_pendingBalance += pending - it->pendingBalance;
    updated = true;
  } else if (it->pendingBalance > pending) {
    m_pendingBalance -= it->pendingBalance - pending;
    updated = true;
  }

  if (updated) {
    m_walletsContainer.get<TransfersContainerIndex>().modify(it, [actual, pending](WalletRecord& wallet) {
      wallet.actualBalance = actual;
      wallet.pendingBalance = pending;
    });

    m_logger(INFO, BRIGHT_WHITE) << "Wallet balance updated, address " << m_currency.accountAddressAsString({ it->spendPublicKey, m_viewPublicKey }) <<
      ", actual " << m_currency.formatAmount(it->actualBalance) <<
      ", pending " << m_currency.formatAmount(it->pendingBalance);
    m_logger(INFO, BRIGHT_WHITE) << "Container balance updated, actual " << m_currency.formatAmount(m_actualBalance) <<
      ", pending " << m_currency.formatAmount(m_pendingBalance);
  }
}

const WalletRecord& WalletGreen::getWalletRecord(const PublicKey& key) const {
  auto it = m_walletsContainer.get<KeysIndex>().find(key);
  if (it == m_walletsContainer.get<KeysIndex>().end()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get wallet: not found. Spend public key " << key;
    throw std::system_error(make_error_code(error::WALLET_NOT_FOUND));
  }

  return *it;
}

const WalletRecord& WalletGreen::getWalletRecord(const std::string& address) const {
  CryptoNote::AccountPublicAddress pubAddr = parseAddress(address);
  return getWalletRecord(pubAddr.spendPublicKey);
}

const WalletRecord& WalletGreen::getWalletRecord(CryptoNote::ITransfersContainer* container) const {
  auto it = m_walletsContainer.get<TransfersContainerIndex>().find(container);
  if (it == m_walletsContainer.get<TransfersContainerIndex>().end()) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to get wallet by container: not found";
    throw std::system_error(make_error_code(error::WALLET_NOT_FOUND));
  }

  return *it;
}

CryptoNote::AccountPublicAddress WalletGreen::parseAddress(const std::string& address) const {
  CryptoNote::AccountPublicAddress pubAddr;

  if (!m_currency.parseAccountAddressString(address, pubAddr)) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to parse address: " << address;
    throw std::system_error(make_error_code(error::BAD_ADDRESS));
  }

  return pubAddr;
}

void WalletGreen::throwIfStopped() const {
  if (m_stopped) {
    m_logger(ERROR, BRIGHT_RED) << "WalletGreen is already stopped";
    throw std::system_error(make_error_code(error::OPERATION_CANCELLED));
  }
}

void WalletGreen::throwIfTrackingMode() const {
  if (getTrackingMode() == WalletTrackingMode::TRACKING) {
    m_logger(ERROR, BRIGHT_RED) << "WalletGreen is in tracking mode";
    throw std::system_error(make_error_code(error::TRACKING_MODE));
  }
}

WalletGreen::WalletTrackingMode WalletGreen::getTrackingMode() const {
  if (m_walletsContainer.get<RandomAccessIndex>().empty()) {
    return WalletTrackingMode::NO_ADDRESSES;
  }

  return m_walletsContainer.get<RandomAccessIndex>().begin()->spendSecretKey == NULL_SECRET_KEY ?
        WalletTrackingMode::TRACKING : WalletTrackingMode::NOT_TRACKING;
}

size_t WalletGreen::createFusionTransaction(uint64_t threshold, uint16_t mixin,
  const std::vector<std::string>& sourceAddresses, const std::string& destinationAddress) {

  size_t id = WALLET_INVALID_TRANSACTION_ID;
  Tools::ScopeExit releaseContext([this, &id] {
    m_dispatcher.yield();

    if (id != WALLET_INVALID_TRANSACTION_ID) {
      auto& tx = m_transactions[id];
      m_logger(INFO, BRIGHT_WHITE) << "Fusion transaction created and sent, ID " << id <<
        ", hash " << m_transactions[id].hash <<
        ", state " << tx.state <<
        ", transfers: " << TransferListFormatter(m_currency, getTransactionTransfersRange(id));
    }
  });

  System::EventLock lk(m_readyEvent);

  m_logger(INFO, BRIGHT_WHITE) << "createFusionTransaction" <<
    ", from " << Common::makeContainerFormatter(sourceAddresses) <<
    ", to '" << destinationAddress << '\'' <<
    ", threshold " << m_currency.formatAmount(threshold) <<
    ", mixin " << mixin;

  throwIfNotInitialized();
  throwIfTrackingMode();
  throwIfStopped();

  validateSourceAddresses(sourceAddresses);
  validateChangeDestination(sourceAddresses, destinationAddress, true);

  const size_t MAX_FUSION_OUTPUT_COUNT = 4;

  if (threshold <= m_currency.defaultDustThreshold()) {
    m_logger(ERROR, BRIGHT_RED) << "Fusion transaction threshold is too small. Threshold " << m_currency.formatAmount(threshold) <<
      ", minimum threshold " << m_currency.formatAmount(m_currency.defaultDustThreshold() + 1);
    throw std::runtime_error("Threshold must be greater than " + m_currency.formatAmount(m_currency.defaultDustThreshold()));
  }

  if (m_walletsContainer.get<RandomAccessIndex>().size() == 0) {
    m_logger(ERROR, BRIGHT_RED) << "The container doesn't have any wallets";
    throw std::runtime_error("You must have at least one address");
  }

  size_t estimatedFusionInputsCount = m_currency.getApproximateMaximumInputCount(m_currency.fusionTxMaxSize(), MAX_FUSION_OUTPUT_COUNT, mixin);
  if (estimatedFusionInputsCount < m_currency.fusionTxMinInputCount()) {
    m_logger(ERROR, BRIGHT_RED) << "Fusion transaction mixin is too big " << mixin;
    throw std::system_error(make_error_code(error::MIXIN_COUNT_TOO_BIG));
  }

  auto fusionInputs = pickRandomFusionInputs(sourceAddresses, threshold, m_currency.fusionTxMinInputCount(), estimatedFusionInputsCount);
  if (fusionInputs.size() < m_currency.fusionTxMinInputCount()) {
    //nothing to optimize
    m_logger(WARNING, BRIGHT_YELLOW) << "Fusion transaction not created: nothing to optimize, threshold " << m_currency.formatAmount(threshold);
    return WALLET_INVALID_TRANSACTION_ID;
  }

  typedef CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount outs_for_amount;
  std::vector<outs_for_amount> mixinResult;
  if (mixin != 0) {
    requestMixinOuts(fusionInputs, mixin, mixinResult);
  }

  std::vector<InputInfo> keysInfo;
  prepareInputs(fusionInputs, mixinResult, mixin, keysInfo);

  AccountPublicAddress destination = getChangeDestination(destinationAddress, sourceAddresses);
  m_logger(DEBUGGING) << "Destination address " << m_currency.accountAddressAsString(destination);

  std::unique_ptr<ITransaction> fusionTransaction;
  size_t transactionSize;
  int round = 0;
  uint64_t transactionAmount;
  do {
    if (round != 0) {
      fusionInputs.pop_back();
      keysInfo.pop_back();
    }

    uint64_t inputsAmount = std::accumulate(fusionInputs.begin(), fusionInputs.end(), static_cast<uint64_t>(0), [] (uint64_t amount, const OutputToTransfer& input) {
      return amount + input.out.amount;
    });

    transactionAmount = inputsAmount;

    ReceiverAmounts decomposedOutputs = decomposeFusionOutputs(destination, inputsAmount);
    assert(decomposedOutputs.amounts.size() <= MAX_FUSION_OUTPUT_COUNT);

    fusionTransaction = makeTransaction(std::vector<ReceiverAmounts>{decomposedOutputs}, keysInfo, "", 0);

    transactionSize = getTransactionSize(*fusionTransaction);

    ++round;
  } while (transactionSize > m_currency.fusionTxMaxSize() && fusionInputs.size() >= m_currency.fusionTxMinInputCount());

  if (fusionInputs.size() < m_currency.fusionTxMinInputCount()) {
    m_logger(ERROR, BRIGHT_RED) << "Unable to create fusion transaction";
    throw std::runtime_error("Unable to create fusion transaction");
  }

  id = validateSaveAndSendTransaction(*fusionTransaction, {}, true, true);
  return id;
}

WalletGreen::ReceiverAmounts WalletGreen::decomposeFusionOutputs(const AccountPublicAddress& address, uint64_t inputsAmount) {
  WalletGreen::ReceiverAmounts outputs;
  outputs.receiver = address;

  decomposeAmount(inputsAmount, 0, outputs.amounts);
  std::sort(outputs.amounts.begin(), outputs.amounts.end());

  return outputs;
}

bool WalletGreen::isFusionTransaction(size_t transactionId) const {
  throwIfNotInitialized();
  throwIfStopped();

  if (m_transactions.size() <= transactionId) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to check transaction: invalid index " << transactionId << ". Number of transactions: " << m_transactions.size();
    throw std::system_error(make_error_code(CryptoNote::error::INDEX_OUT_OF_RANGE));
  }

  auto isFusionIter = m_fusionTxsCache.find(transactionId);
  if (isFusionIter != m_fusionTxsCache.end()) {
    return isFusionIter->second;
  }

  bool result = isFusionTransaction(m_transactions.get<RandomAccessIndex>()[transactionId]);
  m_fusionTxsCache.emplace(transactionId, result);
  return result;
}

bool WalletGreen::isFusionTransaction(const WalletTransaction& walletTx) const {
  if (walletTx.fee != 0) {
    return false;
  }

  uint64_t inputsSum = 0;
  uint64_t outputsSum = 0;
  std::vector<uint64_t> outputsAmounts;
  std::vector<uint64_t> inputsAmounts;
  TransactionInformation txInfo;
  bool gotTx = false;
  const auto& walletsIndex = m_walletsContainer.get<RandomAccessIndex>();
  for (const WalletRecord& wallet : walletsIndex) {
    for (const TransactionOutputInformation& output : wallet.container->getTransactionOutputs(walletTx.hash, ITransfersContainer::IncludeTypeKey | ITransfersContainer::IncludeStateAll)) {
      if (outputsAmounts.size() <= output.outputInTransaction) {
        outputsAmounts.resize(output.outputInTransaction + 1, 0);
      }

      assert(output.amount != 0);
      assert(outputsAmounts[output.outputInTransaction] == 0);
      outputsAmounts[output.outputInTransaction] = output.amount;
      outputsSum += output.amount;
    }

    for (const TransactionOutputInformation& input : wallet.container->getTransactionInputs(walletTx.hash, ITransfersContainer::IncludeTypeKey)) {
      inputsSum += input.amount;
      inputsAmounts.push_back(input.amount);
    }

    if (!gotTx) {
      gotTx = wallet.container->getTransactionInformation(walletTx.hash, txInfo);
    }
  }

  if (!gotTx) {
    return false;
  }

  if (outputsSum != inputsSum || outputsSum != txInfo.totalAmountOut || inputsSum != txInfo.totalAmountIn) {
    return false;
  } else {
    return m_currency.isFusionTransaction(inputsAmounts, outputsAmounts, 0); //size = 0 here because can't get real size of tx in wallet.
  }
}

IFusionManager::EstimateResult WalletGreen::estimate(uint64_t threshold, const std::vector<std::string>& sourceAddresses) const {
  System::EventLock lk(m_readyEvent);

  throwIfNotInitialized();
  throwIfStopped();

  validateSourceAddresses(sourceAddresses);

  IFusionManager::EstimateResult result{0, 0};
  auto walletOuts = sourceAddresses.empty() ? pickWalletsWithMoney() : pickWallets(sourceAddresses);
  std::array<size_t, std::numeric_limits<uint64_t>::digits10 + 1> bucketSizes;
  bucketSizes.fill(0);
  for (size_t walletIndex = 0; walletIndex < walletOuts.size(); ++walletIndex) {
    for (auto& out : walletOuts[walletIndex].outs) {
      uint8_t powerOfTen = 0;
      if (m_currency.isAmountApplicableInFusionTransactionInput(out.amount, threshold, powerOfTen)) {
        assert(powerOfTen < std::numeric_limits<uint64_t>::digits10 + 1);
        bucketSizes[powerOfTen]++;
      }
    }

    result.totalOutputCount += walletOuts[walletIndex].outs.size();
  }

  for (auto bucketSize : bucketSizes) {
    if (bucketSize >= m_currency.fusionTxMinInputCount()) {
      result.fusionReadyCount += bucketSize;
    }
  }

  return result;
}

std::vector<WalletGreen::OutputToTransfer> WalletGreen::pickRandomFusionInputs(const std::vector<std::string>& addresses,
  uint64_t threshold, size_t minInputCount, size_t maxInputCount) {

  std::vector<WalletGreen::OutputToTransfer> allFusionReadyOuts;
  auto walletOuts = addresses.empty() ? pickWalletsWithMoney() : pickWallets(addresses);
  std::array<size_t, std::numeric_limits<uint64_t>::digits10 + 1> bucketSizes;
  bucketSizes.fill(0);
  for (size_t walletIndex = 0; walletIndex < walletOuts.size(); ++walletIndex) {
    for (auto& out : walletOuts[walletIndex].outs) {
      uint8_t powerOfTen = 0;
      if (m_currency.isAmountApplicableInFusionTransactionInput(out.amount, threshold, powerOfTen)) {
        allFusionReadyOuts.push_back({std::move(out), walletOuts[walletIndex].wallet});
        assert(powerOfTen < std::numeric_limits<uint64_t>::digits10 + 1);
        bucketSizes[powerOfTen]++;
      }
    }
  }

  //now, pick the bucket
  std::vector<uint8_t> bucketNumbers(bucketSizes.size());
  std::iota(bucketNumbers.begin(), bucketNumbers.end(), 0);
  std::shuffle(bucketNumbers.begin(), bucketNumbers.end(), std::default_random_engine{Crypto::rand<std::default_random_engine::result_type>()});
  size_t bucketNumberIndex = 0;
  for (; bucketNumberIndex < bucketNumbers.size(); ++bucketNumberIndex) {
    if (bucketSizes[bucketNumbers[bucketNumberIndex]] >= minInputCount) {
      break;
    }
  }
  
  if (bucketNumberIndex == bucketNumbers.size()) {
    return {};
  }

  size_t selectedBucket = bucketNumbers[bucketNumberIndex];
  assert(selectedBucket < std::numeric_limits<uint64_t>::digits10 + 1);
  assert(bucketSizes[selectedBucket] >= minInputCount);
  uint64_t lowerBound = 1;
  for (size_t i = 0; i < selectedBucket; ++i) {
    lowerBound *= 10;
  }
   
  uint64_t upperBound = selectedBucket == std::numeric_limits<uint64_t>::digits10 ? UINT64_MAX : lowerBound * 10;
  std::vector<WalletGreen::OutputToTransfer> selectedOuts;
  selectedOuts.reserve(bucketSizes[selectedBucket]);
  for (size_t outIndex = 0; outIndex < allFusionReadyOuts.size(); ++outIndex) {
    if (allFusionReadyOuts[outIndex].out.amount >= lowerBound && allFusionReadyOuts[outIndex].out.amount < upperBound) {
      selectedOuts.push_back(std::move(allFusionReadyOuts[outIndex]));
    }
  }

  assert(selectedOuts.size() >= minInputCount);

  auto outputsSortingFunction = [](const OutputToTransfer& l, const OutputToTransfer& r) { return l.out.amount < r.out.amount; };
  if (selectedOuts.size() <= maxInputCount) {
    std::sort(selectedOuts.begin(), selectedOuts.end(), outputsSortingFunction);
    return selectedOuts;
  }

  ShuffleGenerator<size_t, Crypto::random_engine<size_t>> generator(selectedOuts.size());
  std::vector<WalletGreen::OutputToTransfer> trimmedSelectedOuts;
  trimmedSelectedOuts.reserve(maxInputCount);
  for (size_t i = 0; i < maxInputCount; ++i) {
    trimmedSelectedOuts.push_back(std::move(selectedOuts[generator()]));
  }

  std::sort(trimmedSelectedOuts.begin(), trimmedSelectedOuts.end(), outputsSortingFunction);
  return trimmedSelectedOuts;  
}

std::vector<TransactionsInBlockInfo> WalletGreen::getTransactionsInBlocks(uint32_t blockIndex, size_t count) const {
  if (count == 0) {
    m_logger(ERROR, BRIGHT_RED) << "Bad argument: block count must be greater than zero";
    throw std::system_error(make_error_code(error::WRONG_PARAMETERS), "blocks count must be greater than zero");
  }

  std::vector<TransactionsInBlockInfo> result;

  if (blockIndex >= m_blockchain.size()) {
    return result;
  }

  auto& blockHeightIndex = m_transactions.get<BlockHeightIndex>();
  uint32_t stopIndex = static_cast<uint32_t>(std::min(m_blockchain.size(), blockIndex + count));

  for (uint32_t height = blockIndex; height < stopIndex; ++height) {
    TransactionsInBlockInfo info;
    info.blockHash = m_blockchain[height];

    auto lowerBound = blockHeightIndex.lower_bound(height);
    auto upperBound = blockHeightIndex.upper_bound(height);
    for (auto it = lowerBound; it != upperBound; ++it) {
      if (it->state != WalletTransactionState::SUCCEEDED) {
        continue;
      }

      WalletTransactionWithTransfers transaction;
      transaction.transaction = *it;

      transaction.transfers = getTransactionTransfers(*it);

      info.transactions.emplace_back(std::move(transaction));
    }

    result.emplace_back(std::move(info));
  }

  return result;
}

Crypto::Hash WalletGreen::getBlockHashByIndex(uint32_t blockIndex) const {
  assert(blockIndex < m_blockchain.size());
  return m_blockchain.get<BlockHeightIndex>()[blockIndex];
}

std::vector<WalletTransfer> WalletGreen::getTransactionTransfers(const WalletTransaction& transaction) const {
  auto& transactionIdIndex = m_transactions.get<RandomAccessIndex>();

  auto it = transactionIdIndex.iterator_to(transaction);
  assert(it != transactionIdIndex.end());

  size_t transactionId = std::distance(transactionIdIndex.begin(), it);
  auto bounds = getTransactionTransfersRange(transactionId);

  std::vector<WalletTransfer> result;
  result.reserve(std::distance(bounds.first, bounds.second));

  for (auto it = bounds.first; it != bounds.second; ++it) {
    result.emplace_back(it->second);
  }

  return result;
}

void WalletGreen::filterOutTransactions(WalletTransactions& transactions, WalletTransfers& transfers, std::function<bool (const WalletTransaction&)>&& pred) const {
  size_t cancelledTransactions = 0;

  transactions.reserve(m_transactions.size());
  transfers.reserve(m_transfers.size());

  auto& index = m_transactions.get<RandomAccessIndex>();
  size_t transferIdx = 0;
  for (size_t i = 0; i < m_transactions.size(); ++i) {
    const WalletTransaction& transaction = index[i];

    if (pred(transaction)) {
      ++cancelledTransactions;

      while (transferIdx < m_transfers.size() && m_transfers[transferIdx].first == i) {
        ++transferIdx;
      }
    } else {
      transactions.emplace_back(transaction);

      while (transferIdx < m_transfers.size() && m_transfers[transferIdx].first == i) {
        transfers.emplace_back(i - cancelledTransactions, m_transfers[transferIdx].second);
        ++transferIdx;
      }
    }
  }
}

void WalletGreen::initBlockchain(const Crypto::PublicKey& viewPublicKey) {
  std::vector<Crypto::Hash> blockchain = m_synchronizer.getViewKeyKnownBlocks(m_viewPublicKey);
  m_blockchain.insert(m_blockchain.end(), blockchain.begin(), blockchain.end());
}

///pre: changeDestinationAddress belongs to current container
///pre: source address belongs to current container
CryptoNote::AccountPublicAddress WalletGreen::getChangeDestination(const std::string& changeDestinationAddress, const std::vector<std::string>& sourceAddresses) const {
  if (!changeDestinationAddress.empty()) {
    return parseAccountAddressString(changeDestinationAddress);
  }

  if (m_walletsContainer.size() == 1) {
    return AccountPublicAddress { m_walletsContainer.get<RandomAccessIndex>()[0].spendPublicKey, m_viewPublicKey };
  }

  assert(sourceAddresses.size() == 1 && isMyAddress(sourceAddresses[0]));
  return parseAccountAddressString(sourceAddresses[0]);
}

bool WalletGreen::isMyAddress(const std::string& addressString) const {
  CryptoNote::AccountPublicAddress address = parseAccountAddressString(addressString);
  return m_viewPublicKey == address.viewPublicKey && m_walletsContainer.get<KeysIndex>().count(address.spendPublicKey) != 0;
}

void WalletGreen::deleteContainerFromUnlockTransactionJobs(const ITransfersContainer* container) {
  for (auto it = m_unlockTransactionsJob.begin(); it != m_unlockTransactionsJob.end();) {
    if (it->container == container) {
      it = m_unlockTransactionsJob.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<size_t> WalletGreen::deleteTransfersForAddress(const std::string& address, std::vector<size_t>& deletedTransactions) {
  assert(!address.empty());

  int64_t deletedInputs = 0;
  int64_t deletedOutputs = 0;

  int64_t unknownInputs = 0;

  bool transfersLeft = false;
  size_t firstTransactionTransfer = 0;

  std::vector<size_t> updatedTransactions;

  for (size_t i = 0; i < m_transfers.size(); ++i) {
    WalletTransfer& transfer = m_transfers[i].second;

    if (transfer.address == address) {
      if (transfer.amount >= 0) {
        deletedOutputs += transfer.amount;
      } else {
        deletedInputs += transfer.amount;
        transfer.address = "";
      }
    } else if (transfer.address.empty()) {
      if (transfer.amount < 0) {
        unknownInputs += transfer.amount;
      }
    } else if (isMyAddress(transfer.address)) {
      transfersLeft = true;
    }

    size_t transactionId = m_transfers[i].first;
    if ((i == m_transfers.size() - 1) || (transactionId != m_transfers[i + 1].first)) {
      //the last transfer for current transaction

      size_t transfersBeforeMerge = m_transfers.size();
      if (deletedInputs != 0) {
        adjustTransfer(transactionId, firstTransactionTransfer, "", deletedInputs + unknownInputs);
      }

      assert(transfersBeforeMerge >= m_transfers.size());
      i -= transfersBeforeMerge - m_transfers.size();

      auto& randomIndex = m_transactions.get<RandomAccessIndex>();

      randomIndex.modify(std::next(randomIndex.begin(), transactionId), [this, transactionId, transfersLeft, deletedInputs, deletedOutputs] (WalletTransaction& transaction) {
        transaction.totalAmount -= deletedInputs + deletedOutputs;

        if (!transfersLeft) {
          transaction.state = WalletTransactionState::DELETED;
          transaction.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
          m_logger(DEBUGGING) << "Transaction state changed, ID " << transactionId <<
            ", hash " << transaction.hash <<
            ", new state " << transaction.state;
        }
      });

      if (!transfersLeft) {
        deletedTransactions.push_back(transactionId);
      }

      if (deletedInputs != 0 || deletedOutputs != 0) {
        updatedTransactions.push_back(transactionId);
      }

      //reset values for next transaction
      deletedInputs = 0;
      deletedOutputs = 0;
      unknownInputs = 0;
      transfersLeft = false;
      firstTransactionTransfer = i + 1;
    }
  }

  return updatedTransactions;
}

void WalletGreen::deleteFromUncommitedTransactions(const std::vector<size_t>& deletedTransactions) {
  for (auto transactionId: deletedTransactions) {
    m_uncommitedTransactions.erase(transactionId);
  }
}

} //namespace CryptoNote
