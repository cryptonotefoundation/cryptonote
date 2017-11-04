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

#include "BlockchainCache.h"

#include <fstream>
#include <tuple>

#include <boost/functional/hash.hpp>

#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Common/ShuffleGenerator.h"

#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/BlockchainStorage.h"
#include "CryptoNoteCore/TransactionExtra.h"

#include "Serialization/SerializationOverloads.h"
#include "TransactionValidatiorState.h"

namespace CryptoNote {

namespace {

UseGenesis addGenesisBlock = UseGenesis(true);
UseGenesis skipGenesisBlock = UseGenesis(false);

template <class T, class F>
void splitGlobalIndexes(T& sourceContainer, T& destinationContainer, uint32_t splitBlockIndex, F lowerBoundFunction) {
  for (auto it = sourceContainer.begin(); it != sourceContainer.end();) {
    auto newCacheOutputsIteratorStart =
        lowerBoundFunction(it->second.outputs.begin(), it->second.outputs.end(), splitBlockIndex);

    auto& indexesForAmount = destinationContainer[it->first];
    auto newCacheOutputsCount =
        static_cast<uint32_t>(std::distance(newCacheOutputsIteratorStart, it->second.outputs.end()));
    indexesForAmount.outputs.reserve(newCacheOutputsCount);

    indexesForAmount.startIndex = it->second.startIndex + static_cast<uint32_t>(it->second.outputs.size()) - newCacheOutputsCount;

    std::move(newCacheOutputsIteratorStart, it->second.outputs.end(), std::back_inserter(indexesForAmount.outputs));
    it->second.outputs.erase(newCacheOutputsIteratorStart, it->second.outputs.end());

    if (indexesForAmount.outputs.empty()) {
      destinationContainer.erase(it->first);
    }

    if (it->second.outputs.empty()) {
      // if we gave all of our outputs we don't need this amount entry any more
      it = sourceContainer.erase(it);
    } else {
      ++it;
    }
  }
}
}

void SpentKeyImage::serialize(ISerializer& s) {
  s(blockIndex, "block_index");
  s(keyImage, "key_image");
}

void CachedTransactionInfo::serialize(ISerializer& s) {
  s(blockIndex, "block_index");
  s(transactionIndex, "transaction_index");
  s(transactionHash, "transaction_hash");
  s(unlockTime, "unlock_time");
  s(outputs, "outputs");
  s(globalIndexes, "global_indexes");
}

void CachedBlockInfo::serialize(ISerializer& s) {
  s(blockHash, "block_hash");
  s(timestamp, "timestamp");
  s(blockSize, "block_size");
  s(cumulativeDifficulty, "cumulative_difficulty");
  s(alreadyGeneratedCoins, "already_generated_coins");
  s(alreadyGeneratedTransactions, "already_generated_transaction_count");
}

void OutputGlobalIndexesForAmount::serialize(ISerializer& s) {
  s(startIndex, "start_index");
  s(outputs, "outputs");
}

void MultisignatureOutputState::serialize(ISerializer& s) {
  s(output, "output");
}

void MultisignatureIndexes::serialize(ISerializer& s) {
  s(startIndex, "start_index");
  s(outputs, "outputs");
}

void PaymentIdTransactionHashPair::serialize(ISerializer& s) {
  s(paymentId, "payment_id");
  s(transactionHash, "transaction_hash");
}

bool serialize(PackedOutIndex& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializer(value.packedValue, name);
}

BlockchainCache::BlockchainCache(const std::string& filename, const Currency& currency, Logging::ILogger& logger_,
                                 IBlockchainCache* parent, uint32_t splitBlockIndex)
    : filename(filename), currency(currency), logger(logger_, "BlockchainCache"), parent(parent), storage(new BlockchainStorage(100)) {
  if (parent == nullptr) {
    startIndex = 0;

    const CachedBlock genesisBlock(currency.genesisBlock());

    uint64_t minerReward = 0;
    for (const TransactionOutput& output : genesisBlock.getBlock().baseTransaction.outputs) {
      minerReward += output.amount;
    }

    assert(minerReward > 0);

    uint64_t coinbaseTransactionSize = getObjectBinarySize(genesisBlock.getBlock().baseTransaction);
    assert(coinbaseTransactionSize < std::numeric_limits<uint64_t>::max());

    std::vector<CachedTransaction> transactions;
    TransactionValidatorState validatorState;
    doPushBlock(genesisBlock, transactions, validatorState, coinbaseTransactionSize, minerReward, 1, {toBinaryArray(genesisBlock.getBlock())});
  } else {
    startIndex = splitBlockIndex;
  }

  logger(Logging::DEBUGGING) << "BlockchainCache with start block index: " << startIndex << " created";
}

void BlockchainCache::pushBlock(const CachedBlock& cachedBlock,
                                const std::vector<CachedTransaction>& cachedTransactions,
                                const TransactionValidatorState& validatorState, size_t blockSize,
                                uint64_t generatedCoins, Difficulty blockDifficulty, RawBlock&& rawBlock) {
  //we have to call this function from constructor so it has to be non-virtual
  doPushBlock(cachedBlock, cachedTransactions, validatorState, blockSize, generatedCoins, blockDifficulty, std::move(rawBlock));
}

void BlockchainCache::doPushBlock(const CachedBlock& cachedBlock,
                                const std::vector<CachedTransaction>& cachedTransactions,
                                const TransactionValidatorState& validatorState, size_t blockSize,
                                uint64_t generatedCoins, Difficulty blockDifficulty, RawBlock&& rawBlock) {
  logger(Logging::DEBUGGING) << "Pushing block " << cachedBlock.getBlockHash() << " at index " << cachedBlock.getBlockIndex();

  assert(blockSize > 0);
  assert(blockDifficulty > 0);

  Difficulty cumulativeDifficulty = 0;
  uint64_t alreadyGeneratedCoins = 0;
  uint64_t alreadyGeneratedTransactions = 0;

  if (getBlockCount() == 0) {
    if (parent != nullptr) {
      cumulativeDifficulty = parent->getCurrentCumulativeDifficulty(cachedBlock.getBlockIndex() - 1);
      alreadyGeneratedCoins = parent->getAlreadyGeneratedCoins(cachedBlock.getBlockIndex() - 1);
      alreadyGeneratedTransactions = parent->getAlreadyGeneratedTransactions(cachedBlock.getBlockIndex() - 1);
    }

    cumulativeDifficulty += blockDifficulty;
    alreadyGeneratedCoins += generatedCoins;
    alreadyGeneratedTransactions += cachedTransactions.size() + 1;
  } else {
    auto& lastBlockInfo = blockInfos.get<BlockIndexTag>().back();

    cumulativeDifficulty = lastBlockInfo.cumulativeDifficulty + blockDifficulty;
    alreadyGeneratedCoins = lastBlockInfo.alreadyGeneratedCoins + generatedCoins;
    alreadyGeneratedTransactions = lastBlockInfo.alreadyGeneratedTransactions + cachedTransactions.size() + 1;
  }

  CachedBlockInfo blockInfo;
  blockInfo.blockHash = cachedBlock.getBlockHash();
  blockInfo.alreadyGeneratedCoins = alreadyGeneratedCoins;
  blockInfo.alreadyGeneratedTransactions = alreadyGeneratedTransactions;
  blockInfo.cumulativeDifficulty = cumulativeDifficulty;
  blockInfo.blockSize = static_cast<uint32_t>(blockSize);
  blockInfo.timestamp = cachedBlock.getBlock().timestamp;

  assert(!hasBlock(blockInfo.blockHash));

  blockInfos.get<BlockIndexTag>().emplace_back(std::move(blockInfo));

  auto blockIndex = cachedBlock.getBlockIndex();
  assert(blockIndex == blockInfos.size() + startIndex - 1);

  for (const auto& keyImage : validatorState.spentKeyImages) {
    addSpentKeyImage(keyImage, blockIndex);
  }

  logger(Logging::DEBUGGING) << "Added " << validatorState.spentKeyImages.size() << " spent key images";

  if (!validatorState.spentMultisignatureGlobalIndexes.empty()) {
    auto& array = spentMultisigOutputsByBlock[blockIndex];
    array.reserve(array.size() + validatorState.spentMultisignatureGlobalIndexes.size());
  }

  for (const auto& multisignature : validatorState.spentMultisignatureGlobalIndexes) {
    addSpentMultisignature(multisignature.first, multisignature.second, blockIndex);
  }

  logger(Logging::DEBUGGING) << "Added " << validatorState.spentMultisignatureGlobalIndexes.size() << " spent multisignatures";

  assert(cachedTransactions.size() <= std::numeric_limits<uint16_t>::max());

  auto transactionBlockIndex = 0;
  auto baseTransaction = cachedBlock.getBlock().baseTransaction;
  pushTransaction(CachedTransaction(std::move(baseTransaction)), blockIndex, transactionBlockIndex++);

  for (auto& cachedTransaction : cachedTransactions) {
    pushTransaction(cachedTransaction, blockIndex, transactionBlockIndex++);
  }

  storage->pushBlock(std::move(rawBlock));

  logger(Logging::DEBUGGING) << "Block " << cachedBlock.getBlockHash() << " successfully pushed";
}

PushedBlockInfo BlockchainCache::getPushedBlockInfo(uint32_t blockIndex) const {
  assert(blockIndex >= startIndex);
  assert(blockIndex < startIndex + getBlockCount());

  auto localIndex = blockIndex - startIndex;
  const auto& cachedBlock = blockInfos.get<BlockIndexTag>()[localIndex];

  PushedBlockInfo pushedBlockInfo;
  pushedBlockInfo.rawBlock = storage->getBlockByIndex(localIndex);
  pushedBlockInfo.blockSize = cachedBlock.blockSize;

  if (blockIndex > startIndex) {
    const auto& previousBlock = blockInfos.get<BlockIndexTag>()[localIndex - 1];
    pushedBlockInfo.blockDifficulty = cachedBlock.cumulativeDifficulty - previousBlock.cumulativeDifficulty;
    pushedBlockInfo.generatedCoins = cachedBlock.alreadyGeneratedCoins - previousBlock.alreadyGeneratedCoins;
  } else {
    if (parent == nullptr) {
      pushedBlockInfo.blockDifficulty = cachedBlock.cumulativeDifficulty;
      pushedBlockInfo.generatedCoins = cachedBlock.alreadyGeneratedCoins;
    } else {
      Difficulty cumulativeDifficulty = parent->getLastCumulativeDifficulties(1, startIndex - 1, addGenesisBlock)[0];
      uint64_t alreadyGeneratedCoins = parent->getAlreadyGeneratedCoins(startIndex - 1);

      pushedBlockInfo.blockDifficulty = cachedBlock.cumulativeDifficulty - cumulativeDifficulty;
      pushedBlockInfo.generatedCoins = cachedBlock.alreadyGeneratedCoins - alreadyGeneratedCoins;
    }
  }

  pushedBlockInfo.validatorState = fillOutputsSpentByBlock(blockIndex);

  return pushedBlockInfo;
}

// Returns upper part of segment. [this] remains lower part.
// All of indexes on blockIndex == splitBlockIndex belong to upper part
// TODO: first move containers to new cache, then copy elements back. This can be much more effective, cause we usualy
// split blockchain near its top.
std::unique_ptr<IBlockchainCache> BlockchainCache::split(uint32_t splitBlockIndex) {
  logger(Logging::DEBUGGING) << "Splitting at block index: " << splitBlockIndex << ", top block index: " << getTopBlockIndex();

  assert(splitBlockIndex > startIndex);
  assert(splitBlockIndex <= getTopBlockIndex());

  std::unique_ptr<BlockchainStorage> newStorage = storage->splitStorage(splitBlockIndex - startIndex);

  std::unique_ptr<BlockchainCache> newCache(
      new BlockchainCache(filename, currency, logger.getLogger(), this, splitBlockIndex));

  newCache->storage = std::move(newStorage);

  splitSpentKeyImages(*newCache, splitBlockIndex);
  splitTransactions(*newCache, splitBlockIndex);
  splitBlocks(*newCache, splitBlockIndex);
  splitKeyOutputsGlobalIndexes(*newCache, splitBlockIndex);
  splitMultiSignatureOutputsGlobalIndexes(*newCache, splitBlockIndex);

  fixChildrenParent(newCache.get());
  newCache->children = children;
  children = { newCache.get() };

  logger(Logging::DEBUGGING) << "Split successfully completed";
  return std::move(newCache);
}

void BlockchainCache::splitSpentKeyImages(BlockchainCache& newCache, uint32_t splitBlockIndex) {
  //Key images with blockIndex == splitBlockIndex remain in upper segment
  auto& imagesIndex = spentKeyImages.get<BlockIndexTag>();
  auto lowerBound = imagesIndex.lower_bound(splitBlockIndex);

  newCache.spentKeyImages.get<BlockIndexTag>().insert(lowerBound, imagesIndex.end());
  imagesIndex.erase(lowerBound, imagesIndex.end());

  logger(Logging::DEBUGGING) << "Spent key images split completed";
}

void BlockchainCache::splitTransactions(BlockchainCache& newCache, uint32_t splitBlockIndex) {
  auto& transactionsIndex = transactions.get<BlockIndexTag>();
  auto lowerBound = transactionsIndex.lower_bound(splitBlockIndex);

  for (auto it = lowerBound; it != transactionsIndex.end(); ++it) {
    removePaymentId(it->transactionHash, newCache);
  }

  newCache.transactions.get<BlockIndexTag>().insert(lowerBound, transactionsIndex.end());
  transactionsIndex.erase(lowerBound, transactionsIndex.end());

  logger(Logging::DEBUGGING) << "Transactions split completed";
}

void BlockchainCache::removePaymentId(const Crypto::Hash& transactionHash, BlockchainCache& newCache) {
  auto& index = paymentIds.get<TransactionHashTag>();
  auto it = index.find(transactionHash);

  if (it == index.end()) {
    return;
  }

  newCache.paymentIds.emplace(*it);
  index.erase(it);
}

void BlockchainCache::splitBlocks(BlockchainCache& newCache, uint32_t splitBlockIndex) {
  auto& blocksIndex = blockInfos.get<BlockIndexTag>();
  auto bound = std::next(blocksIndex.begin(), splitBlockIndex - startIndex);
  std::move(bound, blocksIndex.end(), std::back_inserter(newCache.blockInfos.get<BlockIndexTag>()));
  blocksIndex.erase(bound, blocksIndex.end());

  logger(Logging::DEBUGGING) << "Blocks split completed";
}

void BlockchainCache::splitKeyOutputsGlobalIndexes(BlockchainCache& newCache, uint32_t splitBlockIndex) {
  auto lowerBoundFunction = [](std::vector<PackedOutIndex>::iterator begin, std::vector<PackedOutIndex>::iterator end,
                               uint32_t splitBlockIndex) -> std::vector<PackedOutIndex>::iterator {
    return std::lower_bound(begin, end, splitBlockIndex, [](PackedOutIndex outputIndex, uint32_t splitIndex) {
      // all outputs in it->second.outputs are sorted according to blockIndex + transactionIndex
      return outputIndex.blockIndex < splitIndex;
    });
  };

  splitGlobalIndexes(keyOutputsGlobalIndexes, newCache.keyOutputsGlobalIndexes, splitBlockIndex, lowerBoundFunction);
  logger(Logging::DEBUGGING) << "Key output global indexes split successfully completed";
}

void BlockchainCache::splitMultiSignatureOutputsGlobalIndexes(BlockchainCache& newCache, uint32_t splitBlockIndex) {
  auto lowerBoundFunction = [](std::vector<MultisignatureOutputState>::iterator begin,
                               std::vector<MultisignatureOutputState>::iterator end,
                               uint32_t splitBlockIndex) -> std::vector<MultisignatureOutputState>::iterator {
    return std::lower_bound(begin, end, splitBlockIndex,
                            [](MultisignatureOutputState outputState, uint32_t splitIndex) {
                              // all outputs in it->second.outputs are sorted according to blockIndex + transactionIndex
                              return outputState.output.blockIndex < splitIndex;
                            });
  };

  splitGlobalIndexes(multisignatureStorage, newCache.multisignatureStorage, splitBlockIndex, lowerBoundFunction);

  // move spent mutisig by block
  auto it = spentMultisigOutputsByBlock.lower_bound(splitBlockIndex);
  std::move(it, spentMultisigOutputsByBlock.end(),
            std::inserter(newCache.spentMultisigOutputsByBlock, newCache.spentMultisigOutputsByBlock.end()));

  logger(Logging::DEBUGGING) << "Multisignature output global indexes split successfully completed";
}

void BlockchainCache::addSpentKeyImage(const Crypto::KeyImage& keyImage, uint32_t blockIndex) {
  assert(!checkIfSpent(keyImage, blockIndex - 1)); //Changed from "assert(!checkIfSpent(keyImage, blockIndex));"
                                                   //to prevent fail when pushing block from DatabaseBlockchainCache.
                                                   //In case of pushing external block double spend within block
                                                   //should be checked by Core.
  spentKeyImages.get<BlockIndexTag>().emplace(SpentKeyImage{blockIndex, keyImage});
}

void BlockchainCache::addSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) {
  assert(!checkIfSpentMultisignature(amount, globalIndex, blockIndex - 1)); //Changed from "assert(!checkIfSpentMultisignature(amount, globalIndex, blockIndex))"
                                                                            //to prevent fail when pushing block from DatabaseBlockchainCache.
                                                                            //In case of pushing external block double spend within block
                                                                            //should be checked by Core.
  spentMultisigOutputsByBlock[blockIndex].push_back({amount, globalIndex});
  spentMultisigOutputs.insert({amount, globalIndex});
}

std::vector<Crypto::Hash> BlockchainCache::getTransactionHashes() const {
  auto& txInfos = transactions.get<TransactionHashTag>();
  std::vector<Crypto::Hash> hashes;
  for (auto& tx : txInfos) {
    // skip base transaction
    if (tx.transactionIndex != 0) {
      hashes.emplace_back(tx.transactionHash);
    }
  }
  return hashes;
}

void BlockchainCache::pushTransaction(const CachedTransaction& cachedTransaction, uint32_t blockIndex,
                                      uint16_t transactionInBlockIndex) {
  logger(Logging::DEBUGGING) << "Adding transaction " << cachedTransaction.getTransactionHash() << " at block " << blockIndex << ", index in block " << transactionInBlockIndex;

  const auto& tx = cachedTransaction.getTransaction();

  CachedTransactionInfo transactionCacheInfo;
  transactionCacheInfo.blockIndex = blockIndex;
  transactionCacheInfo.transactionIndex = transactionInBlockIndex;
  transactionCacheInfo.transactionHash = cachedTransaction.getTransactionHash();
  transactionCacheInfo.unlockTime = tx.unlockTime;

  assert(tx.outputs.size() <= std::numeric_limits<uint16_t>::max());

  transactionCacheInfo.globalIndexes.reserve(tx.outputs.size());
  transactionCacheInfo.outputs.reserve(tx.outputs.size());

  logger(Logging::DEBUGGING) << "Adding " << tx.outputs.size() << " transaction outputs";
  auto outputCount = 0;
  for (auto& output : tx.outputs) {
    transactionCacheInfo.outputs.push_back(output.target);

    PackedOutIndex poi;
    poi.blockIndex = blockIndex;
    poi.transactionIndex = transactionInBlockIndex;
    poi.outputIndex = outputCount++;

    if (output.target.type() == typeid(KeyOutput)) {
      transactionCacheInfo.globalIndexes.push_back(insertKeyOutputToGlobalIndex(output.amount, poi, blockIndex));
    } else if (output.target.type() == typeid(MultisignatureOutput)) {
      transactionCacheInfo.globalIndexes.push_back(insertMultisignatureToGlobalIndex(output.amount, poi, blockIndex));
    }
  }

  assert(transactions.get<TransactionHashTag>().count(transactionCacheInfo.transactionHash) == 0);
  transactions.get<TransactionInBlockTag>().emplace(std::move(transactionCacheInfo));

  PaymentIdTransactionHashPair paymentIdTransactionHash;
  if (!getPaymentIdFromTxExtra(tx.extra, paymentIdTransactionHash.paymentId)) {
    logger(Logging::DEBUGGING) << "Transaction " << cachedTransaction.getTransactionHash() << " successfully added";
    return;
  }

  logger(Logging::DEBUGGING) << "Payment id found: " << paymentIdTransactionHash.paymentId;

  paymentIdTransactionHash.transactionHash = cachedTransaction.getTransactionHash();
  paymentIds.emplace(std::move(paymentIdTransactionHash));
  logger(Logging::DEBUGGING) << "Transaction " << cachedTransaction.getTransactionHash() << " successfully added";
}

uint32_t BlockchainCache::insertKeyOutputToGlobalIndex(uint64_t amount, PackedOutIndex output, uint32_t blockIndex) {
  auto pair = keyOutputsGlobalIndexes.emplace(amount, OutputGlobalIndexesForAmount{});
  auto& indexEntry = pair.first->second;
  indexEntry.outputs.push_back(output);
  if (pair.second && parent != nullptr) {
    indexEntry.startIndex = static_cast<uint32_t>(parent->getKeyOutputsCountForAmount(amount, blockIndex));
    logger(Logging::DEBUGGING) << "Key output count for amount " << amount << " requested from parent. Returned count: " << indexEntry.startIndex;
  }

  return indexEntry.startIndex + static_cast<uint32_t>(indexEntry.outputs.size()) - 1;
}

uint32_t BlockchainCache::insertMultisignatureToGlobalIndex(uint64_t amount, PackedOutIndex output, uint32_t blockIndex) {
  auto pair = multisignatureStorage.emplace(amount, MultisignatureIndexes{});
  auto& indexEntry = pair.first->second;
  indexEntry.outputs.push_back({output});
  if (pair.second && parent != nullptr) {
    indexEntry.startIndex = static_cast<uint32_t>(parent->getMultisignatureCountForAmount(amount, blockIndex));
    logger(Logging::DEBUGGING) << "Multisignature output count for amount " << amount << " requested from parent. Returned count: " << indexEntry.startIndex;
  }

  return indexEntry.startIndex + static_cast<uint32_t>(indexEntry.outputs.size()) - 1;
}

bool BlockchainCache::checkIfSpent(const Crypto::KeyImage& keyImage, uint32_t blockIndex) const {
  if (blockIndex < startIndex) {
    assert(parent != nullptr);
    return parent->checkIfSpent(keyImage, blockIndex);
  }

  auto it = spentKeyImages.get<KeyImageTag>().find(keyImage);
  if (it == spentKeyImages.get<KeyImageTag>().end()) {
    return parent != nullptr ? parent->checkIfSpent(keyImage, blockIndex) : false;
  }

  return it->blockIndex <= blockIndex;
}

bool BlockchainCache::checkIfSpent(const Crypto::KeyImage& keyImage) const {
  if (spentKeyImages.get<KeyImageTag>().count(keyImage) != 0) {
    return true;
  }

  return parent != nullptr && parent->checkIfSpent(keyImage);
}

uint32_t BlockchainCache::getBlockCount() const {
  return static_cast<uint32_t>(blockInfos.size());
}

bool BlockchainCache::hasBlock(const Crypto::Hash& blockHash) const {
  return blockInfos.get<BlockHashTag>().count(blockHash) != 0;
}

uint32_t BlockchainCache::getBlockIndex(const Crypto::Hash& blockHash) const {
//  assert(blockInfos.get<BlockHashTag>().count(blockHash) > 0);
  const auto hashIt = blockInfos.get<BlockHashTag>().find(blockHash);
  if (hashIt == blockInfos.get<BlockHashTag>().end()) {
    throw std::runtime_error("no such block");
  }

  const auto rndIt = blockInfos.project<BlockIndexTag>(hashIt);
  return static_cast<uint32_t>(std::distance(blockInfos.get<BlockIndexTag>().begin(), rndIt)) + startIndex;
}

BlockchainCache::OutputSearchResult
BlockchainCache::findPackedOutForMultisignatureInCurrentSegment(uint64_t amount, uint32_t globalIndex,
                                                                PackedOutIndex& packedOut) const {
  auto outputGlobalIndexesIterator = multisignatureStorage.find(amount);

  if (outputGlobalIndexesIterator != multisignatureStorage.end() &&
      outputGlobalIndexesIterator->second.startIndex <= globalIndex) {

    auto& globalIndexesVector = outputGlobalIndexesIterator->second.outputs;
    if (globalIndex - outputGlobalIndexesIterator->second.startIndex >= globalIndexesVector.size()) {
      return OutputSearchResult::INVALID_ARGUMENT;
    }

    packedOut = globalIndexesVector[globalIndex - outputGlobalIndexesIterator->second.startIndex].output;
    return OutputSearchResult::FOUND;
  }

  return OutputSearchResult::NOT_FOUND;
}

Crypto::Hash BlockchainCache::getBlockHash(uint32_t blockIndex) const {
  if (blockIndex < startIndex) {
    assert(parent != nullptr);
    return parent->getBlockHash(blockIndex);
  }

  assert(blockIndex - startIndex < blockInfos.size());
  return blockInfos.get<BlockIndexTag>()[blockIndex - startIndex].blockHash;
}

std::vector<Crypto::Hash> BlockchainCache::getBlockHashes(uint32_t startBlockIndex, size_t maxCount) const {
  size_t blocksLeft;
  size_t start = 0;
  std::vector<Crypto::Hash> hashes;

  if (startBlockIndex < startIndex) {
    assert(parent != nullptr);
    hashes = parent->getBlockHashes(startBlockIndex, maxCount);
    blocksLeft = std::min(maxCount - hashes.size(), blockInfos.size());
  } else {
    start = startBlockIndex - startIndex;
    blocksLeft = std::min(blockInfos.size() - start, maxCount);
  }

  for (auto i = start; i < start + blocksLeft; ++i) {
    hashes.push_back(blockInfos.get<BlockIndexTag>()[i].blockHash);
  }

  return hashes;
}

IBlockchainCache* BlockchainCache::getParent() const {
  return parent;
}

void BlockchainCache::setParent(IBlockchainCache* p) {
  parent = p;
}

uint32_t BlockchainCache::getStartBlockIndex() const {
  return startIndex;
}

size_t BlockchainCache::getKeyOutputsCountForAmount(uint64_t amount, uint32_t blockIndex) const {
  auto it = keyOutputsGlobalIndexes.find(amount);
  if (it == keyOutputsGlobalIndexes.end()) {
    if (parent == nullptr) {
      return 0;
    }

    return parent->getKeyOutputsCountForAmount(amount, blockIndex);
  }

  auto lowerBound = std::lower_bound(it->second.outputs.begin(), it->second.outputs.end(), blockIndex, [] (const PackedOutIndex& output, uint32_t blockIndex) {
    return output.blockIndex < blockIndex;
  });

  return it->second.startIndex + static_cast<size_t>(std::distance(it->second.outputs.begin(), lowerBound));
}

size_t BlockchainCache::getMultisignatureCountForAmount(uint64_t amount, uint32_t blockIndex) const {
  auto it = multisignatureStorage.find(amount);
  if (it == multisignatureStorage.end()) {
    if (parent == nullptr) {
      return 0;
    }

    return parent->getMultisignatureCountForAmount(amount, blockIndex);
  }

  auto lowerBound = std::lower_bound(it->second.outputs.begin(), it->second.outputs.end(), blockIndex, [] (const MultisignatureOutputState& output, uint32_t blockIndex) {
    return output.output.blockIndex < blockIndex;
  });

  return it->second.startIndex + static_cast<size_t>(std::distance(it->second.outputs.begin(), lowerBound));
}

uint32_t BlockchainCache::getTimestampLowerBoundBlockIndex(uint64_t timestamp) const {
  assert(!blockInfos.empty());

  auto& index = blockInfos.get<BlockIndexTag>();
  if (index.back().timestamp < timestamp) {
    // we don't have it
    throw std::runtime_error("no blocks for this timestamp, too large");
  }

  if (index.front().timestamp < timestamp) {
    // we know the timestamp is in current segment for sure
    auto bound =
        std::lower_bound(index.begin(), index.end(), timestamp,
                         [](const CachedBlockInfo& blockInfo, uint64_t value) { return blockInfo.timestamp < value; });

    return startIndex + static_cast<uint32_t>(std::distance(index.begin(), bound));
  }

  // if index.front().timestamp >= timestamp we can't be sure the timestamp is in current segment
  // so we ask parent. If it doesn't have it then index.front() is the block being searched for.

  if (parent == nullptr) {
    // if given timestamp is less or equal genesis block timestamp
    return 0;
  }

  uint32_t blockIndex = parent->getTimestampLowerBoundBlockIndex(timestamp);
  return blockIndex == INVALID_BLOCK_INDEX ? blockIndex : startIndex;
}

bool BlockchainCache::getTransactionGlobalIndexes(const Crypto::Hash& transactionHash,
                                                  std::vector<uint32_t>& globalIndexes) const {
  auto it = transactions.get<TransactionHashTag>().find(transactionHash);
  if (it == transactions.get<TransactionHashTag>().end()) {
    return false;
  }

  globalIndexes = it->globalIndexes;
  return true;
}

size_t BlockchainCache::getTransactionCount() const {
  size_t count = 0;

  if (parent != nullptr) {
    count = parent->getTransactionCount();
  }

  count += transactions.size();
  return count;
}

RawBlock BlockchainCache::getBlockByIndex(uint32_t index) const {
  return index < startIndex ? parent->getBlockByIndex(index) : storage->getBlockByIndex(index - startIndex);
}

BinaryArray BlockchainCache::getRawTransaction(uint32_t index, uint32_t transactionIndex) const {
  if (index < startIndex) {
    return parent->getRawTransaction(index, transactionIndex);
  } else {
    auto rawBlock = storage->getBlockByIndex(index - startIndex);
    if (transactionIndex == 0) {
      auto block = fromBinaryArray<BlockTemplate>(rawBlock.block);
      return toBinaryArray(block.baseTransaction);
    }

    assert(rawBlock.transactions.size() >= transactionIndex - 1);
    return rawBlock.transactions[transactionIndex - 1];
  }
}

std::vector<BinaryArray>
BlockchainCache::getRawTransactions(const std::vector<Crypto::Hash>& requestedTransactions) const {
  std::vector<Crypto::Hash> misses;
  auto ret = getRawTransactions(requestedTransactions, misses);
  assert(misses.empty());
  return ret;
}

std::vector<BinaryArray> BlockchainCache::getRawTransactions(const std::vector<Crypto::Hash>& requestedTransactions,
                                                             std::vector<Crypto::Hash>& missedTransactions) const {
  std::vector<BinaryArray> res;
  getRawTransactions(requestedTransactions, res, missedTransactions);
  return res;
}

void BlockchainCache::getRawTransactions(const std::vector<Crypto::Hash>& requestedTransactions,
                                         std::vector<BinaryArray>& foundTransactions,
                                         std::vector<Crypto::Hash>& missedTransactions) const {
  auto& index = transactions.get<TransactionHashTag>();
  for (const auto& transactionHash : requestedTransactions) {
    auto it = index.find(transactionHash);
    if (it == index.end()) {
      missedTransactions.emplace_back(transactionHash);
      continue;
    }

    // assert(startIndex <= it->blockIndex);
    foundTransactions.emplace_back(getRawTransaction(it->blockIndex, it->transactionIndex));
  }
}

size_t BlockchainCache::getChildCount() const {
  return children.size();
}

void BlockchainCache::addChild(IBlockchainCache* child) {
  assert(std::find(children.begin(), children.end(), child) == children.end());
  children.push_back(child);
}

bool BlockchainCache::deleteChild(IBlockchainCache* child) {
  auto it = std::find(children.begin(), children.end(), child);
  if (it == children.end()) {
    return false;
  }

  children.erase(it);
  return true;
}

void BlockchainCache::serialize(ISerializer& s) {
  assert(s.type() == ISerializer::OUTPUT);

  uint32_t version = CURRENT_SERIALIZATION_VERSION;

  s(version, "version");

  if (s.type() == ISerializer::OUTPUT) {
    writeSequence<CachedTransactionInfo>(transactions.begin(), transactions.end(), "transactions", s);
    writeSequence<SpentKeyImage>(spentKeyImages.begin(), spentKeyImages.end(), "spent_key_images", s);
    writeSequence<CachedBlockInfo>(blockInfos.begin(), blockInfos.end(), "block_hash_indexes", s);
    writeSequence<PaymentIdTransactionHashPair>(paymentIds.begin(), paymentIds.end(), "payment_id_indexes", s);

    s(keyOutputsGlobalIndexes, "key_outputs_global_indexes");
    s(multisignatureStorage, "multisignature_storage");
  } else {
    TransactionsCacheContainer restoredTransactions;
    SpentKeyImagesContainer restoredSpentKeyImages;
    BlockInfoContainer restoredBlockHashIndex;
    OutputsGlobalIndexesContainer restoredKeyOutputsGlobalIndexes;
    MultisignaturesContainer restoredMultisignatureContainer;
    PaymentIdContainer restoredPaymentIds;

    readSequence<CachedTransactionInfo>(std::inserter(restoredTransactions, restoredTransactions.end()), "transactions", s);
    readSequence<SpentKeyImage>(std::inserter(restoredSpentKeyImages, restoredSpentKeyImages.end()), "spent_key_images", s);
    readSequence<CachedBlockInfo>(std::back_inserter(restoredBlockHashIndex), "block_hash_indexes", s);
    readSequence<PaymentIdTransactionHashPair>(std::inserter(restoredPaymentIds, restoredPaymentIds.end()), "payment_id_indexes", s);

    s(restoredKeyOutputsGlobalIndexes, "key_outputs_global_indexes");
    s(restoredMultisignatureContainer, "multisignature_storage");

    transactions = std::move(restoredTransactions);
    spentKeyImages = std::move(restoredSpentKeyImages);
    multisignatureStorage = std::move(restoredMultisignatureContainer);
    blockInfos = std::move(restoredBlockHashIndex);
    keyOutputsGlobalIndexes = std::move(restoredKeyOutputsGlobalIndexes);
    paymentIds = std::move(restoredPaymentIds);
  }
}

void BlockchainCache::save() {
  std::ofstream file(filename.c_str());
  Common::StdOutputStream stream(file);
  CryptoNote::BinaryOutputStreamSerializer s(stream);

  serialize(s);
}

void BlockchainCache::load() {
  std::ifstream file(filename.c_str());
  Common::StdInputStream stream(file);
  CryptoNote::BinaryInputStreamSerializer s(stream);

  serialize(s);
}

// output must exist
bool BlockchainCache::checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex) const {
  if (spentMultisigOutputs.count({amount, globalIndex})) {
    return true;
  }

  return parent != nullptr ? parent->checkIfSpentMultisignature(amount, globalIndex) : false;
}

bool BlockchainCache::checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) const {
  return checkIfSpentMultisignature(amount, globalIndex);
}

bool BlockchainCache::isTransactionSpendTimeUnlocked(uint64_t unlockTime) const {
  return isTransactionSpendTimeUnlocked(unlockTime, getTopBlockIndex());
}

bool BlockchainCache::isTransactionSpendTimeUnlocked(uint64_t unlockTime, uint32_t blockIndex) const {
  if (unlockTime < currency.maxBlockHeight()) {
    // interpret as block index
    return blockIndex + currency.lockedTxAllowedDeltaBlocks() >= unlockTime;
  }

  // interpret as time
  return static_cast<uint64_t>(time(nullptr)) + currency.lockedTxAllowedDeltaSeconds() >= unlockTime;
}

ExtractOutputKeysResult BlockchainCache::extractKeyOutputKeys(uint64_t amount,
                                                              Common::ArrayView<uint32_t> globalIndexes,
                                                              std::vector<Crypto::PublicKey>& publicKeys) const {
  return extractKeyOutputKeys(amount, getTopBlockIndex(), globalIndexes, publicKeys);
}

std::vector<uint32_t> BlockchainCache::getRandomOutsByAmount(Amount amount, size_t count, uint32_t blockIndex) const {
  std::vector<uint32_t> offs;
  auto it = keyOutputsGlobalIndexes.find(amount);
  if (it == keyOutputsGlobalIndexes.end()) {
    return parent != nullptr ? parent->getRandomOutsByAmount(amount, count, blockIndex) : offs;
  }

  auto& outs = it->second.outputs;
  auto end = std::find_if(outs.rbegin(), outs.rend(), [&](PackedOutIndex index) {
               return index.blockIndex <= blockIndex - currency.minedMoneyUnlockWindow();
             }).base();
  uint32_t dist = static_cast<uint32_t>(std::distance(outs.begin(), end));
  dist = std::min(static_cast<uint32_t>(count), dist);
  ShuffleGenerator<uint32_t, Crypto::random_engine<uint32_t>> generator(dist);
  while (dist--) {
    auto offset = generator();
    auto& outIndex = it->second.outputs[offset];
    auto transactionIterator = transactions.get<TransactionInBlockTag>().find(
        boost::make_tuple<uint32_t, uint32_t>(outIndex.blockIndex, outIndex.transactionIndex));
    if (isTransactionSpendTimeUnlocked(transactionIterator->unlockTime, blockIndex)) {
      offs.push_back(it->second.startIndex + offset);
    }
  }

  if (offs.size() < count && parent != nullptr) {
    auto prevs = parent->getRandomOutsByAmount(amount, count - offs.size(), blockIndex);
    offs.reserve(prevs.size() + offs.size());
    std::copy(prevs.begin(), prevs.end(), std::back_inserter(offs));
  }

  return offs;
}

ExtractOutputKeysResult BlockchainCache::extractKeyOutputKeys(uint64_t amount, uint32_t blockIndex,
                                                              Common::ArrayView<uint32_t> globalIndexes,
                                                              std::vector<Crypto::PublicKey>& publicKeys) const {
  assert(!globalIndexes.isEmpty());
  assert(std::is_sorted(globalIndexes.begin(), globalIndexes.end()));                            // sorted
  assert(std::adjacent_find(globalIndexes.begin(), globalIndexes.end()) == globalIndexes.end()); // unique

  return extractKeyOutputs(amount, blockIndex, globalIndexes, [&] (const CachedTransactionInfo& info, PackedOutIndex index, uint32_t globalIndex) {
    if (!isTransactionSpendTimeUnlocked(info.unlockTime, blockIndex)) {
      return ExtractOutputKeysResult::OUTPUT_LOCKED;
    }

    assert(info.outputs[index.outputIndex].type() == typeid(KeyOutput));
    publicKeys.push_back(boost::get<KeyOutput>(info.outputs[index.outputIndex]).key);
    return ExtractOutputKeysResult::SUCCESS;
  });
}

ExtractOutputKeysResult
BlockchainCache::extractKeyOtputReferences(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes,
                                           std::vector<std::pair<Crypto::Hash, size_t>>& outputReferences) const {
  assert(!globalIndexes.isEmpty());
  assert(std::is_sorted(globalIndexes.begin(), globalIndexes.end()));                            // sorted
  assert(std::adjacent_find(globalIndexes.begin(), globalIndexes.end()) == globalIndexes.end()); // unique

  return extractKeyOutputs(amount, getTopBlockIndex(), globalIndexes, [&] (const CachedTransactionInfo& info, PackedOutIndex index, uint32_t globalIndex) {
    outputReferences.push_back(std::make_pair(info.transactionHash, index.outputIndex));
    return ExtractOutputKeysResult::SUCCESS;
  });
}

// TODO: start from index
ExtractOutputKeysResult BlockchainCache::extractKeyOutputs(
    uint64_t amount, uint32_t blockIndex, Common::ArrayView<uint32_t> globalIndexes,
    std::function<ExtractOutputKeysResult(const CachedTransactionInfo& info, PackedOutIndex index, uint32_t globalIndex)> pred) const {
  assert(!globalIndexes.isEmpty());
  assert(std::is_sorted(globalIndexes.begin(), globalIndexes.end()));                            // sorted
  assert(std::adjacent_find(globalIndexes.begin(), globalIndexes.end()) == globalIndexes.end()); // unique

  auto globalIndexesIterator = keyOutputsGlobalIndexes.find(amount);
  if (globalIndexesIterator == keyOutputsGlobalIndexes.end() || blockIndex < startIndex) {
    return parent != nullptr ? parent->extractKeyOutputs(amount, blockIndex, globalIndexes, std::move(pred))
                             : ExtractOutputKeysResult::INVALID_GLOBAL_INDEX;
  }

  auto startGlobalIndex = globalIndexesIterator->second.startIndex;
  auto parentIndexesIterator = std::lower_bound(globalIndexes.begin(), globalIndexes.end(), startGlobalIndex);

  auto offset = std::distance(globalIndexes.begin(), parentIndexesIterator);
  if (parentIndexesIterator != globalIndexes.begin()) {
    assert(parent != nullptr);
    auto result = parent->extractKeyOutputs(amount, blockIndex, globalIndexes.head(parentIndexesIterator - globalIndexes.begin()), pred);
    if (result != ExtractOutputKeysResult::SUCCESS) {
      return result;
    }
  }

  auto myGlobalIndexes = globalIndexes.unhead(offset);
  auto& outputs = globalIndexesIterator->second.outputs;
  assert(!outputs.empty());
  for (auto globalIndex : myGlobalIndexes) {
    if (globalIndex - startGlobalIndex >= outputs.size()) {
      logger(Logging::DEBUGGING) << "Couldn't extract key output for amount " << amount << " with global index " << globalIndex
                                 << " because global index is greater than the last available: " << (startGlobalIndex + outputs.size());
      return ExtractOutputKeysResult::INVALID_GLOBAL_INDEX;
    }
    
    auto outputIndex = outputs[globalIndex - startGlobalIndex];
    
    assert(outputIndex.blockIndex >= startIndex);
    assert(outputIndex.blockIndex <= blockIndex);

    auto txIt = transactions.get<TransactionInBlockTag>().find(
        boost::make_tuple<uint32_t, uint32_t>(outputIndex.blockIndex, outputIndex.transactionIndex));
    if (txIt == transactions.get<TransactionInBlockTag>().end()) {
      logger(Logging::DEBUGGING) << "Couldn't extract key output for amount " << amount << " with global index " << globalIndex
                                 << " because containing transaction doesn't exist in index "
                                 << "(block index: " << outputIndex.blockIndex << ", transaction index: " << outputIndex.transactionIndex << ")";
      return ExtractOutputKeysResult::INVALID_GLOBAL_INDEX;
    }

    auto ret = pred(*txIt, outputIndex, globalIndex);
    if (ret != ExtractOutputKeysResult::SUCCESS) {
      logger(Logging::DEBUGGING) << "Couldn't extract key output for amount " << amount << " with global index " << globalIndex
                                 << " because callback returned fail status (block index: " << outputIndex.blockIndex
                                 << ", transaction index: " << outputIndex.transactionIndex << ")";
      return ret;
    }
  }

  return ExtractOutputKeysResult::SUCCESS;
}

std::vector<Crypto::Hash> BlockchainCache::getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) const {
  std::vector<Crypto::Hash> transactionHashes;

  if (parent != nullptr) {
    transactionHashes = parent->getTransactionHashesByPaymentId(paymentId);
  }

  auto& index = paymentIds.get<PaymentIdTag>();
  auto range = index.equal_range(paymentId);

  transactionHashes.reserve(transactionHashes.size() + std::distance(range.first, range.second));
  for (auto it = range.first; it != range.second; ++it) {
    transactionHashes.emplace_back(it->transactionHash);
  }

  logger(Logging::DEBUGGING) << "Found " << transactionHashes.size() << " transactions with payment id " << paymentId;
  return transactionHashes;
}

std::vector<Crypto::Hash> BlockchainCache::getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const {
  std::vector<Crypto::Hash> blockHashes;
  if (secondsCount == 0) {
    return blockHashes;
  }

  if (parent != nullptr) {
    blockHashes = parent->getBlockHashesByTimestamps(timestampBegin, secondsCount);
  }

  auto& index = blockInfos.get<TimestampTag>();
  auto begin = index.lower_bound(timestampBegin);
  auto end = index.upper_bound(timestampBegin + static_cast<uint64_t>(secondsCount) - 1);

  blockHashes.reserve(blockHashes.size() + std::distance(begin, end));
  for (auto it = begin; it != end; ++it) {
    blockHashes.push_back(it->blockHash);
  }

  logger(Logging::DEBUGGING) << "Found " << blockHashes.size() << " within timestamp interval "
                             << "[" << timestampBegin << ":" << (timestampBegin + secondsCount) << "]";
  return blockHashes;
}

ExtractOutputKeysResult BlockchainCache::extractKeyOtputIndexes(uint64_t amount,
                                                                Common::ArrayView<uint32_t> globalIndexes,
                                                                std::vector<PackedOutIndex>& outIndexes) const {
  assert(!globalIndexes.isEmpty());
  return extractKeyOutputs(amount, getTopBlockIndex(), globalIndexes,
                           [&](const CachedTransactionInfo& info, PackedOutIndex index, uint32_t globalIndex) {
                             outIndexes.push_back(index);
                             return ExtractOutputKeysResult::SUCCESS;
                           });
}

bool BlockchainCache::getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex,
                                                      MultisignatureOutput& output, uint64_t& unlockTime) const {
  return getMultisignatureOutputIfExists(amount, globalIndex, getTopBlockIndex(), output, unlockTime);
}

bool BlockchainCache::getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex,
                                                      MultisignatureOutput& output, uint64_t& unlockTime) const {
  PackedOutIndex packedOut;
  auto searchResult = findPackedOutForMultisignatureInCurrentSegment(amount, globalIndex, packedOut);

  if (searchResult == OutputSearchResult::INVALID_ARGUMENT) {
    return false;
  } else if (searchResult == OutputSearchResult::NOT_FOUND) {
    if (parent != nullptr) {
      return parent->getMultisignatureOutputIfExists(amount, globalIndex, blockIndex, output, unlockTime);
    }

    return false;
  }

  if (packedOut.blockIndex > blockIndex) {
    return false;
  }

  assert(packedOut.blockIndex >= startIndex);
  assert(packedOut.blockIndex <= getTopBlockIndex());

  auto transactionIt = transactions.get<TransactionInBlockTag>().find(
      boost::make_tuple<uint32_t, uint32_t>(packedOut.blockIndex, packedOut.transactionIndex));
  assert(transactionIt != transactions.get<TransactionInBlockTag>().end());

  unlockTime = transactionIt->unlockTime;
  assert(transactionIt->outputs[packedOut.outputIndex].type() == typeid(MultisignatureOutput));
  output = boost::get<MultisignatureOutput>(transactionIt->outputs[packedOut.outputIndex]);

  return true;
}

std::pair<Crypto::Hash, size_t> BlockchainCache::getMultisignatureOutputReference(uint64_t amount,
                                                                                  uint32_t globalIndex) const {
  PackedOutIndex packedOut;
  auto searchResult = findPackedOutForMultisignatureInCurrentSegment(amount, globalIndex, packedOut);

  assert(searchResult != OutputSearchResult::INVALID_ARGUMENT);
  if (searchResult == OutputSearchResult::NOT_FOUND) {
    assert(parent != nullptr);
    return parent->getMultisignatureOutputReference(amount, globalIndex);
  }

  assert(packedOut.blockIndex >= startIndex);
  assert(packedOut.blockIndex <= getTopBlockIndex());
  auto transactionIt = transactions.get<TransactionInBlockTag>().find(
      boost::make_tuple<uint32_t, uint32_t>(packedOut.blockIndex, packedOut.transactionIndex));
  assert(transactionIt != transactions.get<TransactionInBlockTag>().end());
  return std::make_pair(transactionIt->transactionHash, packedOut.outputIndex);
}

uint32_t BlockchainCache::getTopBlockIndex() const {
  assert(!blockInfos.empty());
  return startIndex + static_cast<uint32_t>(blockInfos.size()) - 1;
}

const Crypto::Hash& BlockchainCache::getTopBlockHash() const {
  assert(!blockInfos.empty());
  return blockInfos.get<BlockIndexTag>().back().blockHash;
}

std::vector<uint64_t> BlockchainCache::getLastTimestamps(size_t count) const {
  return getLastTimestamps(count, getTopBlockIndex(), skipGenesisBlock);
}

std::vector<uint64_t> BlockchainCache::getLastTimestamps(size_t count, uint32_t blockIndex,
                                                         UseGenesis useGenesis) const {
  return getLastUnits(count, blockIndex, useGenesis, [](const CachedBlockInfo& inf) { return inf.timestamp; });
}

std::vector<uint64_t> BlockchainCache::getLastBlocksSizes(size_t count) const {
  return getLastBlocksSizes(count, getTopBlockIndex(), skipGenesisBlock);
}

std::vector<uint64_t> BlockchainCache::getLastUnits(size_t count, uint32_t blockIndex, UseGenesis useGenesis,
                                                    std::function<uint64_t(const CachedBlockInfo&)> pred) const {
  assert(blockIndex <= getTopBlockIndex());

  size_t to = blockIndex < startIndex ? 0 : blockIndex - startIndex + 1;
  auto realCount = std::min(count, to);
  auto from = to - realCount;
  if (!useGenesis && from == 0 && realCount != 0 && parent == nullptr) {
    from += 1;
    realCount -= 1;
  }

  auto& blocksIndex = blockInfos.get<BlockIndexTag>();

  std::vector<uint64_t> result;
  if (realCount < count && parent != nullptr) {
    result =
        parent->getLastUnits(count - realCount, std::min(blockIndex, parent->getTopBlockIndex()), useGenesis, pred);
  }

  std::transform(std::next(blocksIndex.begin(), from), std::next(blocksIndex.begin(), to), std::back_inserter(result),
                 std::move(pred));
  return result;
}

std::vector<uint64_t> BlockchainCache::getLastBlocksSizes(size_t count, uint32_t blockIndex,
                                                          UseGenesis useGenesis) const {
  return getLastUnits(count, blockIndex, useGenesis, [](const CachedBlockInfo& cb) { return cb.blockSize; });
}

Difficulty BlockchainCache::getDifficultyForNextBlock() const {
  return getDifficultyForNextBlock(getTopBlockIndex());
}

Difficulty BlockchainCache::getDifficultyForNextBlock(uint32_t blockIndex) const {
  assert(blockIndex <= getTopBlockIndex());
  auto timestamps = getLastTimestamps(currency.difficultyBlocksCount(), blockIndex, skipGenesisBlock);
  auto commulativeDifficulties =
      getLastCumulativeDifficulties(currency.difficultyBlocksCount(), blockIndex, skipGenesisBlock);
  return currency.nextDifficulty(std::move(timestamps), std::move(commulativeDifficulties));
}

Difficulty BlockchainCache::getCurrentCumulativeDifficulty() const {
  assert(!blockInfos.empty());
  return blockInfos.get<BlockIndexTag>().back().cumulativeDifficulty;
}

Difficulty BlockchainCache::getCurrentCumulativeDifficulty(uint32_t blockIndex) const {
  assert(!blockInfos.empty());
  assert(blockIndex <= getTopBlockIndex());
  return blockInfos.get<BlockIndexTag>().at(blockIndex - startIndex).cumulativeDifficulty;
}

uint64_t BlockchainCache::getAlreadyGeneratedCoins() const {
  return getAlreadyGeneratedCoins(getTopBlockIndex());
}

uint64_t BlockchainCache::getAlreadyGeneratedCoins(uint32_t blockIndex) const {
  if (blockIndex < startIndex) {
    assert(parent != nullptr);
    return parent->getAlreadyGeneratedCoins(blockIndex);
  }

  return blockInfos.get<BlockIndexTag>().at(blockIndex - startIndex).alreadyGeneratedCoins;
}

uint64_t BlockchainCache::getAlreadyGeneratedTransactions(uint32_t blockIndex) const {
  if (blockIndex < startIndex) {
    assert(parent != nullptr);
    return parent->getAlreadyGeneratedTransactions(blockIndex);
  }

  return blockInfos.get<BlockIndexTag>().at(blockIndex - startIndex).alreadyGeneratedTransactions;
}

std::vector<Difficulty> BlockchainCache::getLastCumulativeDifficulties(size_t count, uint32_t blockIndex,
                                                                       UseGenesis useGenesis) const {
  return getLastUnits(count, blockIndex, useGenesis,
                      [](const CachedBlockInfo& info) { return info.cumulativeDifficulty; });
}

std::vector<Difficulty> BlockchainCache::getLastCumulativeDifficulties(size_t count) const {
  return getLastCumulativeDifficulties(count, getTopBlockIndex(), skipGenesisBlock);
}

TransactionValidatorState BlockchainCache::fillOutputsSpentByBlock(uint32_t blockIndex) const {
  TransactionValidatorState spentOutputs;
  auto& keyImagesIndex = spentKeyImages.get<BlockIndexTag>();

  auto range = keyImagesIndex.equal_range(blockIndex);
  for (auto it = range.first; it != range.second; ++it) {
    spentOutputs.spentKeyImages.insert(it->keyImage);
  }

  auto it = spentMultisigOutputsByBlock.find(blockIndex);
  if (it != spentMultisigOutputsByBlock.end()) {
    std::copy(it->second.begin(), it->second.end(), std::inserter(spentOutputs.spentMultisignatureGlobalIndexes,
                                                                  spentOutputs.spentMultisignatureGlobalIndexes.end()));
  }

  return spentOutputs;
}

bool BlockchainCache::hasTransaction(const Crypto::Hash& transactionHash) const {
  auto& index = transactions.get<TransactionHashTag>();
  auto it = index.find(transactionHash);
  return it != index.end();
}

uint32_t BlockchainCache::getBlockIndexContainingTx(const Crypto::Hash& transactionHash) const {
  auto& index = transactions.get<TransactionHashTag>();
  auto it = index.find(transactionHash);
  assert(it != index.end());
  return it->blockIndex;
}

void BlockchainCache::fixChildrenParent(IBlockchainCache* p) {
  for (auto child: children) {
    child->setParent(p);
  }
}

}
