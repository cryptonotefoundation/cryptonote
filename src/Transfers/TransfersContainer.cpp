// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
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

#include "TransfersContainer.h"
#include "IWalletLegacy.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/SerializationOverloads.h"

using namespace Common;
using namespace Crypto;
using namespace Logging;

namespace CryptoNote {

void serialize(TransactionInformation& ti, CryptoNote::ISerializer& s) {
  s(ti.transactionHash, "");
  s(ti.publicKey, "");
  serializeBlockHeight(s, ti.blockHeight, "");
  s(ti.timestamp, "");
  s(ti.unlockTime, "");
  s(ti.totalAmountIn, "");
  s(ti.totalAmountOut, "");
  s(ti.extra, "");
  s(ti.paymentId, "");
}

const uint32_t TRANSFERS_CONTAINER_STORAGE_VERSION = 0;

namespace {
  template<typename TIterator>
  class TransferIteratorList {
  public:
    TransferIteratorList(const TIterator& begin, const TIterator& end) : m_end(end) {
      for (auto it = begin; it != end; ++it) {
        m_list.emplace_back(it);
      }
    }

    TransferIteratorList(TransferIteratorList<TIterator>&& other) {
      m_list = std::move(other.m_list);
      m_end = std::move(other.m_end);
    }

    TransferIteratorList& operator=(TransferIteratorList<TIterator>&& other) {
      m_list = std::move(other.m_list);
      m_end = std::move(other.m_end);
      return *this;
    }

    void sort() {
      std::sort(m_list.begin(), m_list.end(), &TransferIteratorList::lessTIterator);
    }

    TIterator findFirstByAmount(uint64_t amount) const {
      auto listIt = std::find_if(m_list.begin(), m_list.end(), [amount](const TIterator& it) {
        return it->amount == amount;
      });
      return listIt == m_list.end() ? m_end : *listIt;
    }

    TIterator minElement() const {
      auto listIt = std::min_element(m_list.begin(), m_list.end(), &TransferIteratorList::lessTIterator);
      return listIt == m_list.end() ? m_end : *listIt;
    }

  private:
    static bool lessTIterator(const TIterator& it1, const TIterator& it2) {
      return
        (it1->blockHeight < it2->blockHeight) ||
        (it1->blockHeight == it2->blockHeight && it1->transactionIndex < it2->transactionIndex);
    }

  private:
    std::vector<TIterator> m_list;
    TIterator m_end;
  };

  template<typename TIterator>
  TransferIteratorList<TIterator> createTransferIteratorList(const std::pair<TIterator, TIterator>& itPair) {
    return TransferIteratorList<TIterator>(itPair.first, itPair.second);
  }
}


SpentOutputDescriptor::SpentOutputDescriptor() :
    m_type(TransactionTypes::OutputType::Invalid) {
}

SpentOutputDescriptor::SpentOutputDescriptor(const TransactionOutputInformationIn& transactionInfo) :
    m_type(transactionInfo.type),
    m_amount(0),
    m_globalOutputIndex(0) {
  if (m_type == TransactionTypes::OutputType::Key) {
    m_keyImage = &transactionInfo.keyImage;
  } else {
    assert(false);
  }
}

SpentOutputDescriptor::SpentOutputDescriptor(const KeyImage* keyImage) {
  assign(keyImage);
}

void SpentOutputDescriptor::assign(const KeyImage* keyImage) {
  m_type = TransactionTypes::OutputType::Key;
  m_keyImage = keyImage;
}

bool SpentOutputDescriptor::isValid() const {
  return m_type != TransactionTypes::OutputType::Invalid;
}

bool SpentOutputDescriptor::operator==(const SpentOutputDescriptor& other) const {
  if (m_type == TransactionTypes::OutputType::Key) {
    return other.m_type == m_type && *other.m_keyImage == *m_keyImage;
  } else {
    assert(false);
    return false;
  }
}

size_t SpentOutputDescriptor::hash() const {
  if (m_type == TransactionTypes::OutputType::Key) {
    static_assert(sizeof(size_t) < sizeof(*m_keyImage), "sizeof(size_t) < sizeof(*m_keyImage)");
    return *reinterpret_cast<const size_t*>(m_keyImage->data);
  } else {
    assert(false);
    return 0;
  }
}


TransfersContainer::TransfersContainer(const Currency& currency, Logging::ILogger& logger, size_t transactionSpendableAge) :
  m_currentHeight(0),
  m_currency(currency),
  m_logger(logger, "TransfersContainer"),
  m_transactionSpendableAge(transactionSpendableAge) {
}

bool TransfersContainer::addTransaction(const TransactionBlockInfo& block, const ITransactionReader& tx,
  const std::vector<TransactionOutputInformationIn>& transfers, bool isOutgoing) {

  try {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (block.height < m_currentHeight) {
      auto message = "Failed to add transaction: block index < m_currentHeight";
      m_logger(ERROR, BRIGHT_RED) << message << ", block " << block.height << ", m_currentHeight " << m_currentHeight;
      throw std::invalid_argument(message);
    }

    if (m_transactions.count(tx.getTransactionHash()) > 0) {
      auto message = "Transaction is already added";
      m_logger(ERROR, BRIGHT_RED) << message << ", hash " << tx.getTransactionHash();
      throw std::invalid_argument(message);
    }

    bool added = addTransactionOutputs(block, tx, transfers);
    added |= addTransactionInputs(block, tx, isOutgoing);

    // For view-only wallets, the sender detection via viewSecretKey may find an outgoing tx
    // that has no change outputs and no trackable inputs.
    // Force-record it so the wallet can report the transaction as outgoing.
    bool forceAdded = isOutgoing && !added;
    if (added || forceAdded) {
      addTransaction(block, tx);
    }

    if (block.height != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      m_currentHeight = block.height;
    }

    return added || forceAdded;
  } catch (...) {
    if (m_transactions.count(tx.getTransactionHash()) == 0) {
      m_logger(ERROR, BRIGHT_RED) << "Failed to add transaction, remove transaction transfers, block " << block.height <<
        ", transaction hash " << tx.getTransactionHash();
      deleteTransactionTransfers(tx.getTransactionHash());
    }

    throw;
  }
}

/**
 * \pre m_mutex is locked.
 */
void TransfersContainer::addTransaction(const TransactionBlockInfo& block, const ITransactionReader& tx) {
  auto txHash = tx.getTransactionHash();

  TransactionInformation txInfo;
  txInfo.blockHeight = block.height;
  txInfo.timestamp = block.timestamp;
  txInfo.transactionHash = txHash;
  txInfo.unlockTime = tx.getUnlockTime();
  txInfo.publicKey = tx.getTransactionPublicKey();
  txInfo.totalAmountIn = tx.getInputTotalAmount();
  txInfo.totalAmountOut = tx.getOutputTotalAmount();
  txInfo.extra = tx.getExtra();

  if (!tx.getPaymentId(txInfo.paymentId)) {
    txInfo.paymentId = NULL_HASH;
  }

  auto result = m_transactions.emplace(std::move(txInfo));
  (void)result; // Disable unused warning
  assert(result.second);
}

/**
 * \pre m_mutex is locked.
 */
bool TransfersContainer::addTransactionOutputs(const TransactionBlockInfo& block, const ITransactionReader& tx,
                                               const std::vector<TransactionOutputInformationIn>& transfers) {
  bool outputsAdded = false;

  auto txHash = tx.getTransactionHash();
  bool transactionIsUnconfimed = (block.height == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
  for (const auto& transfer : transfers) {
    assert(transfer.outputInTransaction < tx.getOutputCount());
    assert(transfer.type == tx.getOutputType(transfer.outputInTransaction));
    assert(transfer.amount > 0);

    bool transferIsUnconfirmed = (transfer.globalOutputIndex == UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
    if (transactionIsUnconfimed != transferIsUnconfirmed) {
      auto message = "Failed to add transaction output: globalOutputIndex is invalid";
      m_logger(ERROR, BRIGHT_RED) << message << ", globalOutputIndex " << transfer.globalOutputIndex << ", transaction is confirmed " << transferIsUnconfirmed;
      throw std::invalid_argument(message);
    }

    TransactionOutputInformationEx info;
    static_cast<TransactionOutputInformationIn&>(info) = transfer;
    info.blockHeight = block.height;
    info.transactionIndex = block.transactionIndex;
    info.unlockTime = tx.getUnlockTime();
    info.transactionHash = txHash;
    info.visible = true;

    if (transferIsUnconfirmed) {
      auto result = m_unconfirmedTransfers.emplace(std::move(info));
      (void)result; // Disable unused warning
      assert(result.second);
    } else {
      if (info.type == TransactionTypes::OutputType::Key) {
        bool duplicate = false;
        SpentOutputDescriptor descriptor(transfer);

        auto availableRange = m_availableTransfers.get<SpentOutputDescriptorIndex>().equal_range(descriptor);
        for (auto it = availableRange.first; !duplicate && it != availableRange.second; ++it) {
          if (it->transactionHash == info.transactionHash && it->outputInTransaction == info.outputInTransaction) {
            duplicate = true;
          }
        }

        auto spentRange = m_spentTransfers.get<SpentOutputDescriptorIndex>().equal_range(descriptor);
        for (auto it = spentRange.first; !duplicate && it != spentRange.second; ++it) {
          if (it->transactionHash == info.transactionHash && it->outputInTransaction == info.outputInTransaction) {
            duplicate = true;
          }
        }

        if (duplicate) {
          auto message = "Failed to add transaction output: key output already exists";
          m_logger(ERROR, BRIGHT_RED) << message << ", transaction hash " << info.transactionHash << ", output index " << info.outputInTransaction <<
            ", key image " << info.keyImage;
          throw std::runtime_error(message);
        }
      }

      auto result = m_availableTransfers.emplace(std::move(info));
      (void)result; // Disable unused warning
      assert(result.second);
    }

    if (info.type == TransactionTypes::OutputType::Key) {
      updateTransfersVisibility(info.keyImage);
    }

    outputsAdded = true;
  }

  return outputsAdded;
}

/**
 * \pre m_mutex is locked.
 */
bool TransfersContainer::addTransactionInputs(const TransactionBlockInfo& block, const ITransactionReader& tx, bool isOutgoing) {
  bool inputsAdded = false;

  for (size_t i = 0; i < tx.getInputCount(); ++i) {
    auto inputType = tx.getInputType(i);

    if (inputType == TransactionTypes::InputType::Key) {
      KeyInput input;
      tx.getInput(i, input);

      SpentOutputDescriptor descriptor(&input.keyImage);
      auto spentRange = m_spentTransfers.get<SpentOutputDescriptorIndex>().equal_range(descriptor);
      if (std::distance(spentRange.first, spentRange.second) > 0) {
        assert(std::distance(spentRange.first, spentRange.second) == 1);
        const auto& spentOutput = *spentRange.first;
        auto message = "Failed add key input: key image already spent";
        m_logger(ERROR, BRIGHT_RED) << message << ", key image " << input.keyImage << '\n' <<
          "    rejected transaction" <<
          ": hash " << tx.getTransactionHash() <<
          ", block " << block.height <<
          ", transaction index " << block.transactionIndex <<
          ", input " << i << '\n' <<
          "    spending transaction" <<
          ": hash " << spentOutput.spendingTransactionHash <<
          ", block " << spentOutput.spendingBlock.height <<
          ", input " << spentOutput.inputInTransaction << '\n' <<
          "    spent output        " <<
          ": hash " << spentOutput.transactionHash <<
          ", block " << spentOutput.blockHeight <<
          ", transaction index " << spentOutput.transactionIndex <<
          ", output " << spentOutput.outputInTransaction <<
          ", amount " << m_currency.formatAmount(spentOutput.amount);
        throw std::runtime_error(message);
      }

      auto availableRange = m_availableTransfers.get<SpentOutputDescriptorIndex>().equal_range(descriptor);
      auto unconfirmedRange = m_unconfirmedTransfers.get<SpentOutputDescriptorIndex>().equal_range(descriptor);
      size_t availableCount = std::distance(availableRange.first, availableRange.second);
      size_t unconfirmedCount = std::distance(unconfirmedRange.first, unconfirmedRange.second);

      if (availableCount == 0) {
        if (unconfirmedCount > 0) {
          auto message = "Failed to add key input: spend output of unconfirmed transaction";
          m_logger(ERROR, BRIGHT_RED) << message << ", key image " << input.keyImage;
          throw std::runtime_error(message);
        }

        // Fallback for view-only wallets: generate_key_image_helper() with a null spendSecretKey
        // produces a partial key image ki_partial = Hs(a·R,i)·Hp(P) rather than the real
        // ki = (Hs(a·R,i)+b)·Hp(P), so the key-image lookup above finds nothing.
        // When isOutgoing is true (confirmed by viewSecretKey match in TransfersConsumer),
        // identify the spent output by ring membership: decode the input's ring-member global
        // output indices and find which of our available outputs is one of them.
        // This is RingCT-compatible — global output indices are always visible even when
        // amounts are committed, unlike amount-based matching.
        if (isOutgoing) {
          // Decode relative offsets -- absolute global output indices.
          // outputIndexes[0] is the first absolute index; subsequent entries are differences.
          std::vector<uint32_t> ringAbsoluteIdxs;
          ringAbsoluteIdxs.reserve(input.outputIndexes.size());
          uint32_t accum = 0;
          for (uint32_t rel : input.outputIndexes) {
            accum += rel;
            ringAbsoluteIdxs.push_back(accum);
          }

          bool ringMatched = false;
          for (auto it = m_availableTransfers.begin(); it != m_availableTransfers.end(); ++it) {
            if (it->globalOutputIndex == UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX) continue;
            if (std::find(ringAbsoluteIdxs.begin(), ringAbsoluteIdxs.end(),
                          it->globalOutputIndex) == ringAbsoluteIdxs.end()) continue;
            // Our output's global index appears in this input's ring -- it is the spent one.
            // Correct the stored partial key image to the real one from the input.
            TransactionOutputInformationEx corrected = *it;
            corrected.keyImage = input.keyImage;
            m_availableTransfers.erase(it);   // iterator invalid after erase
            copyToSpent(block, tx, i, corrected);
            updateTransfersVisibility(input.keyImage);
            inputsAdded = true;
            ringMatched = true;
            break;
          }
          if (!ringMatched) {
            m_logger(DEBUGGING) << "Audit wallet: outgoing input key image " << input.keyImage
              << " has no ring member matching any available output"
              << " (may have been received before sync start)";
          }
        }
        // Whether we matched or not, move on to the next input.
        continue;
      }

      auto& outputDescriptorIndex = m_availableTransfers.get<SpentOutputDescriptorIndex>();
      auto availableOutputsRange = outputDescriptorIndex.equal_range(SpentOutputDescriptor(&input.keyImage));

      auto iteratorList = createTransferIteratorList(availableOutputsRange);
      iteratorList.sort();
      auto spendingTransferIt = iteratorList.findFirstByAmount(input.amount);

      if (spendingTransferIt == availableOutputsRange.second) {
        auto message = "Failed to add key input: invalid amount";
        m_logger(ERROR, BRIGHT_RED) << message << ", key image " << input.keyImage << ", amount " << m_currency.formatAmount(input.amount);
        throw std::runtime_error(message);
      }

      assert(spendingTransferIt->keyImage == input.keyImage);
      copyToSpent(block, tx, i, *spendingTransferIt);
      // erase from available outputs
      outputDescriptorIndex.erase(spendingTransferIt);
      updateTransfersVisibility(input.keyImage);

      inputsAdded = true;
    } else {
      assert(inputType == TransactionTypes::InputType::Generating);
    }
  }

  return inputsAdded;
}

bool TransfersContainer::deleteUnconfirmedTransaction(const Hash& transactionHash) {
  std::unique_lock<std::mutex> lock(m_mutex);

  auto it = m_transactions.find(transactionHash);
  if (it == m_transactions.end()) {
    return false;
  } else if (it->blockHeight != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
    return false;
  } else {
    deleteTransactionTransfers(it->transactionHash);
    m_transactions.erase(it);
    return true;
  }
}

bool TransfersContainer::markTransactionConfirmed(const TransactionBlockInfo& block, const Hash& transactionHash,
                                                  const std::vector<uint32_t>& globalIndices) {
  if (block.height == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
    auto message = "Failed to confirm transaction: block height is unconfirmed";
    m_logger(ERROR, BRIGHT_RED) << message << ", transaction hash " << transactionHash;
    throw std::invalid_argument(message);
  }

  std::unique_lock<std::mutex> lock(m_mutex);

  auto transactionIt = m_transactions.find(transactionHash);
  if (transactionIt == m_transactions.end()) {
    return false;
  }

  if (transactionIt->blockHeight != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
    return false;
  }

  try {
    auto txInfo = *transactionIt;
    txInfo.blockHeight = block.height;
    txInfo.timestamp = block.timestamp;
    m_transactions.replace(transactionIt, txInfo);

    auto availableRange = m_unconfirmedTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
    for (auto transferIt = availableRange.first; transferIt != availableRange.second; ) {
      auto transfer = *transferIt;
      assert(transfer.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
      assert(transfer.globalOutputIndex == UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
      if (transfer.outputInTransaction >= globalIndices.size()) {
        auto message = "Failed to confirm transaction: not enough elements in globalIndices";
        m_logger(ERROR, BRIGHT_RED) << message << ", globalIndices.size() " << globalIndices.size() << ", output index " << transfer.outputInTransaction;
        throw std::invalid_argument(message);
      }

      transfer.blockHeight = block.height;
      transfer.transactionIndex = block.transactionIndex;
      transfer.globalOutputIndex = globalIndices[transfer.outputInTransaction];

      auto result = m_availableTransfers.emplace(std::move(transfer));
      (void)result; // Disable unused warning
      assert(result.second);

      transferIt = m_unconfirmedTransfers.get<ContainingTransactionIndex>().erase(transferIt);

      if (transfer.type == TransactionTypes::OutputType::Key) {
        updateTransfersVisibility(transfer.keyImage);
      }
    }

    auto& spendingTransactionIndex = m_spentTransfers.get<SpendingTransactionIndex>();
    auto spentRange = spendingTransactionIndex.equal_range(transactionHash);
    for (auto transferIt = spentRange.first; transferIt != spentRange.second; ++transferIt) {
      auto transfer = *transferIt;
      assert(transfer.spendingBlock.height == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);

      transfer.spendingBlock = block;
      spendingTransactionIndex.replace(transferIt, transfer);
    }
  } catch (std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "markTransactionConfirmed failed: " << e.what() << ", rollback changes, block index " << block.height <<
      ", tx " << transactionHash;

    auto txInfo = *transactionIt;
    txInfo.blockHeight = WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT;
    txInfo.timestamp = 0;
    m_transactions.replace(transactionIt, txInfo);

    auto availableRange = m_availableTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
    for (auto transferIt = availableRange.first; transferIt != availableRange.second; ) {
      TransactionOutputInformationEx unconfirmedTransfer = *transferIt;
      assert(unconfirmedTransfer.blockHeight != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
      assert(unconfirmedTransfer.globalOutputIndex != UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);
      unconfirmedTransfer.blockHeight = WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT;
      unconfirmedTransfer.transactionIndex = 0;
      unconfirmedTransfer.globalOutputIndex = UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX;

      auto result = m_unconfirmedTransfers.emplace(std::move(unconfirmedTransfer));
      (void)result; // Disable unused warning
      assert(result.second);

      transferIt = m_availableTransfers.get<ContainingTransactionIndex>().erase(transferIt);

      if (unconfirmedTransfer.type == TransactionTypes::OutputType::Key) {
        updateTransfersVisibility(unconfirmedTransfer.keyImage);
      }
    }

    auto& spendingTransactionIndex = m_spentTransfers.get<SpendingTransactionIndex>();
    auto spentRange = spendingTransactionIndex.equal_range(transactionHash);
    for (auto transferIt = spentRange.first; transferIt != spentRange.second; ++transferIt) {
      auto spentTransfer = *transferIt;
      spentTransfer.spendingBlock.height = WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT;
      spentTransfer.spendingBlock.timestamp = 0;
      spentTransfer.spendingBlock.transactionIndex = 0;

      spendingTransactionIndex.replace(transferIt, spentTransfer);
    }

    throw;
  }

  return true;
}

/**
 * \pre m_mutex is locked.
 */
void TransfersContainer::deleteTransactionTransfers(const Hash& transactionHash) {
  auto& spendingTransactionIndex = m_spentTransfers.get<SpendingTransactionIndex>();
  auto spentTransfersRange = spendingTransactionIndex.equal_range(transactionHash);
  for (auto it = spentTransfersRange.first; it != spentTransfersRange.second;) {
    assert(it->blockHeight != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
    assert(it->globalOutputIndex != UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

    auto result = m_availableTransfers.emplace(static_cast<const TransactionOutputInformationEx&>(*it));
    assert(result.second);
    it = spendingTransactionIndex.erase(it);

    if (result.first->type == TransactionTypes::OutputType::Key) {
      updateTransfersVisibility(result.first->keyImage);
    }
  }

  auto unconfirmedTransfersRange = m_unconfirmedTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
  for (auto it = unconfirmedTransfersRange.first; it != unconfirmedTransfersRange.second;) {
    if (it->type == TransactionTypes::OutputType::Key) {
      KeyImage keyImage = it->keyImage;
      it = m_unconfirmedTransfers.get<ContainingTransactionIndex>().erase(it);
      updateTransfersVisibility(keyImage);
    } else {
      it = m_unconfirmedTransfers.get<ContainingTransactionIndex>().erase(it);
    }
  }

  auto& transactionTransfersIndex = m_availableTransfers.get<ContainingTransactionIndex>();
  auto transactionTransfersRange = transactionTransfersIndex.equal_range(transactionHash);
  for (auto it = transactionTransfersRange.first; it != transactionTransfersRange.second;) {
    if (it->type == TransactionTypes::OutputType::Key) {
      KeyImage keyImage = it->keyImage;
      it = transactionTransfersIndex.erase(it);
      updateTransfersVisibility(keyImage);
    } else {
      it = transactionTransfersIndex.erase(it);
    }
  }
}

/**
 * \pre m_mutex is locked.
 */
void TransfersContainer::copyToSpent(const TransactionBlockInfo& block, const ITransactionReader& tx, size_t inputIndex,
                                     const TransactionOutputInformationEx& output) {
  assert(output.blockHeight != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
  assert(output.globalOutputIndex != UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

  SpentTransactionOutput spentOutput;
  static_cast<TransactionOutputInformationEx&>(spentOutput) = output;
  spentOutput.spendingBlock = block;
  spentOutput.spendingTransactionHash = tx.getTransactionHash();
  spentOutput.inputInTransaction = static_cast<uint32_t>(inputIndex);
  auto result = m_spentTransfers.emplace(std::move(spentOutput));
  (void)result; // Disable unused warning
  assert(result.second);
}

std::vector<Hash> TransfersContainer::detach(uint32_t height) {
  // This method expects that WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT is a big positive number
  assert(height < WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);

  std::lock_guard<std::mutex> lk(m_mutex);

  std::vector<Hash> deletedTransactions;
  auto& spendingTransactionIndex = m_spentTransfers.get<SpendingTransactionIndex>();
  auto& blockHeightIndex = m_transactions.get<1>();
  auto it = blockHeightIndex.end();
  while (it != blockHeightIndex.begin()) {
    --it;

    bool doDelete = false;
    if (it->blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      auto range = spendingTransactionIndex.equal_range(it->transactionHash);
      for (auto spentTransferIt = range.first; spentTransferIt != range.second; ++spentTransferIt) {
        if (spentTransferIt->blockHeight >= height) {
          doDelete = true;
          break;
        }
      }
    } else if (it->blockHeight >= height) {
      doDelete = true;
    } else {
      break;
    }

    if (doDelete) {
      deleteTransactionTransfers(it->transactionHash);
      deletedTransactions.emplace_back(it->transactionHash);
      it = blockHeightIndex.erase(it);
    }
  }

  // TODO: notification on detach
  m_currentHeight = height == 0 ? 0 : height - 1;

  return deletedTransactions;
}

namespace {
  template<typename C, typename T>
  void updateVisibility(C& collection, const T& range, bool visible) {
    for (auto it = range.first; it != range.second; ++it) {
      auto updated = *it;
      updated.visible = visible;
      collection.replace(it, updated);
    }
  }
}

/**
 * \pre m_mutex is locked.
 */
void TransfersContainer::updateTransfersVisibility(const KeyImage& keyImage) {
  auto& unconfirmedIndex = m_unconfirmedTransfers.get<SpentOutputDescriptorIndex>();
  auto& availableIndex = m_availableTransfers.get<SpentOutputDescriptorIndex>();
  auto& spentIndex = m_spentTransfers.get<SpentOutputDescriptorIndex>();

  SpentOutputDescriptor descriptor(&keyImage);
  auto unconfirmedRange = unconfirmedIndex.equal_range(descriptor);
  auto availableRange = availableIndex.equal_range(descriptor);
  auto spentRange = spentIndex.equal_range(descriptor);

  size_t unconfirmedCount = std::distance(unconfirmedRange.first, unconfirmedRange.second);
  size_t availableCount = std::distance(availableRange.first, availableRange.second);
  size_t spentCount = std::distance(spentRange.first, spentRange.second);
  assert(spentCount == 0 || spentCount == 1);

  if (spentCount > 0) {
    updateVisibility(unconfirmedIndex, unconfirmedRange, false);
    updateVisibility(availableIndex, availableRange, false);
    updateVisibility(spentIndex, spentRange, true);
  } else if (availableCount > 0) {
    updateVisibility(unconfirmedIndex, unconfirmedRange, false);
    updateVisibility(availableIndex, availableRange, false);

    auto iteratorList = createTransferIteratorList(availableRange);
    auto earliestTransferIt = iteratorList.minElement();
    assert(earliestTransferIt != availableRange.second);

    auto earliestTransfer = *earliestTransferIt;
    earliestTransfer.visible = true;
    availableIndex.replace(earliestTransferIt, earliestTransfer);
  } else {
    updateVisibility(unconfirmedIndex, unconfirmedRange, unconfirmedCount == 1);
  }
}

bool TransfersContainer::advanceHeight(uint32_t height) {
  std::lock_guard<std::mutex> lk(m_mutex);

  if (m_currentHeight <= height) {
    m_currentHeight = height;
    return true;
  }

  return false;
}

size_t TransfersContainer::transfersCount() const {
  std::lock_guard<std::mutex> lk(m_mutex);
  return m_unconfirmedTransfers.size() + m_availableTransfers.size() + m_spentTransfers.size();
}

size_t TransfersContainer::transactionsCount() const {
  std::lock_guard<std::mutex> lk(m_mutex);
  return m_transactions.size();
}

uint64_t TransfersContainer::balance(uint32_t flags) const {
  std::lock_guard<std::mutex> lk(m_mutex);
  uint64_t amount = 0;

  for (const auto& t : m_availableTransfers) {
    if (t.visible && isIncluded(t, flags)) {
      amount += t.amount;
    }
  }

  if ((flags & IncludeStateLocked) != 0) {
    for (const auto& t : m_unconfirmedTransfers) {
      if (t.visible && isIncluded(t.type, IncludeStateLocked, flags)) {
        amount += t.amount;
      }
    }
  }

  return amount;
}

void TransfersContainer::getOutputs(std::vector<TransactionOutputInformation>& transfers, uint32_t flags) const {
  std::lock_guard<std::mutex> lk(m_mutex);
  for (const auto& t : m_availableTransfers) {
    if (t.visible && isIncluded(t, flags)) {
      transfers.push_back(t);
    }
  }

  if ((flags & IncludeStateLocked) != 0) {
    for (const auto& t : m_unconfirmedTransfers) {
      if (t.visible && isIncluded(t.type, IncludeStateLocked, flags)) {
        transfers.push_back(t);
      }
    }
  }
}

bool TransfersContainer::getTransactionInformation(const Hash& transactionHash, TransactionInformation& info, uint64_t* amountIn, uint64_t* amountOut) const {
  std::lock_guard<std::mutex> lk(m_mutex);
  auto it = m_transactions.find(transactionHash);
  if (it == m_transactions.end()) {
    return false;
  }

  info = *it;

  if (amountOut != nullptr) {
    *amountOut = 0;

    if (info.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      auto unconfirmedOutputsRange = m_unconfirmedTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
      for (auto it = unconfirmedOutputsRange.first; it != unconfirmedOutputsRange.second; ++it) {
        *amountOut += it->amount;
      }
    } else {
      auto availableOutputsRange = m_availableTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
      for (auto it = availableOutputsRange.first; it != availableOutputsRange.second; ++it) {
        *amountOut += it->amount;
      }

      auto spentOutputsRange = m_spentTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
      for (auto it = spentOutputsRange.first; it != spentOutputsRange.second; ++it) {
        *amountOut += it->amount;
      }
    }
  }

  if (amountIn != nullptr) {
    *amountIn = 0;
    auto rangeInputs = m_spentTransfers.get<SpendingTransactionIndex>().equal_range(transactionHash);
    for (auto it = rangeInputs.first; it != rangeInputs.second; ++it) {
      *amountIn += it->amount;
    }
  }

  return true;
}

std::vector<TransactionOutputInformation> TransfersContainer::getTransactionOutputs(const Hash& transactionHash,
                                                                                    uint32_t flags) const {
  std::lock_guard<std::mutex> lk(m_mutex);

  std::vector<TransactionOutputInformation> result;

  auto availableRange = m_availableTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
  for (auto i = availableRange.first; i != availableRange.second; ++i) {
    const auto& t = *i;
    if (isIncluded(t, flags)) {
      result.push_back(t);
    }
  }

  if ((flags & IncludeStateLocked) != 0) {
    auto unconfirmedRange = m_unconfirmedTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
    for (auto i = unconfirmedRange.first; i != unconfirmedRange.second; ++i) {
      if (isIncluded(i->type, IncludeStateLocked, flags)) {
        result.push_back(*i);
      }
    }
  }

  if ((flags & IncludeStateSpent) != 0) {
    auto spentRange = m_spentTransfers.get<ContainingTransactionIndex>().equal_range(transactionHash);
    for (auto i = spentRange.first; i != spentRange.second; ++i) {
      if (isIncluded(i->type, IncludeStateAll, flags)) {
        result.push_back(*i);
      }
    }
  }

  return result;
}

std::vector<TransactionOutputInformation> TransfersContainer::getTransactionInputs(const Hash& transactionHash, uint32_t flags) const {
  //only type flags are feasible
  assert((flags & IncludeStateAll) == 0);
  flags |= IncludeStateUnlocked;

  std::lock_guard<std::mutex> lk(m_mutex);

  std::vector<TransactionOutputInformation> result;
  auto transactionInputsRange = m_spentTransfers.get<SpendingTransactionIndex>().equal_range(transactionHash);
  for (auto it = transactionInputsRange.first; it != transactionInputsRange.second; ++it) {
    if (isIncluded(it->type, IncludeStateUnlocked, flags)) {
      result.push_back(*it);
    }
  }

  return result;
}

void TransfersContainer::getUnconfirmedTransactions(std::vector<Crypto::Hash>& transactions) const {
  std::lock_guard<std::mutex> lk(m_mutex);
  transactions.clear();
  for (auto& element : m_transactions) {
    if (element.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      transactions.push_back(*reinterpret_cast<const Crypto::Hash*>(&element.transactionHash));
    }
  }
}

std::vector<TransactionSpentOutputInformation> TransfersContainer::getSpentOutputs() const {
  std::lock_guard<std::mutex> lk(m_mutex);

  std::vector<TransactionSpentOutputInformation> spentOutputs;

  spentOutputs.reserve(m_spentTransfers.size());

  for (const auto& o : m_spentTransfers) {
    TransactionSpentOutputInformation spentOutput;
    static_cast<TransactionOutputInformation&>(spentOutput) = o;

    spentOutput.spendingBlockHeight = o.spendingBlock.height;
    spentOutput.timestamp = o.spendingBlock.timestamp;
    spentOutput.spendingTransactionHash = o.spendingTransactionHash;
    spentOutput.keyImage = o.keyImage;
    spentOutput.inputInTransaction = o.inputInTransaction;

    spentOutputs.push_back(spentOutput);
  }

  return spentOutputs;
}

void TransfersContainer::save(std::ostream& os) {
  std::lock_guard<std::mutex> lk(m_mutex);
  StdOutputStream stream(os);
  CryptoNote::BinaryOutputStreamSerializer s(stream);

  s(const_cast<uint32_t&>(TRANSFERS_CONTAINER_STORAGE_VERSION), "version");

  s(m_currentHeight, "height");
  writeSequence<TransactionInformation>(m_transactions.begin(), m_transactions.end(), "transactions", s);
  writeSequence<TransactionOutputInformationEx>(m_unconfirmedTransfers.begin(), m_unconfirmedTransfers.end(), "unconfirmedTransfers", s);
  writeSequence<TransactionOutputInformationEx>(m_availableTransfers.begin(), m_availableTransfers.end(), "availableTransfers", s);
  writeSequence<SpentTransactionOutput>(m_spentTransfers.begin(), m_spentTransfers.end(), "spentTransfers", s);
}

void TransfersContainer::load(std::istream& in) {
  std::lock_guard<std::mutex> lk(m_mutex);
  StdInputStream stream(in);
  CryptoNote::BinaryInputStreamSerializer s(stream);

  uint32_t version = 0;
  s(version, "version");

  if (version > TRANSFERS_CONTAINER_STORAGE_VERSION) {
    auto message = "Failed to load: unsupported version";
    m_logger(ERROR, BRIGHT_RED) << message << ", version " << version << ", supported version " << TRANSFERS_CONTAINER_STORAGE_VERSION;
    throw std::runtime_error(message);
  }

  uint32_t currentHeight = 0;
  TransactionMultiIndex transactions;
  UnconfirmedTransfersMultiIndex unconfirmedTransfers;
  AvailableTransfersMultiIndex availableTransfers;
  SpentTransfersMultiIndex spentTransfers;

  s(currentHeight, "height");
  readSequence<TransactionInformation>(std::inserter(transactions, transactions.end()), "transactions", s);
  readSequence<TransactionOutputInformationEx>(std::inserter(unconfirmedTransfers, unconfirmedTransfers.end()), "unconfirmedTransfers", s);
  readSequence<TransactionOutputInformationEx>(std::inserter(availableTransfers, availableTransfers.end()), "availableTransfers", s);
  readSequence<SpentTransactionOutput>(std::inserter(spentTransfers, spentTransfers.end()), "spentTransfers", s);

  m_currentHeight = currentHeight;
  m_transactions = std::move(transactions);
  m_unconfirmedTransfers = std::move(unconfirmedTransfers);
  m_availableTransfers = std::move(availableTransfers);
  m_spentTransfers = std::move(spentTransfers);

  // Repair the container if it was broken while handling addTransaction() in previous version of the code
  // Hope it isn't necessary anymore
  //repair();
}

void TransfersContainer::repair() {
  size_t deletedInputCount = 0;
  for (auto it = m_spentTransfers.begin(); it != m_spentTransfers.end();) {
    assert(it->blockHeight != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);
    assert(it->globalOutputIndex != UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX);

    if (m_transactions.count(it->spendingTransactionHash) == 0) {
      bool isInputConfirmed = it->spendingBlock.height != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT;
      m_logger(WARNING, BRIGHT_YELLOW) << "Orphan input found, remove it and return output spent by them to available outputs:\n" <<
        "    input       " <<
        ": block " << std::setw(7) << (isInputConfirmed ? static_cast<int32_t>(it->spendingBlock.height) : -1) <<
        ", transaction index " << std::setw(2) << (isInputConfirmed ? static_cast<int32_t>(it->spendingBlock.transactionIndex) : -1) <<
        ", transaction hash " << it->spendingTransactionHash <<
        ", input " << std::setw(3) << it->inputInTransaction << '\n' <<
        "    spent output" <<
        ": block " << std::setw(7) << it->blockHeight <<
        ", transaction index " << std::setw(2) << it->transactionIndex <<
        ", transaction hash " << it->transactionHash <<
        ", output " << std::setw(2) << it->outputInTransaction;

      auto result = m_availableTransfers.emplace(static_cast<const TransactionOutputInformationEx&>(*it));
      assert(result.second);
      it = m_spentTransfers.erase(it);

      if (result.first->type == TransactionTypes::OutputType::Key) {
        updateTransfersVisibility(result.first->keyImage);
      }

      ++deletedInputCount;
    } else {
      ++it;
    }
  }

  size_t deletedUnconfirmedOutputCount = 0;
  for (auto it = m_unconfirmedTransfers.begin(); it != m_unconfirmedTransfers.end();) {
    assert(it->blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);

    if (m_transactions.count(it->transactionHash) == 0) {
      m_logger(WARNING, BRIGHT_YELLOW) << "Orphan unconfirmed output found, remove it" <<
        ", transaction hash " << it->transactionHash <<
        ", output " << std::setw(2) << it->outputInTransaction <<
        ", amount " << m_currency.formatAmount(it->amount);

      if (it->type == TransactionTypes::OutputType::Key) {
        KeyImage keyImage = it->keyImage;
        it = m_unconfirmedTransfers.erase(it);
        updateTransfersVisibility(keyImage);
      } else {
        it = m_unconfirmedTransfers.erase(it);
      }

      ++deletedUnconfirmedOutputCount;
    } else {
      ++it;
    }
  }

  size_t deletedAvailableOutputCount = 0;
  for (auto it = m_availableTransfers.begin(); it != m_availableTransfers.end();) {
    assert(it->blockHeight != WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT);

    if (m_transactions.count(it->transactionHash) == 0) {
      m_logger(WARNING, BRIGHT_YELLOW) << "Orphan output found, remove it" <<
        ", block " << std::setw(7) << it->blockHeight <<
        ", transaction index " << std::setw(2) << it->transactionIndex <<
        ", transaction hash " << it->transactionHash <<
        ", output " << std::setw(2) << it->outputInTransaction <<
        ", amount " << m_currency.formatAmount(it->amount);

      if (it->type == TransactionTypes::OutputType::Key) {
        KeyImage keyImage = it->keyImage;
        it = m_availableTransfers.erase(it);
        updateTransfersVisibility(keyImage);
      } else {
        it = m_availableTransfers.erase(it);
      }

      ++deletedAvailableOutputCount;
    } else {
      ++it;
    }
  }

  if (deletedInputCount + deletedUnconfirmedOutputCount + deletedAvailableOutputCount > 0) {
    m_logger(WARNING, BRIGHT_YELLOW) << "Repair finished:\n" <<
      "    Deleted inputs " << deletedInputCount << ", total inputs " << m_spentTransfers.size() << '\n' <<
      "    Deleted unconfirmed outputs " << deletedUnconfirmedOutputCount << ", total unconfirmed outputs " << m_unconfirmedTransfers.size() << '\n' <<
      "    Deleted available outputs " << deletedAvailableOutputCount << ", total available outputs " << m_availableTransfers.size();
  } else {
    m_logger(DEBUGGING) << "Repair finished";
  }
}

bool TransfersContainer::isSpendTimeUnlocked(uint64_t unlockTime) const {
  if (unlockTime < m_currency.maxBlockHeight()) {
    // interpret as block index
    return m_currentHeight + m_currency.lockedTxAllowedDeltaBlocks() >= unlockTime;
  } else {
    //interpret as time
    uint64_t current_time = static_cast<uint64_t>(time(NULL));
    return current_time + m_currency.lockedTxAllowedDeltaSeconds() >= unlockTime;
  }

  return false;
}

bool TransfersContainer::isIncluded(const TransactionOutputInformationEx& info, uint32_t flags) const {
  uint32_t state;
  if (info.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT || !isSpendTimeUnlocked(info.unlockTime)) {
    state = IncludeStateLocked;
  } else if (m_currentHeight < info.blockHeight + m_transactionSpendableAge) {
    state = IncludeStateSoftLocked;
  } else {
    state = IncludeStateUnlocked;
  }

  return isIncluded(info.type, state, flags);
}

bool TransfersContainer::isIncluded(TransactionTypes::OutputType type, uint32_t state, uint32_t flags) {
  return
    // filter by type
    (
    ((flags & IncludeTypeKey) != 0            && type == TransactionTypes::OutputType::Key)
    )
    &&
    // filter by state
    ((flags & state) != 0);
}

}
