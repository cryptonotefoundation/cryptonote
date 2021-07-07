// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
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

#include "WalletLegacy.h"

#include <algorithm>
#include <numeric>
#include <crypto/random.h>
#include <set>
#include <tuple>
#include <utility>
#include <string.h>
#include <time.h>

#include "crypto/crypto.h"
#include "Common/Base58.h"
#include "Common/ShuffleGenerator.h"
#include "Logging/ConsoleLogger.h"
#include "WalletLegacy/WalletHelper.h"
#include "WalletLegacy/WalletLegacySerialization.h"
#include "WalletLegacy/WalletLegacySerializer.h"
#include "WalletLegacy/WalletUtils.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "Mnemonics/electrum-words.h"

extern "C"
{
#include "crypto/keccak.h"
#include "crypto/crypto-ops.h"
}

using namespace Crypto;

namespace {

const uint64_t ACCOUNT_CREATE_TIME_ACCURACY = 24 * 60 * 60;

void throwNotDefined() {
  throw std::runtime_error("The behavior is not defined!");
}

class ContextCounterHolder
{
public:
  ContextCounterHolder(CryptoNote::WalletAsyncContextCounter& shutdowner) : m_shutdowner(shutdowner) {}
  ~ContextCounterHolder() { m_shutdowner.delAsyncContext(); }

private:
  CryptoNote::WalletAsyncContextCounter& m_shutdowner;
};

template <typename F>
void runAtomic(std::mutex& mutex, F f) {
  std::unique_lock<std::mutex> lock(mutex);
  f();
}

class InitWaiter : public CryptoNote::IWalletLegacyObserver {
public:
  InitWaiter() : future(promise.get_future()) {}

  virtual void initCompleted(std::error_code result) override {
    promise.set_value(result);
  }

  std::error_code waitInit() {
    return future.get();
  }
private:
  std::promise<std::error_code> promise;
  std::future<std::error_code> future;
};


class SaveWaiter : public CryptoNote::IWalletLegacyObserver {
public:
  SaveWaiter() : future(promise.get_future()) {}

  virtual void saveCompleted(std::error_code result) override {
    promise.set_value(result);
  }

  std::error_code waitSave() {
    return future.get();
  }

private:
  std::promise<std::error_code> promise;
  std::future<std::error_code> future;
};

} //namespace

using namespace Logging;

namespace CryptoNote {

class SyncStarter : public CryptoNote::IWalletLegacyObserver {
public:
  SyncStarter(BlockchainSynchronizer& sync) : m_sync(sync) {}
  virtual ~SyncStarter() {}

  virtual void initCompleted(std::error_code result) override {
    if (!result) {
      m_sync.start();
    }
  }

  BlockchainSynchronizer& m_sync;
};

WalletLegacy::WalletLegacy(const CryptoNote::Currency& currency, INode& node, Logging::ILogger& log) :
  m_state(NOT_INITIALIZED),
  m_currency(currency),
  m_node(node),
  m_logger(log, "WalletLegacy"),
  m_isStopping(false),
  m_lastNotifiedActualBalance(0),
  m_lastNotifiedPendingBalance(0),
  m_lastNotifiedUnmixableBalance(0),
  m_blockchainSync(node, m_logger.getLogger(), currency.genesisBlockHash()),
  m_transfersSync(currency, m_logger.getLogger(), m_blockchainSync, node),
  m_transferDetails(nullptr),
  m_transactionsCache(m_currency.mempoolTxLiveTime()),
  m_sender(nullptr),
  m_onInitSyncStarter(new SyncStarter(m_blockchainSync))
{
  addObserver(m_onInitSyncStarter.get());
}

WalletLegacy::~WalletLegacy() {
  removeObserver(m_onInitSyncStarter.get());

  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    if (m_state != NOT_INITIALIZED) {
      m_sender->stop();
      m_isStopping = true;
    }
  }

  m_blockchainSync.removeObserver(this);
  m_blockchainSync.stop();
  m_asyncContextCounter.waitAsyncContextsFinish();
  m_sender.reset();
}

void WalletLegacy::addObserver(IWalletLegacyObserver* observer) {
  m_observerManager.add(observer);
}

void WalletLegacy::removeObserver(IWalletLegacyObserver* observer) {
  m_observerManager.remove(observer);
}

void WalletLegacy::initAndGenerate(const std::string& password) {
  {
    std::unique_lock<std::mutex> stateLock(m_cacheMutex);

    if (m_state != NOT_INITIALIZED) {
      throw std::system_error(make_error_code(error::ALREADY_INITIALIZED));
    }

    m_account.generate();
	//m_account.generateDeterministic();
    m_password = password;

    initSync();
  }

  m_observerManager.notify(&IWalletLegacyObserver::initCompleted, std::error_code());
}

void WalletLegacy::initAndGenerateDeterministic(const std::string& password) {
  {
    std::unique_lock<std::mutex> stateLock(m_cacheMutex);

    if (m_state != NOT_INITIALIZED) {
      throw std::system_error(make_error_code(error::ALREADY_INITIALIZED));
    }

    m_account.generateDeterministic();
    m_password = password;

    initSync();
  }

  m_observerManager.notify(&IWalletLegacyObserver::initCompleted, std::error_code());
}

void WalletLegacy::initWithKeys(const AccountKeys& accountKeys, const std::string& password) {
  {
    std::unique_lock<std::mutex> stateLock(m_cacheMutex);

    if (m_state != NOT_INITIALIZED) {
      throw std::system_error(make_error_code(error::ALREADY_INITIALIZED));
    }

    m_account.setAccountKeys(accountKeys);
    m_account.set_createtime(ACCOUNT_CREATE_TIME_ACCURACY);
    m_password = password;

    initSync();
  }

  m_observerManager.notify(&IWalletLegacyObserver::initCompleted, std::error_code());
}

uint64_t WalletLegacy::getBlockTimestamp(const uint32_t blockHeight) {
  uint64_t timestamp = 0;

  auto getBlockTimestampCompleted = std::promise<std::error_code>();
  auto getBlockTimestampWaitFuture = getBlockTimestampCompleted.get_future();

  m_node.getBlockTimestamp(std::move(blockHeight), std::ref(timestamp),
    [&getBlockTimestampCompleted](std::error_code ec) {
    auto detachedPromise = std::move(getBlockTimestampCompleted);
    detachedPromise.set_value(ec);
  });

  std::error_code ec = getBlockTimestampWaitFuture.get();

  if (ec) {
    m_logger(ERROR) << "Failed to get block timestamp: " << ec << ", " << ec.message();
  }

  return timestamp;
}

uint64_t getCurrentTimestampAdjusted() {
  /* Get the current time as a unix timestamp */
  std::time_t time = std::time(nullptr);

  /* Take the amount of time a block can potentially be in the past/future */
  std::initializer_list<uint64_t> limits = {
    CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT,
    CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1
  };

  /* Get the largest adjustment possible */
  uint64_t adjust = std::max(limits);

  /* Take the earliest timestamp that will include all possible blocks */
  return time - adjust;
}

uint64_t WalletLegacy::scanHeightToTimestamp(const uint32_t scanHeight) {
  if (scanHeight == 0) {
    return 0;
  }

  /* Get the block timestamp from the node if the node has it */
  uint64_t timestamp = getBlockTimestamp(scanHeight);

  if (timestamp != 0) {
    return timestamp;
  }

  /* Get the amount of seconds since the blockchain launched */
  uint64_t secondsSinceLaunch = scanHeight * CryptoNote::parameters::DIFFICULTY_TARGET;

  /* Add a bit of a buffer in case of difficulty weirdness, blocks coming
     out too fast */
  secondsSinceLaunch = static_cast<uint64_t>(secondsSinceLaunch * 0.95);

  /* Get the genesis block timestamp and add the time since launch */
  timestamp = UINT64_C(1464595534) + secondsSinceLaunch;

  /* Timestamp in the future */
  if (timestamp >= static_cast<uint64_t>(std::time(nullptr))) {
    return getCurrentTimestampAdjusted();
  }

  return timestamp;
}

void WalletLegacy::initWithKeys(const AccountKeys& accountKeys, const std::string& password, const uint32_t scanHeight) {
  {
    std::unique_lock<std::mutex> stateLock(m_cacheMutex);

    if (m_state != NOT_INITIALIZED) {
      throw std::system_error(make_error_code(error::ALREADY_INITIALIZED));
    }

    m_account.setAccountKeys(accountKeys);
    uint64_t newTimestamp = scanHeightToTimestamp(scanHeight);
    m_account.set_createtime(newTimestamp);
    m_password = password;

    initSync();
  }

  m_observerManager.notify(&IWalletLegacyObserver::initCompleted, std::error_code());
}

void WalletLegacy::initAndLoad(std::istream& source, const std::string& password) {
  std::unique_lock<std::mutex> stateLock(m_cacheMutex);

  if (m_state != NOT_INITIALIZED) {
    throw std::system_error(make_error_code(error::ALREADY_INITIALIZED));
  }

  m_password = password;
  m_state = LOADING;
      
  m_asyncContextCounter.addAsyncContext();
  std::thread loader(&WalletLegacy::doLoad, this, std::ref(source));
  loader.detach();
}

void WalletLegacy::initSync() {
  AccountSubscription sub;
  sub.keys = reinterpret_cast<const AccountKeys&>(m_account.getAccountKeys());
  sub.transactionSpendableAge = CryptoNote::parameters::CRYPTONOTE_TX_SPENDABLE_AGE;
  sub.syncStart.height = 0;
  sub.syncStart.timestamp = std::max(m_account.get_createtime(), ACCOUNT_CREATE_TIME_ACCURACY) - ACCOUNT_CREATE_TIME_ACCURACY;
  
  auto& subObject = m_transfersSync.addSubscription(sub);
  m_transferDetails = &subObject.getContainer();
  subObject.addObserver(this);

  m_sender.reset(new WalletTransactionSender(m_currency, m_transactionsCache, m_account.getAccountKeys(), *m_transferDetails, m_node));
  m_state = INITIALIZED;
  
  m_blockchainSync.addObserver(this);
}

void WalletLegacy::doLoad(std::istream& source) {
  ContextCounterHolder counterHolder(m_asyncContextCounter);
  try {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    
    std::string cache;
    WalletLegacySerializer serializer(m_account, m_transactionsCache);
    serializer.deserialize(source, m_password, cache);
      
    initSync();

    try {
      if (!cache.empty()) {
        std::stringstream stream(cache);
        m_transfersSync.load(stream);
      }
    } catch (const std::exception&) {
      // ignore cache loading errors
    }

	// Read all output keys cache
    std::vector<TransactionOutputInformation> allTransfers;
    m_transferDetails->getOutputs(allTransfers, ITransfersContainer::IncludeAll);
    m_logger(Logging::INFO) << "Loaded " + std::to_string(allTransfers.size()) + " known transfer(s)";
    for (auto& o : allTransfers) {
      if (o.type != TransactionTypes::OutputType::Invalid) {
        m_transfersSync.addPublicKeysSeen(m_account.getAccountKeys().address, o.transactionHash, o.outputKey);
      }
    }

  } catch (std::system_error& e) {
    runAtomic(m_cacheMutex, [this] () {this->m_state = WalletLegacy::NOT_INITIALIZED;} );
    m_observerManager.notify(&IWalletLegacyObserver::initCompleted, e.code());
    return;
  } catch (std::exception&) {
    runAtomic(m_cacheMutex, [this] () {this->m_state = WalletLegacy::NOT_INITIALIZED;} );
    m_observerManager.notify(&IWalletLegacyObserver::initCompleted, make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR));
    return;
  }

  m_observerManager.notify(&IWalletLegacyObserver::initCompleted, std::error_code());
}

bool WalletLegacy::tryLoadWallet(std::istream& source, const std::string& password) {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  WalletLegacySerializer serializer(m_account, m_transactionsCache);
  return serializer.deserialize(source, password);
}

void WalletLegacy::shutdown() {
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);

    if (m_isStopping)
      throwNotDefined();

    m_isStopping = true;

    if (m_state != INITIALIZED)
      throwNotDefined();

    m_sender->stop();
  }

  m_blockchainSync.removeObserver(this);
  m_blockchainSync.stop();
  m_asyncContextCounter.waitAsyncContextsFinish();

  m_sender.reset();
   
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    m_isStopping = false;
    m_state = NOT_INITIALIZED;

    const auto& accountAddress = m_account.getAccountKeys().address;
    auto subObject = m_transfersSync.getSubscription(accountAddress);
    assert(subObject != nullptr);
    subObject->removeObserver(this);
    m_transfersSync.removeSubscription(accountAddress);
    m_transferDetails = nullptr;

    m_transactionsCache.reset();
    m_lastNotifiedActualBalance = 0;
    m_lastNotifiedPendingBalance = 0;
    m_lastNotifiedUnmixableBalance = 0;
  }
}

void WalletLegacy::reset() {
  try {
    std::error_code saveError;
    std::stringstream ss;
    {
      SaveWaiter saveWaiter;
      WalletHelper::IWalletRemoveObserverGuard saveGuarantee(*this, saveWaiter);
      save(ss, false, false);
      saveError = saveWaiter.waitSave();
    }

    if (!saveError) {
      shutdown();
      InitWaiter initWaiter;
      WalletHelper::IWalletRemoveObserverGuard initGuarantee(*this, initWaiter);
      initAndLoad(ss, m_password);
      initWaiter.waitInit();
    }
  } catch (std::exception& e) {
    m_logger(Logging::ERROR) << "exception in reset: " << e.what();
  }
}

void WalletLegacy::save(std::ostream& destination, bool saveDetailed, bool saveCache) {
  if(m_isStopping) {
    m_observerManager.notify(&IWalletLegacyObserver::saveCompleted, make_error_code(CryptoNote::error::OPERATION_CANCELLED));
    return;
  }

  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);

    throwIf(m_state != INITIALIZED, CryptoNote::error::WRONG_STATE);

    m_state = SAVING;
  }

  m_asyncContextCounter.addAsyncContext();
  std::thread saver(&WalletLegacy::doSave, this, std::ref(destination), saveDetailed, saveCache);
  saver.detach();
}

void WalletLegacy::doSave(std::ostream& destination, bool saveDetailed, bool saveCache) {
  ContextCounterHolder counterHolder(m_asyncContextCounter);

  try {
    m_blockchainSync.stop();
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    
    WalletLegacySerializer serializer(m_account, m_transactionsCache);
    std::string cache;

    if (saveCache) {
      std::stringstream stream;
      m_transfersSync.save(stream);
      cache = stream.str();
    }

    serializer.serialize(destination, m_password, saveDetailed, cache);

    m_state = INITIALIZED;
    m_blockchainSync.start(); //XXX: start can throw. what to do in this case?
    }
  catch (std::system_error& e) {
    runAtomic(m_cacheMutex, [this] () {this->m_state = WalletLegacy::INITIALIZED;} );
    m_observerManager.notify(&IWalletLegacyObserver::saveCompleted, e.code());
    return;
  }
  catch (std::exception&) {
    runAtomic(m_cacheMutex, [this] () {this->m_state = WalletLegacy::INITIALIZED;} );
    m_observerManager.notify(&IWalletLegacyObserver::saveCompleted, make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR));
    return;
  }

  m_observerManager.notify(&IWalletLegacyObserver::saveCompleted, std::error_code());
}

std::error_code WalletLegacy::changePassword(const std::string& oldPassword, const std::string& newPassword) {
  std::unique_lock<std::mutex> passLock(m_cacheMutex);

  throwIfNotInitialised();

  if (m_password.compare(oldPassword))
    return make_error_code(CryptoNote::error::WRONG_PASSWORD);

  //we don't let the user to change the password while saving
  m_password = newPassword;

  return std::error_code();
}

bool WalletLegacy::getSeed(std::string& electrum_words)
{
	std::string lang = "English";
	Crypto::ElectrumWords::bytes_to_words(m_account.getAccountKeys().spendSecretKey, electrum_words, lang);

	Crypto::SecretKey second;
	keccak((uint8_t *)&m_account.getAccountKeys().spendSecretKey, sizeof(Crypto::SecretKey), (uint8_t *)&second, sizeof(Crypto::SecretKey));

	sc_reduce32((uint8_t *)&second);

	return memcmp(second.data, m_account.getAccountKeys().viewSecretKey.data, sizeof(Crypto::SecretKey)) == 0;
}

std::string WalletLegacy::getAddress() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_currency.accountAddressAsString(m_account);
}

std::string WalletLegacy::sign_message(const std::string &message) {
  return CryptoNote::signMessage(message, m_account.getAccountKeys());
}

bool WalletLegacy::verify_message(const std::string &message, const CryptoNote::AccountPublicAddress &address, const std::string &signature) {
  return CryptoNote::verifyMessage(message, address, signature, m_logger.getLogger());
}

std::vector<Payments> WalletLegacy::getTransactionsByPaymentIds(const std::vector<PaymentId>& paymentIds) const {
  return m_transactionsCache.getTransactionsByPaymentIds(paymentIds);
}

uint64_t WalletLegacy::actualBalance() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transferDetails->balance(ITransfersContainer::IncludeKeyUnlocked) -
    m_transactionsCache.unconfrimedOutsAmount();
}

uint64_t WalletLegacy::pendingBalance() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  uint64_t change = m_transactionsCache.unconfrimedOutsAmount() - m_transactionsCache.unconfirmedTransactionsAmount();
  return m_transferDetails->balance(ITransfersContainer::IncludeKeyNotUnlocked) + change;
}

uint64_t WalletLegacy::unmixableBalance() {
	std::unique_lock<std::mutex> lock(m_cacheMutex);
	throwIfNotInitialised();

	std::vector<TransactionOutputInformation> outputs;
	m_transferDetails->getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);

	uint64_t money = 0;
	
	for (size_t i = 0; i < outputs.size(); ++i) {
		const auto& out = outputs[i];
		if (!m_transactionsCache.isUsed(out)) {
			if (!is_valid_decomposed_amount(out.amount)) {
				money += out.amount;
			}
		}
	}

	return money;
}

size_t WalletLegacy::getTransactionCount() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.getTransactionCount();
}

size_t WalletLegacy::getTransferCount() {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.getTransferCount();
}

TransactionId WalletLegacy::findTransactionByTransferId(TransferId transferId) {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.findTransactionByTransferId(transferId);
}

bool WalletLegacy::getTransaction(TransactionId transactionId, WalletLegacyTransaction& transaction) {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.getTransaction(transactionId, transaction);
}

bool WalletLegacy::getTransfer(TransferId transferId, WalletLegacyTransfer& transfer) {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
  throwIfNotInitialised();

  return m_transactionsCache.getTransfer(transferId, transfer);
}

size_t WalletLegacy::getUnlockedOutputsCount() {
  std::vector<TransactionOutputInformation> outputs;
  m_transferDetails->getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);
  return outputs.size();
}

std::vector<TransactionOutputInformation> WalletLegacy::getOutputs() {
  std::vector<TransactionOutputInformation> outputs;
  m_transferDetails->getOutputs(outputs, ITransfersContainer::IncludeAll);
  return outputs;
}

std::vector<TransactionOutputInformation> WalletLegacy::getLockedOutputs() {
  std::vector<TransactionOutputInformation> outputs;
  m_transferDetails->getOutputs(outputs, ITransfersContainer::IncludeAllLocked);
  return outputs;
}

std::vector<TransactionOutputInformation> WalletLegacy::getUnlockedOutputs() {
  std::vector<TransactionOutputInformation> outputs;
  m_transferDetails->getOutputs(outputs, ITransfersContainer::IncludeAllUnlocked);
  return outputs;
}

std::vector<TransactionSpentOutputInformation> WalletLegacy::getSpentOutputs() {
  return m_transferDetails->getSpentOutputs();
}

size_t WalletLegacy::estimateFusion(const uint64_t& threshold) {
  size_t fusionReadyCount = 0;
  std::vector<TransactionOutputInformation> outputs;
  m_transferDetails->getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);
  std::array<size_t, std::numeric_limits<uint64_t>::digits10 + 1> bucketSizes;
  bucketSizes.fill(0);
  for (auto& out : outputs) {
    uint8_t powerOfTen = 0;
	if (m_currency.isAmountApplicableInFusionTransactionInput(out.amount, threshold, powerOfTen, m_node.getLastKnownBlockHeight())) {
      assert(powerOfTen < std::numeric_limits<uint64_t>::digits10 + 1);
      bucketSizes[powerOfTen]++;
	}
  }
  for (auto bucketSize : bucketSizes) {
    if (bucketSize >= m_currency.fusionTxMinInputCount()) {
      fusionReadyCount += bucketSize;
    }
  }
  return fusionReadyCount;
}

std::list<TransactionOutputInformation> WalletLegacy::selectFusionTransfersToSend(uint64_t threshold, size_t minInputCount, size_t maxInputCount) {
  std::list<TransactionOutputInformation> selectedOutputs;
  std::vector<TransactionOutputInformation> outputs;
  std::vector<TransactionOutputInformation> allFusionReadyOuts;
  m_transferDetails->getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);
  std::array<size_t, std::numeric_limits<uint64_t>::digits10 + 1> bucketSizes;
  bucketSizes.fill(0);
  for (auto& out : outputs) {
    uint8_t powerOfTen = 0;
    if (m_currency.isAmountApplicableInFusionTransactionInput(out.amount, threshold, powerOfTen, m_node.getLastKnownBlockHeight())) {
      allFusionReadyOuts.push_back(std::move(out));
      assert(powerOfTen < std::numeric_limits<uint64_t>::digits10 + 1);
      bucketSizes[powerOfTen]++;
    }
  }

  //now, pick the bucket
  std::vector<uint8_t> bucketNumbers(bucketSizes.size());
  std::iota(bucketNumbers.begin(), bucketNumbers.end(), 0);
  std::shuffle(bucketNumbers.begin(), bucketNumbers.end(), Random::generator());
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
  std::vector<TransactionOutputInformation> selectedOuts;
  selectedOuts.reserve(bucketSizes[selectedBucket]);
  for (size_t outIndex = 0; outIndex < allFusionReadyOuts.size(); ++outIndex) {
	  if (allFusionReadyOuts[outIndex].amount >= lowerBound && allFusionReadyOuts[outIndex].amount < upperBound) {
		  selectedOuts.push_back(std::move(allFusionReadyOuts[outIndex]));
	  }
  }

  assert(selectedOuts.size() >= minInputCount);

  auto outputsSortingFunction = [](const TransactionOutputInformation& l, const TransactionOutputInformation& r) { return l.amount < r.amount; };
  if (selectedOuts.size() <= maxInputCount) {
	  std::sort(selectedOuts.begin(), selectedOuts.end(), outputsSortingFunction);
	  std::copy(selectedOuts.begin(), selectedOuts.end(), std::back_inserter(selectedOutputs));
	  return selectedOutputs;
  }

  ShuffleGenerator<size_t> generator(selectedOuts.size());
  std::vector<TransactionOutputInformation> trimmedSelectedOuts;
  trimmedSelectedOuts.reserve(maxInputCount);
  for (size_t i = 0; i < maxInputCount; ++i) {
	  trimmedSelectedOuts.push_back(std::move(selectedOuts[generator()]));
  }

  std::sort(trimmedSelectedOuts.begin(), trimmedSelectedOuts.end(), outputsSortingFunction);
  std::copy(trimmedSelectedOuts.begin(), trimmedSelectedOuts.end(), std::back_inserter(selectedOutputs));
  return selectedOutputs;
}

TransactionId WalletLegacy::sendTransaction(const WalletLegacyTransfer& transfer, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {
  std::vector<WalletLegacyTransfer> transfers;
  transfers.push_back(transfer);
  throwIfNotInitialised();

  return sendTransaction(transfers, fee, extra, mixIn, unlockTimestamp);
}

TransactionId WalletLegacy::sendTransaction(const std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {
  TransactionId txId = 0;
  std::shared_ptr<WalletRequest> request;
  std::deque<std::shared_ptr<WalletLegacyEvent>> events;
  throwIfNotInitialised();

  std::list<CryptoNote::TransactionOutputInformation> _selectedOuts = {};

  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    request = m_sender->makeSendRequest(txId, events, transfers, _selectedOuts, fee, extra, mixIn, unlockTimestamp);
  }

  notifyClients(events);

  if (request) {
    m_asyncContextCounter.addAsyncContext();
    request->perform(m_node, std::bind(&WalletLegacy::sendTransactionCallback, this, std::placeholders::_1, std::placeholders::_2));
  }

  return txId;
}

TransactionId WalletLegacy::sendTransaction(const std::vector<WalletLegacyTransfer>& transfers, const std::list<TransactionOutputInformation>& selectedOuts, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {
  TransactionId txId = 0;
  std::shared_ptr<WalletRequest> request;
  std::deque<std::shared_ptr<WalletLegacyEvent>> events;
  throwIfNotInitialised();

  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    request = m_sender->makeSendRequest(txId, events, transfers, selectedOuts, fee, extra, mixIn, unlockTimestamp);
  }

  notifyClients(events);

  if (request) {
    m_asyncContextCounter.addAsyncContext();
    request->perform(m_node, std::bind(&WalletLegacy::sendTransactionCallback, this, std::placeholders::_1, std::placeholders::_2));
  }

  return txId;
}

std::string WalletLegacy::prepareRawTransaction(TransactionId& transactionId, const std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {
  std::deque<std::shared_ptr<WalletLegacyEvent>> events;
  throwIfNotInitialised();

  std::list<CryptoNote::TransactionOutputInformation> _selectedOuts = {};

  std::string tx_as_hex;

  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    tx_as_hex = m_sender->makeRawTransaction(transactionId, events, transfers, _selectedOuts, fee, extra, mixIn, unlockTimestamp);
  }

  notifyClients(events);

  return tx_as_hex;
}

std::string WalletLegacy::prepareRawTransaction(TransactionId& transactionId, const std::vector<WalletLegacyTransfer>& transfers, const std::list<CryptoNote::TransactionOutputInformation>& selectedOuts, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {
  std::deque<std::shared_ptr<WalletLegacyEvent>> events;
  throwIfNotInitialised();

  std::string tx_as_hex;

  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    tx_as_hex = m_sender->makeRawTransaction(transactionId, events, transfers, selectedOuts, fee, extra, mixIn, unlockTimestamp);
  }

  notifyClients(events);

  return tx_as_hex;
}

std::string WalletLegacy::prepareRawTransaction(TransactionId& transactionId, const WalletLegacyTransfer& transfer, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {
  std::vector<WalletLegacyTransfer> transfers;
  transfers.push_back(transfer);
  throwIfNotInitialised();

  return prepareRawTransaction(transactionId, transfers, fee, extra, mixIn, unlockTimestamp);
}

TransactionId WalletLegacy::sendFusionTransaction(const std::list<TransactionOutputInformation>& fusionInputs, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {
	TransactionId txId = 0;
	std::shared_ptr<WalletRequest> request;
	std::deque<std::shared_ptr<WalletLegacyEvent>> events;
	throwIfNotInitialised();
	std::vector<WalletLegacyTransfer> transfers;
	WalletLegacyTransfer destination;
	destination.amount = 0;
	for (auto& out : fusionInputs) {
		destination.amount += out.amount;
	}
	destination.address = getAddress();
	transfers.push_back(destination);

	{
		std::unique_lock<std::mutex> lock(m_cacheMutex);
		request = m_sender->makeSendFusionRequest(txId, events, transfers, fusionInputs, fee, extra, mixIn, unlockTimestamp);
	}

	notifyClients(events);

	if (request) {
		m_asyncContextCounter.addAsyncContext();
		request->perform(m_node, std::bind(&WalletLegacy::sendTransactionCallback, this, std::placeholders::_1, std::placeholders::_2));
	}

	return txId;
}


void WalletLegacy::sendTransactionCallback(WalletRequest::Callback callback, std::error_code ec) {
  ContextCounterHolder counterHolder(m_asyncContextCounter);
  std::deque<std::shared_ptr<WalletLegacyEvent> > events;

  boost::optional<std::shared_ptr<WalletRequest> > nextRequest;
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    callback(events, nextRequest, ec);
  }

  notifyClients(events);

  if (nextRequest) {
    m_asyncContextCounter.addAsyncContext();
    (*nextRequest)->perform(m_node, std::bind(&WalletLegacy::synchronizationCallback, this, std::placeholders::_1, std::placeholders::_2));
  }
}

void WalletLegacy::synchronizationCallback(WalletRequest::Callback callback, std::error_code ec) {
  ContextCounterHolder counterHolder(m_asyncContextCounter);

  std::deque<std::shared_ptr<WalletLegacyEvent> > events;
  boost::optional<std::shared_ptr<WalletRequest> > nextRequest;
  {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    callback(events, nextRequest, ec);
  }

  notifyClients(events);

  if (nextRequest) {
    m_asyncContextCounter.addAsyncContext();
    (*nextRequest)->perform(m_node, std::bind(&WalletLegacy::synchronizationCallback, this, std::placeholders::_1, std::placeholders::_2));
  }
}

std::error_code WalletLegacy::cancelTransaction(size_t transactionId) {
  return make_error_code(CryptoNote::error::TX_CANCEL_IMPOSSIBLE);
}

void WalletLegacy::synchronizationProgressUpdated(uint32_t current, uint32_t total) {
  auto deletedTransactions = deleteOutdatedUnconfirmedTransactions();

  // forward notification
  m_observerManager.notify(&IWalletLegacyObserver::synchronizationProgressUpdated, current, total);

  for (auto transactionId: deletedTransactions) {
    m_observerManager.notify(&IWalletLegacyObserver::transactionUpdated, transactionId);
  }

  // check if balance has changed and notify client
  notifyIfBalanceChanged();
}

void WalletLegacy::synchronizationCompleted(std::error_code result) {
  if (result != std::make_error_code(std::errc::interrupted)) {
    m_observerManager.notify(&IWalletLegacyObserver::synchronizationCompleted, result);
  }

  if (result) {
    return;
  }

  auto deletedTransactions = deleteOutdatedUnconfirmedTransactions();
  std::for_each(deletedTransactions.begin(), deletedTransactions.end(), [&] (TransactionId transactionId) {
    m_observerManager.notify(&IWalletLegacyObserver::transactionUpdated, transactionId);
  });

  notifyIfBalanceChanged();
}

void WalletLegacy::onTransactionUpdated(ITransfersSubscription* object, const Hash& transactionHash) {
  std::shared_ptr<WalletLegacyEvent> event;

  TransactionInformation txInfo;
  uint64_t amountIn;
  uint64_t amountOut;
  if (m_transferDetails->getTransactionInformation(transactionHash, txInfo, &amountIn, &amountOut)) {
    std::unique_lock<std::mutex> lock(m_cacheMutex);
    event = m_transactionsCache.onTransactionUpdated(txInfo, static_cast<int64_t>(amountOut) - static_cast<int64_t>(amountIn));
  }

  if (event.get()) {
    event->notify(m_observerManager);
  }
}

void WalletLegacy::onTransactionDeleted(ITransfersSubscription* object, const Hash& transactionHash) {
  std::shared_ptr<WalletLegacyEvent> event;

  {
  std::unique_lock<std::mutex> lock(m_cacheMutex);
    event = m_transactionsCache.onTransactionDeleted(transactionHash);
  }

  if (event.get()) {
    event->notify(m_observerManager);
  }
}

void WalletLegacy::throwIfNotInitialised() {
  if (m_state == NOT_INITIALIZED || m_state == LOADING) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
  assert(m_transferDetails);
}

void WalletLegacy::notifyClients(std::deque<std::shared_ptr<WalletLegacyEvent> >& events) {
  while (!events.empty()) {
    std::shared_ptr<WalletLegacyEvent> event = events.front();
    event->notify(m_observerManager);
    events.pop_front();
  }
}

void WalletLegacy::notifyIfBalanceChanged() {
  auto actual = actualBalance();
  auto prevActual = m_lastNotifiedActualBalance.exchange(actual);

  if (prevActual != actual) {
    m_observerManager.notify(&IWalletLegacyObserver::actualBalanceUpdated, actual);
  }

  auto pending = pendingBalance();
  auto prevPending = m_lastNotifiedPendingBalance.exchange(pending);

  if (prevPending != pending) {
    m_observerManager.notify(&IWalletLegacyObserver::pendingBalanceUpdated, pending);
  }

  auto unmixable = unmixableBalance();
  auto prevUnmixable = m_lastNotifiedUnmixableBalance.exchange(unmixable);

  if (prevUnmixable != unmixable) {
    m_observerManager.notify(&IWalletLegacyObserver::unmixableBalanceUpdated, unmixable);
  }

}

void WalletLegacy::getAccountKeys(AccountKeys& keys) {
  if (m_state == NOT_INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  keys = m_account.getAccountKeys();
}

bool WalletLegacy::isTrackingWallet() {
  AccountKeys keys;
  getAccountKeys(keys);
  
  return keys.spendSecretKey == boost::value_initialized<Crypto::SecretKey>();
}

std::vector<TransactionId> WalletLegacy::deleteOutdatedUnconfirmedTransactions() {
  std::lock_guard<std::mutex> lock(m_cacheMutex);
  return m_transactionsCache.deleteOutdatedTransactions();
}

/* Returns either deterministic key or stored in wallet cache
 * (returns null key if is absent in cache).
 * In order to generate deterministic key raw transaction is 
 * requested from Node.
 */
Crypto::SecretKey WalletLegacy::getTxKey(Crypto::Hash& txid) {
  TransactionId ti = m_transactionsCache.findTransactionByHash(txid);
  WalletLegacyTransaction transaction;
  getTransaction(ti, transaction);
  if (transaction.secretKey && NULL_SECRET_KEY != reinterpret_cast<const Crypto::SecretKey&>(transaction.secretKey.get())) {
    return reinterpret_cast<const Crypto::SecretKey&>(transaction.secretKey.get());
  } else {
    auto getTransactionCompleted = std::promise<std::error_code>();
    auto getTransactionWaitFuture = getTransactionCompleted.get_future();
    CryptoNote::Transaction tx;
    m_node.getTransaction(std::move(txid), std::ref(tx),
      [&getTransactionCompleted](std::error_code ec) {
      auto detachedPromise = std::move(getTransactionCompleted);
      detachedPromise.set_value(ec);
    });
    std::error_code ec = getTransactionWaitFuture.get();
    if (ec) {
      m_logger(ERROR) << "Failed to get tx: " << ec << ", " << ec.message();
      return reinterpret_cast<const Crypto::SecretKey&>(transaction.secretKey.get());
    }

    Crypto::PublicKey txPubKey = getTransactionPublicKeyFromExtra(tx.extra);
    KeyPair deterministicTxKeys;
    bool ok = generateDeterministicTransactionKeys(tx, m_account.getAccountKeys().viewSecretKey, deterministicTxKeys)
      && deterministicTxKeys.publicKey == txPubKey;

    return ok ? deterministicTxKeys.secretKey : reinterpret_cast<const Crypto::SecretKey&>(transaction.secretKey.get());
  }
}

bool WalletLegacy::get_tx_key(Crypto::Hash& txid, Crypto::SecretKey& txSecretKey) {
  TransactionId ti = m_transactionsCache.findTransactionByHash(txid);
  WalletLegacyTransaction transaction;
  getTransaction(ti, transaction);
  txSecretKey = transaction.secretKey.get();
  if (txSecretKey == NULL_SECRET_KEY) {
    m_logger(Logging::INFO) << "Transaction secret key is not stored in wallet cache.";
    return false;
  }

  return true;
}

bool WalletLegacy::getTxProof(Crypto::Hash& txid, CryptoNote::AccountPublicAddress& address, Crypto::SecretKey& tx_key, std::string& sig_str) {
  return getTransactionProof(txid, address, tx_key, sig_str, m_logger.getLogger());
}

bool compareTransactionOutputInformationByAmount(const TransactionOutputInformation &a, const TransactionOutputInformation &b) {
  return a.amount < b.amount;
}

std::string WalletLegacy::getReserveProof(const uint64_t &reserve, const std::string &message) {
  const CryptoNote::AccountKeys keys = m_account.getAccountKeys();

  if (keys.spendSecretKey == NULL_SECRET_KEY) {
    throw std::runtime_error("Reserve proof can only be generated by a full wallet");
  }

  if (actualBalance() == 0) {
    throw std::runtime_error("Zero balance");
  }

  if (actualBalance() < reserve) {
    throw std::runtime_error("Not enough balance for the requested minimum reserve amount");
  }

  // determine which outputs to include in the proof
  std::vector<TransactionOutputInformation> selected_transfers;
  m_transferDetails->getOutputs(selected_transfers, ITransfersContainer::IncludeAllUnlocked);

  // minimize the number of outputs included in the proof, by only picking the N largest outputs that can cover the requested min reserve amount
  std::sort(selected_transfers.begin(), selected_transfers.end(), compareTransactionOutputInformationByAmount);
  std::reverse(selected_transfers.begin(), selected_transfers.end());
  while (selected_transfers.size() >= 2 && selected_transfers[1].amount >= reserve)
    selected_transfers.erase(selected_transfers.begin());
  size_t sz = 0;
  uint64_t total = 0;
  while (total < reserve) {
    total += selected_transfers[sz].amount;
    ++sz;
  }
  selected_transfers.resize(sz);

  std::string reserveProof = "";
  bool r = CryptoNote::getReserveProof(selected_transfers, keys, reserve, message, reserveProof, m_logger.getLogger());
  if (!r) {
    throw std::runtime_error("Failed to get reserve proof");
  }

  return reserveProof;
}

bool WalletLegacy::getTransactionInformation(const Crypto::Hash& transactionHash, TransactionInformation& info,
                                             uint64_t* amountIn, uint64_t* amountOut) const {
  return m_transferDetails->getTransactionInformation(transactionHash, info, amountIn, amountOut);
};

std::vector<TransactionOutputInformation> WalletLegacy::getTransactionOutputs(const Crypto::Hash& transactionHash, uint32_t flags) const {
  return m_transferDetails->getTransactionOutputs(transactionHash, flags);
};

std::vector<TransactionOutputInformation> WalletLegacy::getTransactionInputs(const Crypto::Hash& transactionHash, uint32_t flags) const {
  return m_transferDetails->getTransactionInputs(transactionHash, flags);
};

bool WalletLegacy::isFusionTransaction(const CryptoNote::WalletLegacyTransaction& walletTx) const {
  if (walletTx.fee != 0) {
    return false;
  }

  uint64_t inputsSum = 0;
  uint64_t outputsSum = 0;
  std::vector<uint64_t> outputsAmounts;
  std::vector<uint64_t> inputsAmounts;

  CryptoNote::TransactionInformation txInfo;

  for (const CryptoNote::TransactionOutputInformation& output :
       getTransactionOutputs(walletTx.hash, CryptoNote::ITransfersContainer::Flags::IncludeTypeKey
                                       | CryptoNote::ITransfersContainer::Flags::IncludeStateAll)) {
    if (outputsAmounts.size() <= output.outputInTransaction) {
        outputsAmounts.resize(output.outputInTransaction + 1, 0);
    }

    assert(output.amount != 0);
    assert(outputsAmounts[output.outputInTransaction] == 0);
    outputsAmounts[output.outputInTransaction] = output.amount;
    outputsSum += output.amount;
  }

  for (const CryptoNote::TransactionOutputInformation& input :
       getTransactionInputs(walletTx.hash, CryptoNote::ITransfersContainer::Flags::IncludeTypeKey)) {
    inputsSum += input.amount;
    inputsAmounts.push_back(input.amount);
  }

  if (!getTransactionInformation(walletTx.hash, txInfo)) {
    return false;
  }

  if (outputsSum != inputsSum || outputsSum != txInfo.totalAmountOut || inputsSum != txInfo.totalAmountIn) {
    return false;
  }

  return m_currency.isFusionTransaction(inputsAmounts, outputsAmounts, 0, txInfo.blockHeight); //size = 0 here because can't get real size of tx in wallet.
}

} //namespace CryptoNote
