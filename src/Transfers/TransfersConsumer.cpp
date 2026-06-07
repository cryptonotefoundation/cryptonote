// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers. 
// Copyright (c) 2018 BBSCoin developers
// Copyright (c) 2016-2026, The Karbo developers
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

#include "TransfersConsumer.h"

#include <numeric>
#include <future>

#include "CommonTypes.h"
#include "Common/BinaryArray.hpp"
#include "Common/BlockingQueue.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/TransactionApi.h"

#include "IWallet.h"
#include "INode.h"

using namespace Crypto;
using namespace Logging;
using namespace Common;


std::unordered_set<Crypto::Hash> transactions_hash_seen;
std::unordered_set<Crypto::PublicKey> public_keys_seen;
std::mutex seen_mutex;

namespace {

using namespace CryptoNote;

class MarkTransactionConfirmedException : public std::exception {
public:
  explicit MarkTransactionConfirmedException(const Crypto::Hash& txHash) : m_txHash(txHash) {}

  const Hash& getTxHash() const {
    return m_txHash;
  }

private:
  Crypto::Hash m_txHash;
};

void checkOutputKey(
  const KeyDerivation& derivation,
  const PublicKey& key,
  size_t keyIndex,
  size_t outputIndex,
  const std::unordered_set<PublicKey>& spendKeys,
  std::unordered_map<PublicKey, std::vector<uint32_t>>& outputs) {

  PublicKey spendKey;
  underive_public_key(derivation, keyIndex, key, spendKey);

  if (spendKeys.find(spendKey) != spendKeys.end()) {
    outputs[spendKey].push_back(static_cast<uint32_t>(outputIndex));
  }

}

void findMyOutputs(
  const ITransactionReader& tx,
  const SecretKey& viewSecretKey,
  const std::unordered_set<PublicKey>& spendKeys,
  std::unordered_map<PublicKey, std::vector<uint32_t>>& outputs) {

  auto txPublicKey = tx.getTransactionPublicKey();
  KeyDerivation derivation;

  if (!generate_key_derivation(txPublicKey, viewSecretKey, derivation)) {
    return;
  }

  size_t keyIndex = 0;
  size_t outputCount = tx.getOutputCount();

  for (size_t idx = 0; idx < outputCount; ++idx) {

    auto outType = tx.getOutputType(size_t(idx));

    if (outType == TransactionTypes::OutputType::Key) {

      uint64_t amount;
      KeyOutput out;
      tx.getOutput(idx, out, amount);
      checkOutputKey(derivation, out.key, keyIndex, idx, spendKeys, outputs);
      ++keyIndex;

    }
  }
}

// Detect whether WE sent a transaction by reconstructing the deterministic tx key.
// r = Hs(viewSecretKey || inputsHash), R = r*G. If R == tx.publicKey then we are the sender.
bool isOurOutgoingTransaction(const CryptoNote::ITransactionReader& tx, const Crypto::SecretKey& viewSecretKey) {
  if (viewSecretKey == CryptoNote::NULL_SECRET_KEY) return false;
  CryptoNote::KeyPair keys;
  if (!CryptoNote::generateDeterministicTransactionKeys(tx.getTransactionInputsHash(), viewSecretKey, keys)) return false;
  return keys.publicKey == tx.getTransactionPublicKey();
}

std::vector<Crypto::Hash> getBlockHashes(const CryptoNote::CompleteBlock* blocks, size_t count) {
  std::vector<Crypto::Hash> result;
  result.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    result.push_back(blocks[i].blockHash);
  }

  return result;
}

}

namespace CryptoNote {

TransfersConsumer::TransfersConsumer(const CryptoNote::Currency& currency, INode& node, Logging::ILogger& logger,
                                     const SecretKey& viewSecret) :
  m_node(node), m_viewSecret(viewSecret), m_currency(currency), m_logger(logger, "TransfersConsumer") {
  updateSyncStart();
}

ITransfersSubscription& TransfersConsumer::addSubscription(const AccountSubscription& subscription) {
  if (subscription.keys.viewSecretKey != m_viewSecret) {
    throw std::runtime_error("TransfersConsumer: view secret key mismatch");
  }

  auto& res = m_subscriptions[subscription.keys.address.spendPublicKey];

  if (res.get() == nullptr) {
    res.reset(new TransfersSubscription(m_currency, m_logger.getLogger(), subscription));
    m_spendKeys.insert(subscription.keys.address.spendPublicKey);
    if (subscription.keys.spendSecretKey != NULL_SECRET_KEY) {
      m_hasSpendKeys = true;
    }

    if (m_subscriptions.size() == 1) {
      m_syncStart = res->getSyncStart();
    }
    else {
      auto subStart = res->getSyncStart();
      m_syncStart.height = std::min(m_syncStart.height, subStart.height);
      m_syncStart.timestamp = std::min(m_syncStart.timestamp, subStart.timestamp);
    }
  }

  return *res;
}

bool TransfersConsumer::removeSubscription(const AccountPublicAddress& address) {
  m_subscriptions.erase(address.spendPublicKey);
  m_spendKeys.erase(address.spendPublicKey);
  updateSyncStart();
  return m_subscriptions.empty();
}

ITransfersSubscription* TransfersConsumer::getSubscription(const AccountPublicAddress& acc) {
  auto it = m_subscriptions.find(acc.spendPublicKey);
  return it == m_subscriptions.end() ? nullptr : it->second.get();
}

void TransfersConsumer::getSubscriptions(std::vector<AccountPublicAddress>& subscriptions) {
  for (const auto& kv : m_subscriptions) {
    subscriptions.push_back(kv.second->getAddress());
  }
}

void TransfersConsumer::initTransactionPool(const std::unordered_set<Crypto::Hash>& uncommitedTransactions) {
  for (auto itSubscriptions = m_subscriptions.begin(); itSubscriptions != m_subscriptions.end(); ++itSubscriptions) {
    std::vector<Crypto::Hash> unconfirmedTransactions;
    itSubscriptions->second->getContainer().getUnconfirmedTransactions(unconfirmedTransactions);

    for (auto itTransactions = unconfirmedTransactions.begin(); itTransactions != unconfirmedTransactions.end(); ++itTransactions) {
      if (uncommitedTransactions.count(*itTransactions) == 0) {
        m_poolTxs.emplace(*itTransactions);
      }
    }
  }
}

void TransfersConsumer::updateSyncStart() {
  SynchronizationStart start;

  start.height = std::numeric_limits<uint64_t>::max();
  start.timestamp = std::numeric_limits<uint64_t>::max();

  for (const auto& kv : m_subscriptions) {
    auto subStart = kv.second->getSyncStart();
    start.height = std::min(start.height, subStart.height);
    start.timestamp = std::min(start.timestamp, subStart.timestamp);
  }

  m_syncStart = start;
}

SynchronizationStart TransfersConsumer::getSyncStart() {
  return m_syncStart;
}

void TransfersConsumer::onBlockchainDetach(uint32_t height) {
  m_observerManager.notify(&IBlockchainConsumerObserver::onBlockchainDetach, this, height);

  for (const auto& kv : m_subscriptions) {
    kv.second->onBlockchainDetach(height);
  }
}

uint32_t TransfersConsumer::onNewBlocks(const CompleteBlock* blocks, uint32_t startHeight, uint32_t count) {
  assert(blocks);
  assert(count > 0);

  struct Tx {
    TransactionBlockInfo blockInfo;
    const ITransactionReader* tx;
    bool isLastTransactionInBlock;
  };

  struct PreprocessedTx : Tx, PreprocessInfo {};

  std::vector<PreprocessedTx> preprocessedTransactions;
  std::mutex preprocessedTransactionsMutex;

  size_t workers = std::thread::hardware_concurrency();
  if (workers == 0) {
    workers = 2;
  }

  BlockingQueue<Tx> inputQueue(workers * 2);

  std::atomic<bool> stopProcessing(false);
  std::atomic<size_t> emptyBlockCount(0);

  auto pushingThread = std::async(std::launch::async, [&] {
    for (uint32_t i = 0; i < count && !stopProcessing; ++i) {
      const auto& block = blocks[i].block;

      if (!block.is_initialized()) {
        ++emptyBlockCount;
        continue;
      }

      // filter by syncStartTimestamp
      if (m_syncStart.timestamp && block->timestamp < m_syncStart.timestamp) {
        ++emptyBlockCount;
        continue;
      }

      TransactionBlockInfo blockInfo;
      blockInfo.height = startHeight + i;
      blockInfo.timestamp = block->timestamp;
      blockInfo.transactionIndex = 0; // position in block

      for (const auto& tx : blocks[i].transactions) {
        auto pubKey = tx->getTransactionPublicKey();
        if (pubKey == NULL_PUBLIC_KEY) {
          ++blockInfo.transactionIndex;
          continue;
        }

        bool isLastTransactionInBlock = blockInfo.transactionIndex + 1 == blocks[i].transactions.size();
        Tx item = { blockInfo, tx.get(), isLastTransactionInBlock };
        inputQueue.push(item);
        ++blockInfo.transactionIndex;
      }
    }

    inputQueue.close();
  });

  auto processingFunction = [&] {
    Tx item;
    std::error_code ec;
    while (!stopProcessing && inputQueue.pop(item)) {
      PreprocessedTx output;
      static_cast<Tx&>(output) = item;

      ec = preprocessOutputs(item.blockInfo, *item.tx, output);
      if (ec) {
        stopProcessing = true;
        break;
      }

      std::lock_guard<std::mutex> lk(preprocessedTransactionsMutex);
      preprocessedTransactions.push_back(std::move(output));
    }
    return ec;
  };

  std::vector<std::future<std::error_code>> processingThreads;
  for (size_t i = 0; i < workers; ++i) {
    processingThreads.push_back(std::async(std::launch::async, processingFunction));
  }

  std::error_code processingError;
  for (auto& f : processingThreads) {
    try {
      std::error_code ec = f.get();
      if (!processingError && ec) {
        processingError = ec;
      }
    } catch (const std::system_error& e) {
      processingError = e.code();
    } catch (const std::exception&) {
      processingError = std::make_error_code(std::errc::operation_canceled);
    }
  }

  if (processingError) {
    forEachSubscription([&](TransfersSubscription& sub) {
      sub.onError(processingError, startHeight);
    });

    return 0;
  }

  std::vector<Crypto::Hash> blockHashes = getBlockHashes(blocks, count);
  m_observerManager.notify(&IBlockchainConsumerObserver::onBlocksAdded, this, blockHashes);

  // sort by block height and transaction index in block
  std::sort(preprocessedTransactions.begin(), preprocessedTransactions.end(), [](const PreprocessedTx& a, const PreprocessedTx& b) {
    return std::tie(a.blockInfo.height, a.blockInfo.transactionIndex) < std::tie(b.blockInfo.height, b.blockInfo.transactionIndex);
  });

  uint32_t processedBlockCount = static_cast<uint32_t>(emptyBlockCount);
  try {
    for (const auto& tx : preprocessedTransactions) {
      processTransaction(tx.blockInfo, *tx.tx, tx);

      if (tx.isLastTransactionInBlock) {
        ++processedBlockCount;
        m_logger(TRACE) << "Processed block " << processedBlockCount << " of " << count << ", last processed block index " << tx.blockInfo.height <<
          ", hash " << blocks[processedBlockCount - 1].blockHash;

        auto newHeight = startHeight + processedBlockCount - 1;
        forEachSubscription([newHeight](TransfersSubscription& sub) {
          sub.advanceHeight(newHeight);
        });
      }
    }
  } catch (const MarkTransactionConfirmedException& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to process block transactions: failed to confirm transaction " << e.getTxHash() <<
    //  ", remove this transaction from all containers and transaction pool";
    //forEachSubscription([&e](TransfersSubscription& sub) {
    //  sub.deleteUnconfirmedTransaction(e.getTxHash());
    //  });
      ", keep wallet state unconfirmed and remove this hash from known pool state";
    m_poolTxs.erase(e.getTxHash());
  } catch (std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to process block transactions, exception: " << e.what();
  } catch (...) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to process block transactions, unknown exception";
  }

  if (processedBlockCount < count) {
    uint32_t detachIndex = startHeight + processedBlockCount;
    m_logger(ERROR, BRIGHT_RED) << "Not all block transactions are processed, fully processed block count: " << processedBlockCount << " of " << count <<
      ", last processed block hash " << (processedBlockCount > 0 ? blocks[processedBlockCount - 1].blockHash : NULL_HASH) <<
      ", detach block index " << detachIndex << " to remove partially processed block";
    forEachSubscription([detachIndex](TransfersSubscription& sub) {
      sub.onBlockchainDetach(detachIndex);
    });
  }

  return processedBlockCount;
}

std::error_code TransfersConsumer::onPoolUpdated(const std::vector<std::unique_ptr<ITransactionReader>>& addedTransactions, const std::vector<Hash>& deletedTransactions) {
  TransactionBlockInfo unconfirmedBlockInfo;
  unconfirmedBlockInfo.timestamp = 0;
  unconfirmedBlockInfo.height = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;

  std::error_code processingError;
  for (auto& cryptonoteTransaction : addedTransactions) {
    m_poolTxs.emplace(cryptonoteTransaction->getTransactionHash());
    processingError = processTransaction(unconfirmedBlockInfo, *cryptonoteTransaction.get());
    if (processingError) {
      for (auto& sub : m_subscriptions) {
        sub.second->onError(processingError, WALLET_UNCONFIRMED_TRANSACTION_HEIGHT);
      }

      return processingError;
    }
  }

  for (auto& deletedTxHash : deletedTransactions) {
    m_poolTxs.erase(deletedTxHash);

    m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionDeleteBegin, this, deletedTxHash);
    for (auto& sub : m_subscriptions) {
      sub.second->deleteUnconfirmedTransaction(*reinterpret_cast<const Hash*>(&deletedTxHash));
    }

    m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionDeleteEnd, this, deletedTxHash);
  }

  return std::error_code();
}

const std::unordered_set<Crypto::Hash>& TransfersConsumer::getKnownPoolTxIds() const {
  return m_poolTxs;
}

std::error_code TransfersConsumer::addUnconfirmedTransaction(const ITransactionReader& transaction) {
  TransactionBlockInfo unconfirmedBlockInfo;
  unconfirmedBlockInfo.height = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
  unconfirmedBlockInfo.timestamp = 0;
  unconfirmedBlockInfo.transactionIndex = 0;

  return processTransaction(unconfirmedBlockInfo, transaction);
}

void TransfersConsumer::removeUnconfirmedTransaction(const Crypto::Hash& transactionHash) {
  m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionDeleteBegin, this, transactionHash);
  for (auto& subscription : m_subscriptions) {
    subscription.second->deleteUnconfirmedTransaction(transactionHash);
  }
  m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionDeleteEnd, this, transactionHash);
}

void TransfersConsumer::addPublicKeysSeen(const Crypto::Hash& transactionHash, const Crypto::PublicKey& outputKey) {
  std::lock_guard<std::mutex> lk(seen_mutex);
  transactions_hash_seen.insert(transactionHash);
  public_keys_seen.insert(outputKey);
}

std::error_code createTransfers(
  const AccountKeys& account,
  const TransactionBlockInfo& blockInfo,
  const ITransactionReader& tx,
  const std::vector<uint32_t>& outputs,
  const std::vector<uint32_t>& globalIdxs,
  std::vector<TransactionOutputInformationIn>& transfers,
  Logging::LoggerRef& m_logger) {

  auto txPubKey = tx.getTransactionPublicKey();
  auto txHash = tx.getTransactionHash();
  std::vector<PublicKey> temp_keys;
  std::lock_guard<std::mutex> lk(seen_mutex);

  for (auto idx : outputs) {

    if (idx >= tx.getOutputCount()) {
      return std::make_error_code(std::errc::argument_out_of_domain);
    }

    auto outType = tx.getOutputType(size_t(idx));

    if (
      outType != TransactionTypes::OutputType::Key) {
      continue;
    }

    TransactionOutputInformationIn info;

    info.type = outType;
    info.transactionPublicKey = txPubKey;
    info.outputInTransaction = idx;
    info.globalOutputIndex = (blockInfo.height == WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) ?
      UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : globalIdxs[idx];

    if (outType == TransactionTypes::OutputType::Key) {
      uint64_t amount;
      KeyOutput out;
      tx.getOutput(idx, out, amount);

      CryptoNote::KeyPair in_ephemeral;
      CryptoNote::generate_key_image_helper(
        account,
        txPubKey,
        idx,
        in_ephemeral,
        info.keyImage);

      assert(out.key == reinterpret_cast<const PublicKey&>(in_ephemeral.publicKey));

      std::unordered_set<Crypto::Hash>::iterator it = transactions_hash_seen.find(txHash);
      if (it == transactions_hash_seen.end()) {
        std::unordered_set<Crypto::PublicKey>::iterator key_it = public_keys_seen.find(out.key);
        if (key_it != public_keys_seen.end()) {
          throw std::runtime_error("duplicate transaction output key is found");
          return std::error_code();
        }
        if (std::find(temp_keys.begin(), temp_keys.end(), out.key) != temp_keys.end()) {
          throw std::runtime_error("the same output key is present more than once");
          return std::error_code();
        }
        temp_keys.push_back(out.key);
      }
      info.amount = amount;
      info.outputKey = out.key;

    }
    transfers.push_back(info);
  }

  transactions_hash_seen.insert(tx.getTransactionHash());
  for (std::vector<PublicKey>::iterator it = temp_keys.begin(); it != temp_keys.end(); it++) {
    public_keys_seen.insert(*it);
  }

  return std::error_code();
}

std::error_code TransfersConsumer::preprocessOutputs(const TransactionBlockInfo& blockInfo, const ITransactionReader& tx, PreprocessInfo& info) {
  std::unordered_map<PublicKey, std::vector<uint32_t>> outputs;
  try {
    findMyOutputs(tx, m_viewSecret, m_spendKeys, outputs);
  }
  catch (const std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to process transaction: " << e.what() << ", transaction hash " << Common::podToHex(tx.getTransactionHash());
    return std::error_code();
  }

  // Detect outgoing transaction via viewSecretKey (view-only wallets only).
  // Skipped for full wallets (m_hasSpendKeys == true): they detect their own outgoing
  // transactions via key-image matching in addTransactionInputs, so calling
  // isOurOutgoingTransaction() on every blockchain transaction would be wasted work.
  if (!m_hasSpendKeys && isOurOutgoingTransaction(tx, m_viewSecret)) {
    info.isOutgoing = true;
    m_logger(DEBUGGING) << "Detected outgoing transaction via viewSecretKey, hash " << Common::podToHex(tx.getTransactionHash());
  }

  if (outputs.empty() && !info.isOutgoing) {
    return std::error_code();
  }

  std::error_code errorCode;
  auto txHash = tx.getTransactionHash();
  if (blockInfo.height != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
    errorCode = getGlobalIndices(reinterpret_cast<const Hash&>(txHash), info.globalIdxs);
    if (errorCode) {
      return errorCode;
    }
  }

  for (const auto& kv : outputs) {
    auto it = m_subscriptions.find(kv.first);
    if (it != m_subscriptions.end()) {
      auto& transfers = info.outputs[kv.first];
      try {
        errorCode = createTransfers(it->second->getKeys(), blockInfo, tx, kv.second, info.globalIdxs, transfers, m_logger);
        if (errorCode) {
          return errorCode;
        }
      }
      catch (const std::exception& e) {
        m_logger(ERROR, BRIGHT_RED) << "Failed to process transaction: " << e.what() << ", transaction hash " << Common::podToHex(tx.getTransactionHash());
        return std::error_code();
      }
    }
  }

  return std::error_code();
}

std::error_code TransfersConsumer::processTransaction(const TransactionBlockInfo& blockInfo, const ITransactionReader& tx) {
  PreprocessInfo info;
  auto ec = preprocessOutputs(blockInfo, tx, info);
  if (ec) {
    return ec;
  }

  processTransaction(blockInfo, tx, info);
  return std::error_code();
}

void TransfersConsumer::processTransaction(const TransactionBlockInfo& blockInfo, const ITransactionReader& tx, const PreprocessInfo& info) {
  std::vector<TransactionOutputInformationIn> emptyOutputs;
  std::vector<ITransfersContainer*> transactionContainers;

  m_logger(TRACE) << "Process transaction, block " << blockInfo.height << ", transaction index " << blockInfo.transactionIndex << ", hash " << tx.getTransactionHash();
  bool someContainerUpdated = false;
  for (auto& kv : m_subscriptions) {
    auto it = info.outputs.find(kv.first);
    auto& subscriptionOutputs = (it == info.outputs.end()) ? emptyOutputs : it->second;

    bool containerContainsTx;
    bool containerUpdated;
    // Pass isOutgoing so subscriptions with no matching outputs still record the tx
    // when detected as our outgoing tx (tracking wallet mode).
    processOutputs(blockInfo, *kv.second, tx, subscriptionOutputs, info.globalIdxs, containerContainsTx, containerUpdated, info.isOutgoing);
    someContainerUpdated = someContainerUpdated || containerUpdated;
    if (containerContainsTx) {
      transactionContainers.emplace_back(&kv.second->getContainer());
    }
  }

  if (someContainerUpdated) {
    m_logger(TRACE) << "Transaction updated some containers, hash " << tx.getTransactionHash();
    m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionUpdated, this, tx.getTransactionHash(), transactionContainers);
  } else {
    m_logger(TRACE) << "Transaction doesn't updated any container, hash " << tx.getTransactionHash();
  }
}

void TransfersConsumer::processOutputs(const TransactionBlockInfo& blockInfo, TransfersSubscription& sub, const ITransactionReader& tx,
  const std::vector<TransactionOutputInformationIn>& transfers, const std::vector<uint32_t>& globalIdxs, bool& contains, bool& updated,
  bool isOutgoing) {

  TransactionInformation subscriptionTxInfo;
  contains = sub.getContainer().getTransactionInformation(tx.getTransactionHash(), subscriptionTxInfo);
  updated = false;

  if (contains) {
    if (subscriptionTxInfo.blockHeight == WALLET_UNCONFIRMED_TRANSACTION_HEIGHT && blockInfo.height != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
      try {
        // pool->blockchain
        sub.markTransactionConfirmed(blockInfo, tx.getTransactionHash(), globalIdxs);
        updated = true;
      } catch (const std::exception& e) {
        m_logger(ERROR, BRIGHT_RED) << "markTransactionConfirmed failed for tx " << tx.getTransactionHash() << ": " << e.what();
        throw MarkTransactionConfirmedException(tx.getTransactionHash());
      } catch (...) {
        m_logger(ERROR, BRIGHT_RED) << "markTransactionConfirmed failed for tx " << tx.getTransactionHash() << ": unknown exception";
        throw MarkTransactionConfirmedException(tx.getTransactionHash());
      }
    } else {
      assert(subscriptionTxInfo.blockHeight == blockInfo.height);
    }
  } else {
    updated = sub.addTransaction(blockInfo, tx, transfers, isOutgoing);
    contains = updated;
  }
}

std::error_code TransfersConsumer::getGlobalIndices(const Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices) {
  std::promise<std::error_code> prom;
  std::future<std::error_code> f = prom.get_future();

  INode::Callback cb = [&prom](std::error_code ec) {
    std::promise<std::error_code> p(std::move(prom));
    p.set_value(ec);
  };

  outsGlobalIndices.clear();
  m_node.getTransactionOutsGlobalIndices(transactionHash, outsGlobalIndices, cb);

  return f.get();
}

}
