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

#include <CryptoNoteCore/DatabaseBlockchainCache.h>

#include <ctime>
#include <cstdlib>

#include <boost/iterator/iterator_facade.hpp>

#include <Common/ShuffleGenerator.h>

#include "BlockchainUtils.h"

#include "crypto/hash.h"

#include <CryptoNoteCore/BlockchainStorage.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include "CryptoNoteCore/TransactionExtra.h"

namespace CryptoNote {

namespace {

const uint32_t ONE_DAY_SECONDS = 60 * 60 * 24;
const CachedBlockInfo NULL_CACHED_BLOCK_INFO {NULL_HASH, 0, 0, 0, 0, 0};

bool requestPackedOutputs(IBlockchainCache::Amount amount, Common::ArrayView<uint32_t> globalIndexes, IDataBase& database, std::vector<PackedOutIndex>& result) {
  BlockchainReadBatch readBatch;
  result.reserve(result.size() + globalIndexes.getSize());

  for (auto globalIndex: globalIndexes) {
    readBatch.requestKeyOutputGlobalIndexForAmount(amount, globalIndex);
  }

  auto dbResult = database.read(readBatch);
  if (dbResult) {
    return false;
  }

  try {
    auto readResult = readBatch.extractResult();
    const auto& packedOutsMap = readResult.getKeyOutputGlobalIndexesForAmounts();
    for (auto globalIndex: globalIndexes) {
      result.push_back(packedOutsMap.at(std::make_pair(amount, globalIndex)));
    }
  } catch (std::exception&) {
    return false;
  }

  return true;
}

bool requestTransactionHashesForGlobalOutputIndexes(const std::vector<PackedOutIndex>& packedOuts, IDataBase& database, std::vector<Crypto::Hash>& transactionHashes) {
  BlockchainReadBatch readHashesBatch;

  std::set<uint32_t> blockIndexes;
  std::for_each(packedOuts.begin(), packedOuts.end(), [&blockIndexes] (PackedOutIndex out) { blockIndexes.insert(out.blockIndex); });
  std::for_each(blockIndexes.begin(), blockIndexes.end(), [&readHashesBatch] (uint32_t blockIndex) { readHashesBatch.requestTransactionHashesByBlock(blockIndex); });

  auto dbResult = database.read(readHashesBatch);
  if (dbResult) {
    return false;
  }

  auto readResult = readHashesBatch.extractResult();
  const auto& transactionHashesMap = readResult.getTransactionHashesByBlocks();

  if (transactionHashesMap.size() != blockIndexes.size()) {
    return false;
  }

  transactionHashes.reserve(transactionHashes.size() + packedOuts.size());
  for (const auto& output: packedOuts) {
    if (output.transactionIndex >= transactionHashesMap.at(output.blockIndex).size()) {
      return false;
    }

    transactionHashes.push_back(transactionHashesMap.at(output.blockIndex)[output.transactionIndex]);
  }

  return true;
}

bool requestCachedTransactionInfos(const std::vector<Crypto::Hash>& transactionHashes, IDataBase& database, std::vector<CachedTransactionInfo>& result) {
  result.reserve(result.size() + transactionHashes.size());

  BlockchainReadBatch transactionsBatch;
  std::for_each(transactionHashes.begin(), transactionHashes.end(), [&transactionsBatch] (const Crypto::Hash& hash) { transactionsBatch.requestCachedTransaction(hash); });
  auto dbResult = database.read(transactionsBatch);
  if (dbResult) {
    return false;
  }

  auto readResult = transactionsBatch.extractResult();
  const auto& transactions = readResult.getCachedTransactions();
  if (transactions.size() != transactionHashes.size()) {
    return false;
  }

  for (const auto& hash: transactionHashes) {
    result.push_back(transactions.at(hash));
  }

  return true;
}

//returns CachedTransactionInfos in the same or as packedOuts are
bool requestCachedTransactionInfos(const std::vector<PackedOutIndex>& packedOuts, IDataBase& database, std::vector<CachedTransactionInfo>& result) {
  std::vector<Crypto::Hash> transactionHashes;
  if (!requestTransactionHashesForGlobalOutputIndexes(packedOuts, database, transactionHashes)) {
    return false;
  }

  return requestCachedTransactionInfos(transactionHashes, database, result);
}

bool requestExtendedTransactionInfos(const std::vector<Crypto::Hash>& transactionHashes, IDataBase& database, std::vector<ExtendedTransactionInfo>& result) {
  result.reserve(result.size() + transactionHashes.size());

  BlockchainReadBatch transactionsBatch;
  std::for_each(transactionHashes.begin(), transactionHashes.end(), [&transactionsBatch] (const Crypto::Hash& hash) { transactionsBatch.requestCachedTransaction(hash); });
  auto dbResult = database.read(transactionsBatch);
  if (dbResult) {
    return false;
  }

  auto readResult = transactionsBatch.extractResult();
  const auto& transactions = readResult.getCachedTransactions();

  std::unordered_set<Crypto::Hash> uniqueTransactionHashes(transactionHashes.begin(), transactionHashes.end());
  if (transactions.size() != uniqueTransactionHashes.size()) {
    return false;
  }

  for (const auto& hash: transactionHashes) {
    result.push_back(transactions.at(hash));
  }

  return true;
}

//returns ExtendedTransactionInfos in the same order as packedOuts are
bool requestExtendedTransactionInfos(const std::vector<PackedOutIndex>& packedOuts, IDataBase& database, std::vector<ExtendedTransactionInfo>& result) {
  std::vector<Crypto::Hash> transactionHashes;
  if (!requestTransactionHashesForGlobalOutputIndexes(packedOuts, database, transactionHashes)) {
    return false;
  }

  return requestExtendedTransactionInfos(transactionHashes, database, result);
}

uint64_t roundToMidnight(uint64_t timestamp) {
  if (timestamp > static_cast<uint64_t>(std::numeric_limits<time_t>::max())) {
    throw std::runtime_error("Timestamp is too big");
  }

  return static_cast<uint64_t>((timestamp / ONE_DAY_SECONDS) * ONE_DAY_SECONDS);
}

std::pair<boost::optional<uint32_t>, bool> requestClosestBlockIndexByTimestamp(uint64_t timestamp, IDataBase& database) {
  std::pair<boost::optional<uint32_t>, bool> result = {{}, false};

  BlockchainReadBatch readBatch;
  readBatch.requestClosestTimestampBlockIndex(timestamp);
  auto dbResult = database.read(readBatch);
  if (dbResult) {
    return result;
  }

  result.second = true;
  auto readResult = readBatch.extractResult();
  if (readResult.getClosestTimestampBlockIndex().count(timestamp)) {
    result.first = readResult.getClosestTimestampBlockIndex().at(timestamp);
  }

  return result;
}

bool requestRawBlock(IDataBase& database, uint32_t blockIndex, RawBlock& block) {
  auto batch = BlockchainReadBatch().requestRawBlock(blockIndex);

  auto error = database.read(batch);
  if (error) {
    //may be throw in all similiar functions???
    return false;
  }

  auto result = batch.extractResult();
  if (result.getRawBlocks().count(blockIndex) == 0) {
    return false;
  }

  block = result.getRawBlocks().at(blockIndex);
  return true;
}

Transaction extractTransaction(const RawBlock& block, uint32_t transactionIndex) {
  assert(transactionIndex < block.transactions.size() + 1);

  if (transactionIndex != 0) {
    Transaction transaction;
    bool r = fromBinaryArray(transaction, block.transactions[transactionIndex - 1]);
    assert(r);

    return transaction;
  }

  BlockTemplate blockTemplate;
  bool r = fromBinaryArray(blockTemplate, block.block);
  assert(r);

  return blockTemplate.baseTransaction;
}

size_t requestPaymentIdTransactionsCount(IDataBase& database, const Crypto::Hash& paymentId) {
  auto batch = BlockchainReadBatch().requestTransactionCountByPaymentId(paymentId);
  auto error = database.read(batch);
  if (error) {
    throw std::system_error(error, "Error while reading transactions count by payment id");
  }

  auto result = batch.extractResult();
  if (result.getTransactionCountByPaymentIds().count(paymentId) == 0) {
    return 0;
  }

  return result.getTransactionCountByPaymentIds().at(paymentId);
}

bool requestPaymentId(IDataBase& database, const Crypto::Hash& transactionHash, Crypto::Hash& paymentId) {
  std::vector<CachedTransactionInfo> cachedTransactions;

  if (!requestCachedTransactionInfos({transactionHash}, database, cachedTransactions)) {
    return false;
  }

  if (cachedTransactions.empty()) {
    return false;
  }

  RawBlock block;
  if (!requestRawBlock(database, cachedTransactions[0].blockIndex, block)) {
    return false;
  }

  Transaction transaction = extractTransaction(block, cachedTransactions[0].transactionIndex);
  return getPaymentIdFromTxExtra(transaction.extra, paymentId);
}

uint32_t requestKeyOutputGlobalIndexesCountForAmount(IBlockchainCache::Amount amount, IDataBase& database) {
  auto batch = BlockchainReadBatch().requestKeyOutputGlobalIndexesCountForAmount(amount);
  auto dbError = database.read(batch);
  if (dbError) {
    throw std::system_error(dbError, "Cannot perform requestKeyOutputGlobalIndexesCountForAmount query");
  }

  auto result = batch.extractResult();

  if (result.getKeyOutputGlobalIndexesCountForAmounts().count(amount) != 0) {
    return result.getKeyOutputGlobalIndexesCountForAmounts().at(amount);
  } else {
    return 0;
  }
}

uint32_t requestMultisignatureOutputGlobalIndexesCountForAmount(IBlockchainCache::Amount amount, IDataBase& database) {
  auto batch = BlockchainReadBatch().requestMultisignatureOutputGlobalIndexesCountForAmount(amount);
  auto dbError = database.read(batch);
  if (dbError) {
    throw std::system_error(dbError, "Cannot perform requestMultisignatureOutputGlobalIndexesCountForAmount query");
  }

  auto result = batch.extractResult();

  if (result.getMultisignatureOutputGlobalIndexesCountForAmounts().count(amount) != 0) {
    return result.getMultisignatureOutputGlobalIndexesCountForAmounts().at(amount);
  } else {
    return 0;
  }
}

class DbOutputConstIterator: public boost::iterator_facade<DbOutputConstIterator, const PackedOutIndex, boost::random_access_traversal_tag /*boost::forward_traversal_tag*/> {
public:
  DbOutputConstIterator(std::function<PackedOutIndex (IBlockchainCache::Amount amount, uint32_t globalOutputIndex)> retriever_,
    IBlockchainCache::Amount amount_,
    uint32_t globalOutputIndex_) : retriever(retriever_), amount(amount_), globalOutputIndex(globalOutputIndex_)
  {}

  const PackedOutIndex& dereference() const {
    cachedValue = retriever(amount, globalOutputIndex);
    return cachedValue;
  }

  bool equal(const DbOutputConstIterator& other) const {
    return globalOutputIndex == other.globalOutputIndex;
  }

  void increment() {
    ++globalOutputIndex;
  }

  void advance(difference_type n) {
    assert(n >= -static_cast<difference_type>(globalOutputIndex));
    globalOutputIndex += static_cast<uint32_t>(n);
  }

  difference_type distance_to(const DbOutputConstIterator& to) const {
    return static_cast<difference_type>(to.globalOutputIndex) - static_cast<difference_type>(globalOutputIndex);
  }

private:
  std::function<PackedOutIndex (IBlockchainCache::Amount amount, uint32_t globalOutputIndex)> retriever;
  IBlockchainCache::Amount amount;
  uint32_t globalOutputIndex;
  mutable PackedOutIndex cachedValue;
};

PackedOutIndex retrieveKeyOutput(IBlockchainCache::Amount amount, uint32_t globalOutputIndex, IDataBase& database) {
  BlockchainReadBatch batch;
  auto dbError = database.read(batch.requestKeyOutputGlobalIndexForAmount(amount, globalOutputIndex));
  if (dbError) {
    throw std::system_error(dbError, "Error during retrieving key output by global output index");
  }

  auto result = batch.extractResult();

  try {
    return result.getKeyOutputGlobalIndexesForAmounts().at(std::make_pair(amount, globalOutputIndex));
  } catch (std::exception&) {
    assert(false);
    throw std::runtime_error("Couldn't find key output for amount " + std::to_string(amount) + " with global output index " + std::to_string(globalOutputIndex));
  }
}

PackedOutIndex retrieveMultisignatureOutput(IBlockchainCache::Amount amount, uint32_t globalOutputIndex, IDataBase& database) {
  BlockchainReadBatch batch;
  auto dbError = database.read(batch.requestMultisignatureOutputGlobalIndexForAmount(amount, globalOutputIndex));
  if (dbError) {
    throw std::system_error(dbError, "Error during retrieving multisignature output by global output index");
  }

  auto result = batch.extractResult();
  try {
    return result.getMultisignatureOutputGlobalIndexesForAmounts().at(std::make_pair(amount, globalOutputIndex));
  } catch (std::exception&) {
    assert(false);
    throw std::runtime_error("Couldn't find multisignature output for amount " + std::to_string(amount) + " with global output index " + std::to_string(globalOutputIndex));
  }
}

std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex> getMinGlobalIndexesByAmount(
    const std::map<IBlockchainCache::Amount, std::vector<IBlockchainCache::GlobalOutputIndex>>& outputIndexes) {

  std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex> minIndexes;
  for (const auto& kv: outputIndexes) {
    auto min = std::min_element(kv.second.begin(), kv.second.end());
    if (min == kv.second.end()) {
      continue;
    }

    minIndexes.emplace(kv.first, *min);
  }

  return minIndexes;
}

void mergeOutputsSplitBoundaries(std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>& dest,
                                 const std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>& src) {
  for (const auto& elem: src) {
    auto it = dest.find(elem.first);
    if (it == dest.end()) {
      dest.emplace(elem.first, elem.second);
      continue;
    }

    if (it->second > elem.second) {
      it->second = elem.second;
    }
  }
}

void cutTail(std::deque<CachedBlockInfo>& cache, size_t count) {
  if (count >= cache.size()) {
    cache.clear();
    return;
  }

  cache.erase(std::next(cache.begin(), cache.size() - count), cache.end());
}

const std::string DB_VERSION_KEY = "db_scheme_version";

class DatabaseVersionReadBatch: public IReadBatch {
public:
  virtual ~DatabaseVersionReadBatch() {}

  virtual std::vector<std::string> getRawKeys() const override {
    return {DB_VERSION_KEY};
  }

  virtual void submitRawResult(const std::vector<std::string>& values, const std::vector<bool>& resultStates) override {
    assert(values.size() == 1);
    assert(resultStates.size() == values.size());

    if (!resultStates[0]) {
      return;
    }

    version = static_cast<uint32_t>(std::atoi(values[0].c_str()));
  }

  boost::optional<uint32_t> getDbSchemeVersion() {
    return version;
  }

private:
  boost::optional<uint32_t> version;
};

class DatabaseVersionWriteBatch: public IWriteBatch {
public:
  DatabaseVersionWriteBatch(uint32_t version): schemeVersion(version) {}
  virtual ~DatabaseVersionWriteBatch() {}

  virtual std::vector<std::pair<std::string, std::string> > extractRawDataToInsert() override {
    return {make_pair(DB_VERSION_KEY, std::to_string(schemeVersion))};
  }

  virtual std::vector<std::string> extractRawKeysToRemove() override {
    return {};
  }

private:
  uint32_t schemeVersion;
};

const uint32_t CURRENT_DB_SCHEME_VERSION = 1;

}

struct DatabaseBlockchainCache::ExtendedPushedBlockInfo {
  PushedBlockInfo pushedBlockInfo;
  uint64_t timestamp;
};


DatabaseBlockchainCache::DatabaseBlockchainCache(const Currency& curr, IDataBase& dataBase, IBlockchainCacheFactory& blockchainCacheFactory, Logging::ILogger& _logger)
    : currency(curr), database(dataBase), blockchainCacheFactory(blockchainCacheFactory), logger(_logger, "DatabaseBlockchainCache") {
  DatabaseVersionReadBatch readBatch;
  auto ec = database.read(readBatch);
  if (ec) {
    throw std::system_error(ec);
  }

  auto version = readBatch.getDbSchemeVersion();
  if (!version) {
    logger(Logging::DEBUGGING) << "DB scheme version not found, writing: " << CURRENT_DB_SCHEME_VERSION;

    DatabaseVersionWriteBatch writeBatch(CURRENT_DB_SCHEME_VERSION);
    auto writeError = database.write(writeBatch);
    if (writeError) {
      throw std::system_error(writeError);
    }
  } else {
    logger(Logging::DEBUGGING) << "Current db scheme version: " << *version;
  }

  if (getTopBlockIndex() == 0) {
    logger(Logging::DEBUGGING) << "top block index is nill, add genesis block";
    addGenesisBlock(CachedBlock (currency.genesisBlock()));
  }
}

void DatabaseBlockchainCache::deleteClosestTimestampBlockIndex(BlockchainWriteBatch& writeBatch, uint32_t splitBlockIndex) {
  auto batch = BlockchainReadBatch().requestCachedBlock(splitBlockIndex);
  auto blockResult = readDatabase(batch);
  auto timestamp = blockResult.getCachedBlocks().at(splitBlockIndex).timestamp;

  auto midnight = roundToMidnight(timestamp);
  auto timestampResult = requestClosestBlockIndexByTimestamp(midnight, database);
  if (!timestampResult.second) {
    logger(Logging::ERROR) << "deleteClosestTimestampBlockIndex error: get closest timestamp block index, database read failed";
    throw std::runtime_error("Couldn't get closest timestamp block index");
  }

  assert(bool(timestampResult.first));

  auto blockIndex = *timestampResult.first;
  assert(splitBlockIndex >= blockIndex);

  if (splitBlockIndex != blockIndex) {
    midnight += ONE_DAY_SECONDS;
  }

  BlockchainReadBatch midnightBatch;
  while (readDatabase(midnightBatch.requestClosestTimestampBlockIndex(midnight)).getClosestTimestampBlockIndex().count(midnight)) {
    writeBatch.removeClosestTimestampBlockIndex(midnight);
    midnight += ONE_DAY_SECONDS;
  }

  logger(Logging::TRACE) << "deleted closest timestamp";
}

/*
 * This methods splits cache, upper part (ie blocks with indexes greater or equal to splitBlockIndex)
 * is copied to new BlockchainCache
 */
std::unique_ptr<IBlockchainCache> DatabaseBlockchainCache::split(uint32_t splitBlockIndex) {
  assert(splitBlockIndex <= getTopBlockIndex());
  logger(Logging::DEBUGGING) << "split at index " << splitBlockIndex << " started, top block index: " << getTopBlockIndex();

  auto cache = blockchainCacheFactory.createBlockchainCache(currency, this, splitBlockIndex);

  using DeleteBlockInfo = std::tuple<uint32_t, Crypto::Hash, TransactionValidatorState, uint64_t>;
  std::vector<DeleteBlockInfo> deletingBlocks;

  BlockchainWriteBatch writeBatch;
  auto currentTop = getTopBlockIndex();
  for (uint32_t blockIndex = splitBlockIndex; blockIndex <= currentTop; ++blockIndex) {
    ExtendedPushedBlockInfo extendedInfo = getExtendedPushedBlockInfo(blockIndex);

    auto validatorState = extendedInfo.pushedBlockInfo.validatorState;
    logger(Logging::DEBUGGING) << "pushing block " << blockIndex << " to child segment";
    auto blockHash = pushBlockToAnotherCache(*cache, std::move(extendedInfo.pushedBlockInfo));

    deletingBlocks.emplace_back(blockIndex, blockHash, validatorState, extendedInfo.timestamp);
  }

  for (auto it = deletingBlocks.rbegin(); it != deletingBlocks.rend(); ++it) {
    auto blockIndex = std::get<0>(*it);
    auto blockHash = std::get<1>(*it);
    auto& validatorState = std::get<2>(*it);
    uint64_t timestamp = std::get<3>(*it);

    writeBatch.removeCachedBlock(blockHash, blockIndex).removeRawBlock(blockIndex);
    requestDeleteSpentOutputs(writeBatch,
                              blockIndex,
                              validatorState);
    requestRemoveTimestamp(writeBatch, timestamp, blockHash);
  }

  auto deletingTransactionHashes = requestTransactionHashesFromBlockIndex(splitBlockIndex);
  requestDeleteTransactions(writeBatch, deletingTransactionHashes);
  requestDeletePaymentIds(writeBatch, deletingTransactionHashes);

  std::vector<ExtendedTransactionInfo> extendedTransactions;
  if (!requestExtendedTransactionInfos(deletingTransactionHashes, database, extendedTransactions)) {
    logger(Logging::ERROR) << "Error while split: failed to request extended transaction info";
    throw std::runtime_error("failed to request extended transaction info"); //TODO: make error codes
  }

  std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex> keyIndexSplitBoundaries;
  std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex> multisigIndexSplitBoundaries;
  for (const auto& transaction: extendedTransactions) {
    auto txkeyBoundaries = getMinGlobalIndexesByAmount(transaction.amountToKeyIndexes);
    auto txMultisigBoundaries = getMinGlobalIndexesByAmount(transaction.amountToMultiIndexes);

    mergeOutputsSplitBoundaries(keyIndexSplitBoundaries, txkeyBoundaries);
    mergeOutputsSplitBoundaries(multisigIndexSplitBoundaries, txMultisigBoundaries);
  }

  requestDeleteKeyOutputs(writeBatch, keyIndexSplitBoundaries);
  requestDeleteMultisignatureOutputs(writeBatch, multisigIndexSplitBoundaries);

  deleteClosestTimestampBlockIndex(writeBatch, splitBlockIndex);

  logger(Logging::DEBUGGING) << "Performing delete operations";
  // all data and indexes are now copied, no errors detected, can now erase data from database
  auto err = database.write(writeBatch);
  if (err) {
    logger(Logging::ERROR) << "split write failed, " << err.message();
    throw std::runtime_error(err.message());
  }

  cutTail(unitsCache, currentTop + 1 - splitBlockIndex);

  children.push_back(cache.get());
  logger(Logging::TRACE) << "Delete successfull";

  // invalidate top block index and hash
  topBlockIndex = boost::none;
  topBlockHash = boost::none;
  transactionsCount = boost::none;

  logger(Logging::DEBUGGING) << "split completed";
  // return new cache
  return cache;
}

//returns hash of pushed block
Crypto::Hash DatabaseBlockchainCache::pushBlockToAnotherCache(IBlockchainCache& segment, PushedBlockInfo&& pushedBlockInfo) {
  BlockTemplate block;
  bool br = fromBinaryArray(block, pushedBlockInfo.rawBlock.block);
  assert(br);

  std::vector<CachedTransaction> transactions;
  bool tr = Utils::restoreCachedTransactions(pushedBlockInfo.rawBlock.transactions, transactions);
  assert(tr);

  CachedBlock cachedBlock(block);
  segment.pushBlock(cachedBlock,
                    transactions,
                    pushedBlockInfo.validatorState,
                    pushedBlockInfo.blockSize,
                    pushedBlockInfo.generatedCoins,
                    pushedBlockInfo.blockDifficulty,
                    std::move(pushedBlockInfo.rawBlock));

  return cachedBlock.getBlockHash();
}

std::vector<Crypto::Hash> DatabaseBlockchainCache::requestTransactionHashesFromBlockIndex(uint32_t splitBlockIndex) {
  logger(Logging::DEBUGGING) << "Requesting transaction hashes starting from block index " << splitBlockIndex;

  BlockchainReadBatch readBatch;
  for (uint32_t blockIndex = splitBlockIndex; blockIndex <= getTopBlockIndex(); ++blockIndex) {
    readBatch.requestTransactionHashesByBlock(blockIndex);
  }

  std::vector<Crypto::Hash> transactionHashes;

  auto dbResult = readDatabase(readBatch);
  for (const auto& kv: dbResult.getTransactionHashesByBlocks()) {
    for (const auto& hash: kv.second) {
      transactionHashes.emplace_back(hash);
    }
  }

  return transactionHashes;
}

void DatabaseBlockchainCache::requestDeleteTransactions(BlockchainWriteBatch& writeBatch, const std::vector<Crypto::Hash>& transactionHashes) {
  for (const auto& hash: transactionHashes) {
    assert(getCachedTransactionsCount() > 0);
    writeBatch.removeCachedTransaction(hash, getCachedTransactionsCount() - 1);
    transactionsCount = *transactionsCount - 1;
  }
}

void DatabaseBlockchainCache::requestDeletePaymentIds(BlockchainWriteBatch& writeBatch, const std::vector<Crypto::Hash>& transactionHashes) {
  std::unordered_map<Crypto::Hash, size_t> paymentCounts;

  for (const auto& hash: transactionHashes) {
    Crypto::Hash paymentId;
    if (!requestPaymentId(database, hash, paymentId)) {
      continue;
    }

    paymentCounts[paymentId] += 1;
  }

  for (const auto& kv: paymentCounts) {
    requestDeletePaymentId(writeBatch, kv.first, kv.second);
  }
}

void DatabaseBlockchainCache::requestDeletePaymentId(BlockchainWriteBatch& writeBatch, const Crypto::Hash& paymentId, size_t toDelete) {
  size_t count = requestPaymentIdTransactionsCount(database, paymentId);
  assert(count > 0);
  assert(count >= toDelete);

  logger(Logging::DEBUGGING) << "Deleting last " << toDelete << " transaction hashes of payment id " << paymentId;
  writeBatch.removePaymentId(paymentId, static_cast<uint32_t>(count - toDelete));
}

void DatabaseBlockchainCache::requestDeleteSpentOutputs(BlockchainWriteBatch& writeBatch, uint32_t blockIndex, const TransactionValidatorState& spentOutputs) {
  logger(Logging::DEBUGGING) << "Deleting spent outputs for block index " << blockIndex;

  std::vector<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>> spentMultisigs(
        spentOutputs.spentMultisignatureGlobalIndexes.begin(),
        spentOutputs.spentMultisignatureGlobalIndexes.end());

  std::vector<Crypto::KeyImage> spentKeys(spentOutputs.spentKeyImages.begin(), spentOutputs.spentKeyImages.end());

  writeBatch.removeSpentKeyImages(blockIndex, spentKeys)
      .removeSpentMultisignatureOutputGlobalIndexes(blockIndex, spentMultisigs);
}

void DatabaseBlockchainCache::requestDeleteKeyOutputs(BlockchainWriteBatch& writeBatch,
                                                      const std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>& boundaries) {
  if (boundaries.empty()) {
    //hardly possible
    logger(Logging::DEBUGGING) << "No key output amounts...";
    return;
  }

  BlockchainReadBatch readBatch;
  for (auto kv: boundaries) {
    readBatch.requestKeyOutputGlobalIndexesCountForAmount(kv.first);
  }

  std::unordered_map<IBlockchainCache::Amount, uint32_t> amountCounts = readDatabase(readBatch).getKeyOutputGlobalIndexesCountForAmounts();
  assert(amountCounts.size() == boundaries.size());

  for (const auto& kv: amountCounts) {
    auto it = boundaries.find(kv.first); //can't be equal end() since assert(amountCounts.size() == boundaries.size())
    requestDeleteKeyOutputsAmount(writeBatch, kv.first, it->second, kv.second);
  }
}

void DatabaseBlockchainCache::requestDeleteKeyOutputsAmount(BlockchainWriteBatch& writeBatch, IBlockchainCache::Amount amount,
                                                            IBlockchainCache::GlobalOutputIndex boundary, uint32_t outputsCount) {
  logger(Logging::DEBUGGING) << "Requesting delete for key output amount " << amount <<
                                " starting from global index " << boundary << " to " << (outputsCount - 1);

  writeBatch.removeKeyOutputGlobalIndexes(amount, outputsCount - boundary, boundary);
  for (GlobalOutputIndex index = boundary; index < outputsCount; ++index) {
    writeBatch.removeKeyOutputInfo(amount, index);
  }

  updateKeyOutputCount(amount, boundary - outputsCount);
}

void DatabaseBlockchainCache::requestDeleteMultisignatureOutputs(BlockchainWriteBatch& writeBatch,
                                                                 const std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>& boundaries) {
  if (boundaries.empty()) {
    logger(Logging::DEBUGGING) << "No multisignature output amounts...";
    return;
  }

  BlockchainReadBatch readBatch;
  for (auto kv: boundaries) {
    readBatch.requestMultisignatureOutputGlobalIndexesCountForAmount(kv.first);
  }

  std::unordered_map<IBlockchainCache::Amount, uint32_t> amountCounts = readDatabase(readBatch).getMultisignatureOutputGlobalIndexesCountForAmounts();

  for (const auto& kv: amountCounts) {
    auto it = boundaries.find(kv.first);
    requestDeleteMultisignatureOutputsAmount(writeBatch, kv.first, it->second, kv.second);
  }
}

void DatabaseBlockchainCache::requestDeleteMultisignatureOutputsAmount(BlockchainWriteBatch& writeBatch, IBlockchainCache::Amount amount,
                                                                       IBlockchainCache::GlobalOutputIndex boundary, uint32_t outputsCount) {
  logger(Logging::DEBUGGING) << "Requesting delete for multisignature output amount " << amount <<
                                " starting from global index " << boundary << " to " << (outputsCount - 1);
  writeBatch.removeKeyOutputGlobalIndexes(amount, outputsCount - boundary, boundary);
  updateMultiOutputCount(amount, boundary - outputsCount);
}

void DatabaseBlockchainCache::requestRemoveTimestamp(BlockchainWriteBatch& batch, uint64_t timestamp, const Crypto::Hash& blockHash) {
  auto readBatch = BlockchainReadBatch().requestBlockHashesByTimestamp(timestamp);
  auto result = readDatabase(readBatch);

  if (result.getBlockHashesByTimestamp().count(timestamp) == 0) {
    return;
  }

  auto indexes = result.getBlockHashesByTimestamp().at(timestamp);
  auto it = std::find(indexes.begin(), indexes.end(), blockHash);
  indexes.erase(it);

  if (indexes.empty()) {
    logger(Logging::DEBUGGING) << "Deleting timestamp " << timestamp;
    batch.removeTimestamp(timestamp);
  } else {
    logger(Logging::DEBUGGING) << "Deleting block hash " << blockHash << " from timestamp " << timestamp;
    batch.insertTimestamp(timestamp, indexes);
  }
}

void DatabaseBlockchainCache::pushTransaction(const CachedTransaction& cachedTransaction,
                                              uint32_t blockIndex,
                                              uint16_t transactionBlockIndex,
                                              BlockchainWriteBatch& batch) {

  logger(Logging::DEBUGGING) << "push transaction with hash " << cachedTransaction.getTransactionHash();
  const auto& tx = cachedTransaction.getTransaction();

  ExtendedTransactionInfo transactionCacheInfo;
  transactionCacheInfo.blockIndex = blockIndex;
  transactionCacheInfo.transactionIndex = transactionBlockIndex;
  transactionCacheInfo.transactionHash = cachedTransaction.getTransactionHash();
  transactionCacheInfo.unlockTime = tx.unlockTime;

  assert(tx.outputs.size() <= std::numeric_limits<uint16_t>::max());

  transactionCacheInfo.globalIndexes.reserve(tx.outputs.size());
  transactionCacheInfo.outputs.reserve(tx.outputs.size());
  auto outputCount = 0;
  std::unordered_map<Amount, std::vector<PackedOutIndex>> keyIndexes;
  std::unordered_map<Amount, std::vector<PackedOutIndex>> multiIndexes;

  std::set<Amount> newKeyAmounts;
  std::set<Amount> newMultisignatureAmounts;

  for (auto& output : tx.outputs) {
    transactionCacheInfo.outputs.push_back(output.target);

    PackedOutIndex poi;
    poi.blockIndex = blockIndex;
    poi.transactionIndex = transactionBlockIndex;
    poi.outputIndex = outputCount++;

    if (output.target.type() == typeid(KeyOutput)) {
      keyIndexes[output.amount].push_back(poi);
      auto outputCountForAmount = updateKeyOutputCount(output.amount, 1);
      if (outputCountForAmount == 1) {
        newKeyAmounts.insert(output.amount);
      }

      assert(outputCountForAmount > 0);
      auto globalIndex = outputCountForAmount - 1;
      transactionCacheInfo.globalIndexes.push_back(globalIndex);
      //output global index:
      transactionCacheInfo.amountToKeyIndexes[output.amount].push_back(globalIndex);

      KeyOutputInfo outputInfo;
      outputInfo.publicKey = boost::get<KeyOutput>(output.target).key;
      outputInfo.transactionHash = transactionCacheInfo.transactionHash;
      outputInfo.unlockTime = transactionCacheInfo.unlockTime;
      outputInfo.outputIndex = poi.outputIndex;

      batch.insertKeyOutputInfo(output.amount, globalIndex, outputInfo);
    } else if (output.target.type() == typeid(MultisignatureOutput)) {
      multiIndexes[output.amount].push_back(poi);
      auto outputCountForAmount = updateMultiOutputCount(output.amount, 1);
      if (outputCountForAmount == 1) {
        newMultisignatureAmounts.insert(output.amount);
      }

      assert(outputCountForAmount > 0);
      transactionCacheInfo.globalIndexes.push_back(outputCountForAmount - 1);
      //output global index:
      transactionCacheInfo.amountToMultiIndexes[output.amount].push_back(outputCountForAmount - 1);
    }
  }

  for (auto& amountToOutputs : keyIndexes) {
    batch.insertKeyOutputGlobalIndexes(amountToOutputs.first, amountToOutputs.second,
                                       updateKeyOutputCount(amountToOutputs.first, 0)); //Size already updated.
  }

  for (auto& amountToMultisigs : multiIndexes) {
    batch.insertMultisignatureOutputGlobalIndexes(amountToMultisigs.first, amountToMultisigs.second,
                                                  updateMultiOutputCount(amountToMultisigs.first, 0)); //Size already updated.
  }

  if (!newKeyAmounts.empty()) {
    assert(keyOutputAmountsCount.is_initialized());
    batch.insertKeyOutputAmounts(newKeyAmounts, *keyOutputAmountsCount);
  }
  
  if (!newMultisignatureAmounts.empty()) {
    assert(multiOutputAmountsCount.is_initialized());
    batch.insertMultisignatureOutputAmounts(newMultisignatureAmounts, *multiOutputAmountsCount);
  }

  Crypto::Hash paymentId;
  if (getPaymentIdFromTxExtra(cachedTransaction.getTransaction().extra, paymentId)) {
    insertPaymentId(batch, cachedTransaction.getTransactionHash(), paymentId);
  }

  batch.insertCachedTransaction(transactionCacheInfo, getCachedTransactionsCount() + 1);
  transactionsCount = *transactionsCount + 1;
  logger(Logging::DEBUGGING) << "push transaction with hash " << cachedTransaction.getTransactionHash() << " finished";
}

uint32_t DatabaseBlockchainCache::updateKeyOutputCount(Amount amount, int32_t diff) const {
  auto it = keyOutputCountsForAmounts.find(amount);
  if (it == keyOutputCountsForAmounts.end()) {
    logger(Logging::TRACE) << "updateKeyOutputCount: failed to found key for amount, request database";

    BlockchainReadBatch batch;
    auto result = readDatabase(batch.requestKeyOutputGlobalIndexesCountForAmount(amount));
    auto found = result.getKeyOutputGlobalIndexesCountForAmounts().find(amount);
    auto val = found != result.getKeyOutputGlobalIndexesCountForAmounts().end() ? found->second : 0;
    it = keyOutputCountsForAmounts.insert({ amount, val }).first;
    logger(Logging::TRACE) << "updateKeyOutputCount: database replied: amount " << amount << " value " << val;

    if (val == 0) {
      if (!keyOutputAmountsCount) {
        auto result = readDatabase(batch.requestKeyOutputAmountsCount());
        keyOutputAmountsCount = result.getKeyOutputAmountsCount();
      }

      keyOutputAmountsCount = *keyOutputAmountsCount + 1;
    }
  } else if (!keyOutputAmountsCount) {
    auto result = readDatabase(BlockchainReadBatch().requestKeyOutputAmountsCount());
    keyOutputAmountsCount = result.getKeyOutputAmountsCount();
  }

  it->second += diff;
  assert(it->second >= 0);
  return it->second;
}

uint32_t DatabaseBlockchainCache::updateMultiOutputCount(Amount amount, int32_t diff) const {
  auto it = multiOutputCountsForAmounts.find(amount);
  if (it == multiOutputCountsForAmounts.end()) {
    logger(Logging::TRACE) << "updateMultiOutputCount: failed to found key for amount, request database";

    BlockchainReadBatch batch;
    auto result = readDatabase(batch.requestMultisignatureOutputGlobalIndexesCountForAmount(amount));
    auto found = result.getMultisignatureOutputGlobalIndexesCountForAmounts().find(amount);
    auto val = found != result.getMultisignatureOutputGlobalIndexesCountForAmounts().end() ? found->second : 0;
    it = multiOutputCountsForAmounts.insert({ amount, val }).first;
    logger(Logging::TRACE) << "updateMultiOutputCount: database replied: amount " << amount << " value " << val;

    if (val == 0) {
      if (!multiOutputAmountsCount) {
        auto result = readDatabase(batch.requestMultisignatureOutputAmountsCount());
        multiOutputAmountsCount = result.getMultisignatureOutputAmountsCount();
      }

      multiOutputAmountsCount = *multiOutputAmountsCount + 1;
    }
  } else if (!multiOutputAmountsCount) {
    auto result = readDatabase(BlockchainReadBatch().requestMultisignatureOutputAmountsCount());
    multiOutputAmountsCount = result.getMultisignatureOutputAmountsCount();
  }


  it->second += diff;
  assert(it->second >= 0);
  return it->second;
}

void DatabaseBlockchainCache::insertPaymentId(BlockchainWriteBatch& batch, const Crypto::Hash& transactionHash, const Crypto::Hash& paymentId) {
  BlockchainReadBatch readBatch;
  uint32_t count = 0;

  auto readResult = readDatabase(readBatch.requestTransactionCountByPaymentId(paymentId));
  if (readResult.getTransactionCountByPaymentIds().count(paymentId) != 0) {
    count = readResult.getTransactionCountByPaymentIds().at(paymentId);
  }

  count += 1;

  batch.insertPaymentId(transactionHash, paymentId, count);
}

void DatabaseBlockchainCache::insertBlockTimestamp(BlockchainWriteBatch& batch, uint64_t timestamp, const Crypto::Hash& blockHash) {
  BlockchainReadBatch readBatch;
  readBatch.requestBlockHashesByTimestamp(timestamp);

  std::vector<Crypto::Hash> blockHashes;
  auto readResult = readDatabase(readBatch);

  if (readResult.getBlockHashesByTimestamp().count(timestamp) != 0) {
    blockHashes = readResult.getBlockHashesByTimestamp().at(timestamp);
  }

  blockHashes.emplace_back(blockHash);

  batch.insertTimestamp(timestamp, blockHashes);
}

void DatabaseBlockchainCache::pushBlock(const CachedBlock& cachedBlock,
                                        const std::vector<CachedTransaction>& cachedTransactions,
                                        const TransactionValidatorState& validatorState, size_t blockSize,
                                        uint64_t generatedCoins, Difficulty blockDifficulty, RawBlock&& rawBlock) {
  BlockchainWriteBatch batch;
  logger(Logging::DEBUGGING) << "push block with hash " << cachedBlock.getBlockHash() << ", and "
                             << cachedTransactions.size() + 1 << " transactions"; //+1 for base transaction

  // TODO: cache top block difficulty, size, timestamp, coins; use it here
  auto lastBlockInfo = getCachedBlockInfo(getTopBlockIndex());
  auto cumulativeDifficulty = lastBlockInfo.cumulativeDifficulty + blockDifficulty;
  auto alreadyGeneratedCoins = lastBlockInfo.alreadyGeneratedCoins + generatedCoins;
  auto alreadyGeneratedTransactions = lastBlockInfo.alreadyGeneratedTransactions + cachedTransactions.size() + 1;

  CachedBlockInfo blockInfo;
  blockInfo.blockHash = cachedBlock.getBlockHash();
  blockInfo.alreadyGeneratedCoins = alreadyGeneratedCoins;
  blockInfo.alreadyGeneratedTransactions = alreadyGeneratedTransactions;
  blockInfo.cumulativeDifficulty = cumulativeDifficulty;
  blockInfo.blockSize = static_cast<uint32_t>(blockSize);
  blockInfo.timestamp = cachedBlock.getBlock().timestamp;

  batch.insertSpentKeyImages(getTopBlockIndex() + 1, validatorState.spentKeyImages);
  batch.insertSpentMultisignatureOutputGlobalIndexes(getTopBlockIndex() + 1,
                                                     validatorState.spentMultisignatureGlobalIndexes);
  auto txHashes = cachedBlock.getBlock().transactionHashes;
  auto baseTransaction = cachedBlock.getBlock().baseTransaction;
  auto cachedBaseTransaction = CachedTransaction{std::move(baseTransaction)};

  // base transaction's hash is always the first one in index for this block
  txHashes.insert(txHashes.begin(), cachedBaseTransaction.getTransactionHash());

  batch.insertCachedBlock(blockInfo, getTopBlockIndex() + 1, txHashes);
  batch.insertRawBlock(getTopBlockIndex() + 1, std::move(rawBlock));

  auto transactionIndex = 0;
  pushTransaction(cachedBaseTransaction, getTopBlockIndex() + 1, transactionIndex++, batch);

  for (const auto& transaction: cachedTransactions) {
    pushTransaction(transaction, getTopBlockIndex() + 1, transactionIndex++, batch);
  }

  auto closestBlockIndexDb = requestClosestBlockIndexByTimestamp(roundToMidnight(cachedBlock.getBlock().timestamp), database);
  if (!closestBlockIndexDb.second) {
    logger(Logging::ERROR) << "push block " << cachedBlock.getBlockHash() << " request closest block index by timestamp failed";
    throw std::runtime_error("Couldn't get closest to timestamp block index");
  }

  if (!closestBlockIndexDb.first) {
    batch.insertClosestTimestampBlockIndex(roundToMidnight(cachedBlock.getBlock().timestamp), getTopBlockIndex() + 1);
  }

  insertBlockTimestamp(batch, cachedBlock.getBlock().timestamp, cachedBlock.getBlockHash());

  auto res = database.write(batch);
  if (res) {
    logger(Logging::ERROR) << "push block " << cachedBlock.getBlockHash() << " write failed: " << res.message();
    throw std::runtime_error(res.message());
  }

  topBlockIndex = *topBlockIndex + 1;
  topBlockHash = cachedBlock.getBlockHash();
  logger(Logging::DEBUGGING) << "push block " << cachedBlock.getBlockHash() << " completed";

  unitsCache.push_back(blockInfo);
  if (unitsCache.size() > unitsCacheSize) {
    unitsCache.pop_front();
  }
}

PushedBlockInfo DatabaseBlockchainCache::getPushedBlockInfo(uint32_t blockIndex) const {
  return getExtendedPushedBlockInfo(blockIndex).pushedBlockInfo;
}

bool DatabaseBlockchainCache::checkIfSpent(const Crypto::KeyImage& keyImage, uint32_t blockIndex) const {
  auto batch = BlockchainReadBatch().requestBlockIndexBySpentKeyImage(keyImage);
  auto res = database.read(batch);
  if (res) {
    logger(Logging::ERROR) << "checkIfSpent failed, request to database failed: " << res.message();
    return false;
  }

  auto readResult = batch.extractResult();
  auto it = readResult.getBlockIndexesBySpentKeyImages().find(keyImage);

  return it != readResult.getBlockIndexesBySpentKeyImages().end() && it->second <= blockIndex;
}

bool DatabaseBlockchainCache::checkIfSpent(const Crypto::KeyImage& keyImage) const {
  return checkIfSpent(keyImage, getTopBlockIndex());
}

bool DatabaseBlockchainCache::checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex) const {
  return checkIfSpentMultisignature(amount, globalIndex, getTopBlockIndex());
}

bool DatabaseBlockchainCache::checkIfSpentMultisignature(uint64_t amount, uint32_t globalIndex,
                                                         uint32_t blockIndex) const {
  auto batch = BlockchainReadBatch().requestMultisignatureOutputSpendingStatus(amount, globalIndex);
  auto res = database.read(batch);
  return !res && batch.extractResult().getMultisignatureOutputsSpendingStatuses().count({ amount, globalIndex }) > 0;
}

bool DatabaseBlockchainCache::isTransactionSpendTimeUnlocked(uint64_t unlockTime) const {
  return isTransactionSpendTimeUnlocked(unlockTime, getTopBlockIndex());
}

// TODO: pass time
bool DatabaseBlockchainCache::isTransactionSpendTimeUnlocked(uint64_t unlockTime, uint32_t blockIndex) const {
  if (unlockTime < currency.maxBlockHeight()) {
    // interpret as block index
    return blockIndex + currency.lockedTxAllowedDeltaBlocks() >= unlockTime;
  }

  // interpret as time
  return static_cast<uint64_t>(time(nullptr)) + currency.lockedTxAllowedDeltaSeconds() >= unlockTime;
}

ExtractOutputKeysResult
DatabaseBlockchainCache::extractKeyOutputKeys(uint64_t amount, Common::ArrayView<uint32_t> globalIndexes,
                                              std::vector<Crypto::PublicKey>& publicKeys) const {
  return extractKeyOutputKeys(amount, getTopBlockIndex(), globalIndexes, publicKeys);
}

ExtractOutputKeysResult
DatabaseBlockchainCache::extractKeyOutputKeys(uint64_t amount, uint32_t blockIndex,
                                              Common::ArrayView<uint32_t> globalIndexes,
                                              std::vector<Crypto::PublicKey>& publicKeys) const {
  return extractKeyOutputs(amount, blockIndex, globalIndexes, [this, &publicKeys, blockIndex] (const CachedTransactionInfo& info, PackedOutIndex index, uint32_t globalIndex) {
    if (!isTransactionSpendTimeUnlocked(info.unlockTime, blockIndex)) {
      logger(Logging::DEBUGGING) << "extractKeyOutputKeys: output " << globalIndex << " is locked";
      return ExtractOutputKeysResult::OUTPUT_LOCKED;
    }

    auto& output = info.outputs[index.outputIndex];
    assert(output.type() == typeid(KeyOutput));
    publicKeys.push_back(boost::get<KeyOutput>(output).key);

    return ExtractOutputKeysResult::SUCCESS;
  });
}

ExtractOutputKeysResult DatabaseBlockchainCache::extractKeyOtputIndexes(uint64_t amount,
                                                                        Common::ArrayView<uint32_t> globalIndexes,
                                                                        std::vector<PackedOutIndex>& outIndexes) const {
  if (!requestPackedOutputs(amount, globalIndexes, database, outIndexes)) {
    logger(Logging::ERROR) << "extractKeyOtputIndexes failed: failed to read database";
    return ExtractOutputKeysResult::INVALID_GLOBAL_INDEX;
  }

  return ExtractOutputKeysResult::SUCCESS;
}

ExtractOutputKeysResult DatabaseBlockchainCache::extractKeyOtputReferences(
    uint64_t amount, Common::ArrayView<uint32_t> globalIndexes,
    std::vector<std::pair<Crypto::Hash, size_t>>& outputReferences) const {

  return extractKeyOutputs(amount, getTopBlockIndex(), globalIndexes, [&outputReferences] (const CachedTransactionInfo& info, PackedOutIndex index, uint32_t globalIndex) {
    outputReferences.push_back(std::make_pair(info.transactionHash, index.outputIndex));
    return ExtractOutputKeysResult::SUCCESS;
  });
}

bool DatabaseBlockchainCache::getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex,
                                                              MultisignatureOutput& output,
                                                              uint64_t& unlockTime) const {
  return getMultisignatureOutputIfExists(amount, globalIndex, getTopBlockIndex(), output, unlockTime);
}

bool DatabaseBlockchainCache::getMultisignatureOutputIfExists(uint64_t amount, uint32_t globalIndex,
                                                              uint32_t blockIndex, MultisignatureOutput& output,
                                                              uint64_t& unlockTime) const {
  return doGetMultisignatureOutputIfExists(amount, globalIndex, blockIndex, [&output, &unlockTime] (const CachedTransactionInfo& transaction, PackedOutIndex packedOutput) {
    unlockTime = transaction.unlockTime;
    auto& transactionOutput = transaction.outputs[packedOutput.outputIndex];

    assert(transactionOutput.type() == typeid(MultisignatureOutput));
    output = boost::get<MultisignatureOutput>(transactionOutput);
  });
}

std::pair<Crypto::Hash, size_t> DatabaseBlockchainCache::getMultisignatureOutputReference(uint64_t amount,
                                                                                          uint32_t globalIndex) const {
  std::pair<Crypto::Hash, size_t> outputReference;

  bool res = doGetMultisignatureOutputIfExists(amount, globalIndex, getTopBlockIndex(), [&outputReference] (const CachedTransactionInfo& transaction, PackedOutIndex packedOutput) {
    outputReference.first = transaction.transactionHash;
    outputReference.second = packedOutput.outputIndex;
  });

  if (!res) {
    logger(Logging::DEBUGGING) << "getMultisignatureOutputReference failed: output not found";
    throw std::runtime_error("Output not found");
  }

  return outputReference;
}

bool DatabaseBlockchainCache::doGetMultisignatureOutputIfExists(
  uint64_t amount,
  uint32_t globalIndex,
  uint32_t blockIndex,
  std::function<void (const CachedTransactionInfo& transaction, PackedOutIndex packedOutput)> extractor) const {

  auto batch = BlockchainReadBatch().requestMultisignatureOutputGlobalIndexForAmount(amount, globalIndex);
  auto result = database.read(batch);
  if (result) {
    logger(Logging::ERROR) << "doGetMultisignatureOutputIfExists failed: request to database failed";
    return false;
  }

  auto readResult = batch.extractResult();
  auto it = readResult.getMultisignatureOutputGlobalIndexesForAmounts().find({ amount, globalIndex });
  if (it == readResult.getMultisignatureOutputGlobalIndexesForAmounts().end()) {
    logger(Logging::DEBUGGING) << "doGetMultisignatureOutputIfExists failed: output not found";
    return false;
  }

  auto packedOut = it->second;
  if (packedOut.blockIndex > blockIndex) {
    logger(Logging::ERROR) << "doGetMultisignatureOutputIfExists failed: output block index in future";
    return false;
  }

  std::vector<CachedTransactionInfo> transactions;
  if (!requestCachedTransactionInfos({packedOut}, database, transactions)) {
    logger(Logging::DEBUGGING) << "doGetMultisignatureOutputIfExists failed: requestCachedTransactionInfos failed";
    return false;
  }

  extractor(transactions[0], packedOut);
  return true;
}

uint32_t DatabaseBlockchainCache::getTopBlockIndex() const {
  if (!topBlockIndex) {
    auto batch = BlockchainReadBatch().requestLastBlockIndex();
    auto result = database.read(batch);

    if (result) {
      logger(Logging::ERROR) << "Failed to read top block index from database";
      throw std::system_error(result);
    }

    auto readResult = batch.extractResult();
    if (!readResult.getLastBlockIndex().second) {
      logger(Logging::TRACE) << "Top block index does not exist in database";
      topBlockIndex = 0;
    }

    topBlockIndex = readResult.getLastBlockIndex().first;
  }

  return *topBlockIndex;
}

uint64_t DatabaseBlockchainCache::getCachedTransactionsCount() const {
  if (!transactionsCount) {
    auto batch = BlockchainReadBatch().requestTransactionsCount();
    auto result = database.read(batch);

    if (result) {
      logger(Logging::ERROR) << "Failed to read transactions count from database";
      throw std::system_error(result);
    }

    auto readResult = batch.extractResult();
    if (!readResult.getTransactionsCount().second) {
      logger(Logging::TRACE) << "Transactions count does not exist in database";
      transactionsCount = 0;
    } else {
      transactionsCount = readResult.getTransactionsCount().first;
    }
  }

  return *transactionsCount;
}

const Crypto::Hash& DatabaseBlockchainCache::getTopBlockHash() const {
  if (!topBlockHash) {
    auto batch = BlockchainReadBatch().requestCachedBlock(getTopBlockIndex());
    auto result = readDatabase(batch);
    topBlockHash = result.getCachedBlocks().at(getTopBlockIndex()).blockHash;
  }
  return *topBlockHash;
}
uint32_t DatabaseBlockchainCache::getBlockCount() const {
  return getTopBlockIndex() + 1;
}

bool DatabaseBlockchainCache::hasBlock(const Crypto::Hash& blockHash) const {
  auto batch = BlockchainReadBatch().requestBlockIndexByBlockHash(blockHash);
  auto result = database.read(batch);
  return !result && batch.extractResult().getBlockIndexesByBlockHashes().count(blockHash);
}

uint32_t DatabaseBlockchainCache::getBlockIndex(const Crypto::Hash& blockHash) const {
  if (blockHash == getTopBlockHash()) {
    return getTopBlockIndex();
  }

  auto batch = BlockchainReadBatch().requestBlockIndexByBlockHash(blockHash);
  auto result = readDatabase(batch);
  return result.getBlockIndexesByBlockHashes().at(blockHash);
}

bool DatabaseBlockchainCache::hasTransaction(const Crypto::Hash& transactionHash) const {
  auto batch = BlockchainReadBatch().requestCachedTransaction(transactionHash);
  auto result = database.read(batch);
  return !result && batch.extractResult().getCachedTransactions().count(transactionHash);
}

std::vector<uint64_t> DatabaseBlockchainCache::getLastTimestamps(size_t count) const {
  return getLastTimestamps(count, getTopBlockIndex(), UseGenesis{true});
}
std::vector<uint64_t> DatabaseBlockchainCache::getLastTimestamps(size_t count, uint32_t blockIndex,
                                                                 UseGenesis useGenesis) const {
  return getLastUnits(count, blockIndex, useGenesis, [](const CachedBlockInfo& inf) { return inf.timestamp; });
}

std::vector<uint64_t> DatabaseBlockchainCache::getLastBlocksSizes(size_t count) const {
  return getLastBlocksSizes(count, getTopBlockIndex(), UseGenesis{true});
}

std::vector<uint64_t> DatabaseBlockchainCache::getLastBlocksSizes(size_t count, uint32_t blockIndex,
                                                                  UseGenesis useGenesis) const {
  return getLastUnits(count, blockIndex, useGenesis, [](const CachedBlockInfo& cb) { return cb.blockSize; });
}

std::vector<Difficulty> DatabaseBlockchainCache::getLastCumulativeDifficulties(size_t count, uint32_t blockIndex,
                                                                               UseGenesis useGenesis) const {
  return getLastUnits(count, blockIndex, useGenesis,
                      [](const CachedBlockInfo& info) { return info.cumulativeDifficulty; });
}
std::vector<Difficulty> DatabaseBlockchainCache::getLastCumulativeDifficulties(size_t count) const {
  return getLastCumulativeDifficulties(count, getTopBlockIndex(), UseGenesis{true});
}

Difficulty DatabaseBlockchainCache::getDifficultyForNextBlock() const {
  return getDifficultyForNextBlock(getTopBlockIndex());
}

Difficulty DatabaseBlockchainCache::getDifficultyForNextBlock(uint32_t blockIndex) const {
  assert(blockIndex <= getTopBlockIndex());
  auto timestamps = getLastTimestamps(currency.difficultyBlocksCount(), blockIndex, UseGenesis{false});
  auto commulativeDifficulties =
      getLastCumulativeDifficulties(currency.difficultyBlocksCount(), blockIndex, UseGenesis{false});
  return currency.nextDifficulty(std::move(timestamps), std::move(commulativeDifficulties));
}

Difficulty DatabaseBlockchainCache::getCurrentCumulativeDifficulty() const {
  return getCachedBlockInfo(getTopBlockIndex()).cumulativeDifficulty;
}

Difficulty DatabaseBlockchainCache::getCurrentCumulativeDifficulty(uint32_t blockIndex) const {
  assert(blockIndex <= getTopBlockIndex());
  return getCachedBlockInfo(blockIndex).cumulativeDifficulty;
}

CachedBlockInfo DatabaseBlockchainCache::getCachedBlockInfo(uint32_t index) const {
  auto batch = BlockchainReadBatch().requestCachedBlock(index);
  auto result = readDatabase(batch);
  return result.getCachedBlocks().at(index);
}

uint64_t DatabaseBlockchainCache::getAlreadyGeneratedCoins() const {
  return getAlreadyGeneratedCoins(getTopBlockIndex());
}

uint64_t DatabaseBlockchainCache::getAlreadyGeneratedCoins(uint32_t blockIndex) const {
  return getCachedBlockInfo(blockIndex).alreadyGeneratedCoins;
}

uint64_t DatabaseBlockchainCache::getAlreadyGeneratedTransactions(uint32_t blockIndex) const {
  return getCachedBlockInfo(blockIndex).alreadyGeneratedTransactions;
}

std::vector<CachedBlockInfo> DatabaseBlockchainCache::getLastCachedUnits(uint32_t blockIndex, size_t count, UseGenesis useGenesis) const {
  assert(blockIndex <= getTopBlockIndex());

  std::vector<CachedBlockInfo> cachedResult;
  uint32_t cacheStart = (getTopBlockIndex() + 1) - static_cast<uint32_t>(unitsCache.size());
  if (cacheStart == 0 && !useGenesis) {
    count = std::min(static_cast<size_t>(getTopBlockIndex()), count);
  cacheStart = 0;
  }

  if (cacheStart > blockIndex || count == 0) {
    return cachedResult;
  }

  count = std::min(blockIndex + 1, static_cast<uint32_t>(count));
  uint32_t offset = std::max(static_cast<uint32_t>(blockIndex + 1 - count), cacheStart) - cacheStart;
  assert(offset < unitsCache.size());

  cachedResult.reserve(unitsCache.size() - offset - (getTopBlockIndex() - blockIndex));
  for (size_t i = offset; (i + cacheStart) <= blockIndex; ++i) {
    cachedResult.push_back(unitsCache[i]);
  }

  return cachedResult;
}

std::vector<CachedBlockInfo> DatabaseBlockchainCache::getLastDbUnits(uint32_t blockIndex, size_t count, UseGenesis useGenesis) const {
  uint32_t readFrom = blockIndex + 1 - std::min(blockIndex + 1, static_cast<uint32_t>(count));
  if (readFrom == 0 && !useGenesis) {
    readFrom += 1;
  }

  uint32_t toRead = blockIndex - readFrom + 1;
  std::vector<CachedBlockInfo> units;
  units.reserve(toRead);

  const uint32_t step = 200;
  while (toRead > 0) {
    auto next = std::min(toRead, step);
    toRead -= next;

    BlockchainReadBatch batch;
    for (auto id = readFrom; id < readFrom + next; ++id) {
      batch.requestCachedBlock(id);
    }

    readFrom += next;

    auto res = readDatabase(batch);

    std::map<uint32_t, CachedBlockInfo> sortedResult(res.getCachedBlocks().begin(), res.getCachedBlocks().end());
    for (const auto& kv: sortedResult) {
      units.push_back(kv.second);
    }
//    std::transform(sortedResult.begin(), sortedResult.end(), std::back_inserter(units),
//                   [&](const std::pair<uint32_t, CachedBlockInfo>& cb) { return pred(cb.second); });
  }

  return units;
}

std::vector<uint64_t>
DatabaseBlockchainCache::getLastUnits(size_t count, uint32_t blockIndex, UseGenesis useGenesis,
                                      std::function<uint64_t(const CachedBlockInfo&)> pred) const {
  assert(count <= std::numeric_limits<uint32_t>::max());

  auto cachedUnits = getLastCachedUnits(blockIndex, count, useGenesis);

  uint32_t availableUnits = blockIndex;
  if (useGenesis) {
    availableUnits += 1;
  }

  assert(availableUnits >= cachedUnits.size());

  if (availableUnits - cachedUnits.size() == 0) {
    std::vector<uint64_t> result;
    result.reserve(cachedUnits.size());
    for (const auto& unit: cachedUnits) {
      result.push_back(pred(unit));
    }

    return result;
  }

  assert(blockIndex + 1 >= cachedUnits.size());
  uint32_t dbIndex = blockIndex - static_cast<uint32_t>(cachedUnits.size());

  assert(count >= cachedUnits.size());
  size_t leftCount = count - cachedUnits.size();

  auto dbUnits = getLastDbUnits(dbIndex, leftCount, useGenesis);
  std::vector<uint64_t> result;
  result.reserve(dbUnits.size() + cachedUnits.size());
  for (const auto& unit: dbUnits) {
    result.push_back(pred(unit));
  }

  for (const auto& unit: cachedUnits) {
    result.push_back(pred(unit));
  }

  return result;
}

Crypto::Hash DatabaseBlockchainCache::getBlockHash(uint32_t blockIndex) const {
  if (blockIndex == getTopBlockIndex()) {
    return getTopBlockHash();
  }

  auto batch = BlockchainReadBatch().requestCachedBlock(blockIndex);
  auto result = readDatabase(batch);
  return result.getCachedBlocks().at(blockIndex).blockHash;
}

std::vector<Crypto::Hash> DatabaseBlockchainCache::getBlockHashes(uint32_t startIndex, size_t maxCount) const {
  assert(startIndex <= getTopBlockIndex());
  assert(maxCount <= std::numeric_limits<uint32_t>::max());

  uint32_t count = std::min(getTopBlockIndex() - startIndex + 1, static_cast<uint32_t>(maxCount));
  if (count == 0) {
    return {};
  }

  BlockchainReadBatch request;
  auto index = startIndex;
  while (index != startIndex + count) {
    request.requestCachedBlock(index++);
  }

  auto result = readDatabase(request);
  assert(result.getCachedBlocks().size() == count);

  std::vector<Crypto::Hash> hashes;
  hashes.reserve(count);

  std::map<uint32_t, CachedBlockInfo> sortedResult(
    result.getCachedBlocks().begin(), result.getCachedBlocks().end());

  std::transform(sortedResult.begin(), sortedResult.end(), std::back_inserter(hashes),
                 [](const std::pair<uint32_t, CachedBlockInfo>& cb) { return cb.second.blockHash; });
  return hashes;
}

IBlockchainCache* DatabaseBlockchainCache::getParent() const {
  return nullptr;
}

uint32_t DatabaseBlockchainCache::getStartBlockIndex() const {
  return 0;
}

size_t DatabaseBlockchainCache::getKeyOutputsCountForAmount(uint64_t amount, uint32_t blockIndex) const {
  uint32_t outputsCount = requestKeyOutputGlobalIndexesCountForAmount(amount, database);

  auto getOutput = std::bind(retrieveKeyOutput, std::placeholders::_1, std::placeholders::_2, std::ref(database));
  auto begin = DbOutputConstIterator(getOutput, amount, 0);
  auto end = DbOutputConstIterator(getOutput, amount, outputsCount);

  auto it = std::lower_bound(begin, end, blockIndex, [] (const PackedOutIndex& output, uint32_t blockIndex) {
    return output.blockIndex < blockIndex;
  });

  size_t result = static_cast<size_t>(std::distance(begin, it));
  logger(Logging::DEBUGGING) << "Key outputs count for amount " << amount << " is " << result << " by block index " << blockIndex;

  return result;
}

size_t DatabaseBlockchainCache::getMultisignatureCountForAmount(uint64_t amount, uint32_t blockIndex) const {
  uint32_t outputsCount = requestMultisignatureOutputGlobalIndexesCountForAmount(amount, database);

  auto getOutput = std::bind(retrieveMultisignatureOutput, std::placeholders::_1, std::placeholders::_2, std::ref(database));
  auto begin = DbOutputConstIterator(getOutput, amount, 0);
  auto end = DbOutputConstIterator(getOutput, amount, outputsCount);

  auto it = std::lower_bound(begin, end, blockIndex, [] (const PackedOutIndex& output, uint32_t blockIndex) {
    return output.blockIndex < blockIndex;
  });

  size_t result = static_cast<size_t>(std::distance(begin, it));
  logger(Logging::DEBUGGING) << "Multisignature outputs count for amount " << amount << " is " << result << " by block index " << blockIndex;

  return result;
}

uint32_t DatabaseBlockchainCache::getTimestampLowerBoundBlockIndex(uint64_t timestamp) const {
  auto midnight = roundToMidnight(timestamp);

  while (midnight > 0) {
    auto dbRes = requestClosestBlockIndexByTimestamp(midnight, database);
    if (!dbRes.second) {
      logger(Logging::DEBUGGING) << "getTimestampLowerBoundBlockIndex failed: failed to read database";
      throw std::runtime_error("Couldn't get closest to timestamp block index");
    }

    if (!dbRes.first) {
      midnight -= 60 * 60 * 24;
      continue;
    }

    return *dbRes.first;
  }

  return 0;
}

bool DatabaseBlockchainCache::getTransactionGlobalIndexes(const Crypto::Hash& transactionHash,
                                                          std::vector<uint32_t>& globalIndexes) const {
  auto batch = BlockchainReadBatch().requestCachedTransaction(transactionHash);
  auto result = database.read(batch);
  if (result) {
    logger(Logging::DEBUGGING) << "getTransactionGlobalIndexes failed: failed to read database";
    return false;
  }

  auto readResult = batch.extractResult();
  auto it = readResult.getCachedTransactions().find(transactionHash);
  if (it == readResult.getCachedTransactions().end()) {
    logger(Logging::DEBUGGING) << "getTransactionGlobalIndexes failed: cached transaction for hash " << transactionHash << " not present";
    return false;
  }

  globalIndexes = it->second.globalIndexes;
  return true;
}

size_t DatabaseBlockchainCache::getTransactionCount() const {
  return static_cast<size_t>(getCachedTransactionsCount());
}

void DatabaseBlockchainCache::addSpentMultisignature(uint64_t amount, uint32_t globalIndex, uint32_t blockIndex) {
  database.write(BlockchainWriteBatch().insertSpentMultisignatureOutputGlobalIndexes(blockIndex, {{amount, globalIndex}}));
}

uint32_t DatabaseBlockchainCache::getBlockIndexContainingTx(const Crypto::Hash& transactionHash) const {
  auto batch = BlockchainReadBatch().requestCachedTransaction(transactionHash);
  auto result = readDatabase(batch);
  return result.getCachedTransactions().at(transactionHash).blockIndex;
}

size_t DatabaseBlockchainCache::getChildCount() const {
  return children.size();
}

void DatabaseBlockchainCache::save() {
}

void DatabaseBlockchainCache::load() {
}

std::vector<BinaryArray>
DatabaseBlockchainCache::getRawTransactions(const std::vector<Crypto::Hash>& transactions,
                                            std::vector<Crypto::Hash>& missedTransactions) const {
  std::vector<BinaryArray> found;
  getRawTransactions(transactions, found, missedTransactions);
  return found;
}

std::vector<BinaryArray>
DatabaseBlockchainCache::getRawTransactions(const std::vector<Crypto::Hash>& transactions) const {
  std::vector<Crypto::Hash> missed;
  std::vector<BinaryArray> found;
  getRawTransactions(transactions, found, missed);
  return found;
}

void DatabaseBlockchainCache::getRawTransactions(const std::vector<Crypto::Hash>& transactions,
                                                 std::vector<BinaryArray>& foundTransactions,
                                                 std::vector<Crypto::Hash>& missedTransactions) const {
  BlockchainReadBatch batch;
  for (auto& hash : transactions) {
    batch.requestCachedTransaction(hash);
  }

  auto res = readDatabase(batch);
  for (auto& tx : res.getCachedTransactions()) {
    batch.requestRawBlock(tx.second.blockIndex);
  }

  auto blocks = readDatabase(batch);

  foundTransactions.reserve(foundTransactions.size() + transactions.size());
  auto& hashesMap = res.getCachedTransactions();
  auto& blocksMap = blocks.getRawBlocks();
  for (const auto& hash: transactions) {
    auto transactionIt = hashesMap.find(hash);
    if (transactionIt == hashesMap.end()) {
      logger(Logging::DEBUGGING) << "detected missing transaction for hash " << hash << " in getRawTransaction";
      missedTransactions.push_back(hash);
      continue;
    }

    auto blockIt = blocksMap.find(transactionIt->second.blockIndex);
    if (blockIt == blocksMap.end()) {
      logger(Logging::DEBUGGING) << "detected missing transaction for hash " << hash << " in getRawTransaction";
      missedTransactions.push_back(hash);
      continue;
    }

    if (transactionIt->second.transactionIndex == 0) {
      auto block = fromBinaryArray<BlockTemplate>(blockIt->second.block);
      foundTransactions.emplace_back(toBinaryArray(block.baseTransaction));
    } else {
      assert(blockIt->second.transactions.size() >= transactionIt->second.transactionIndex - 1);
      foundTransactions.emplace_back(blockIt->second.transactions[transactionIt->second.transactionIndex - 1]);
    }
  }
}

RawBlock DatabaseBlockchainCache::getBlockByIndex(uint32_t index) const {
  auto batch = BlockchainReadBatch().requestRawBlock(index);
  auto res = readDatabase(batch);
  return std::move(res.getRawBlocks().at(index));
}

BinaryArray DatabaseBlockchainCache::getRawTransaction(uint32_t blockIndex, uint32_t transactionIndex) const {
  return getBlockByIndex(blockIndex).transactions.at(transactionIndex);
}

std::vector<Crypto::Hash> DatabaseBlockchainCache::getTransactionHashes() const {
  assert(false);
  return {};
}

std::vector<uint32_t> DatabaseBlockchainCache::getRandomOutsByAmount(uint64_t amount, size_t count,
                                                                     uint32_t blockIndex) const {
  auto batch = BlockchainReadBatch().requestKeyOutputGlobalIndexesCountForAmount(amount);
  auto result = readDatabase(batch);
  auto outputsCount = result.getKeyOutputGlobalIndexesCountForAmounts();
  auto outputsToPick = std::min(static_cast<uint32_t>(count), outputsCount[amount]);

  std::vector<uint32_t> resultOuts;
  resultOuts.reserve(outputsToPick);

  ShuffleGenerator<uint32_t, Crypto::random_engine<uint32_t>> generator(outputsCount[amount]);

  while (outputsToPick) {
    std::vector<uint32_t> globalIndexes;
    globalIndexes.reserve(outputsToPick);

    try {
      for (uint32_t i = 0; i < outputsToPick; ++i, globalIndexes.push_back(generator())) { }
      //std::generate_n(std::back_inserter(globalIndexes), outputsToPick, generator);
    } catch (const SequenceEnded&) {
      logger(Logging::TRACE) << "getRandomOutsByAmount: generator reached sequence end";
      return resultOuts;
    }

    std::vector<PackedOutIndex> outputs;
    if (extractKeyOtputIndexes(amount, Common::ArrayView<uint32_t>(globalIndexes.data(), globalIndexes.size()), outputs) != ExtractOutputKeysResult::SUCCESS) {
      logger(Logging::DEBUGGING) << "getRandomOutsByAmount: failed to extract key output indexes";
      throw std::runtime_error("Invalid output index"); //TODO: make error code
    }

    std::vector<ExtendedTransactionInfo> transactions;
    if (!requestExtendedTransactionInfos(outputs, database, transactions)) {
      logger(Logging::TRACE) << "getRandomOutsByAmount: requestExtendedTransactionInfos failed";
      throw std::runtime_error("Error while requesting transactions"); //TODO: make error code
    }

    assert(globalIndexes.size() == transactions.size());

    uint32_t uppperBlockIndex = 0;
    if (blockIndex > currency.minedMoneyUnlockWindow()) {
      uppperBlockIndex = blockIndex - currency.minedMoneyUnlockWindow();
    }

    for (size_t i = 0; i < transactions.size(); ++i) {
      if (!isTransactionSpendTimeUnlocked(transactions[i].unlockTime, blockIndex) || transactions[i].blockIndex > uppperBlockIndex) {
        continue;
      }

      resultOuts.push_back(globalIndexes[i]);
      --outputsToPick;
    }
  }

  return resultOuts;
}

ExtractOutputKeysResult DatabaseBlockchainCache::extractKeyOutputs(
    uint64_t amount, uint32_t blockIndex, Common::ArrayView<uint32_t> globalIndexes,
    std::function<ExtractOutputKeysResult(const CachedTransactionInfo& info, PackedOutIndex index,
                                          uint32_t globalIndex)> callback) const {
  BlockchainReadBatch batch;
  for (auto it = globalIndexes.begin(); it != globalIndexes.end(); ++it) {
    batch.requestKeyOutputInfo(amount, *it);
  }

  auto result = readDatabase(batch).getKeyOutputInfo();
  std::map<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>, KeyOutputInfo> sortedResult(result.begin(), result.end());
  for (const auto& kv: sortedResult) {
    ExtendedTransactionInfo tx;
    tx.unlockTime = kv.second.unlockTime;
    tx.transactionHash = kv.second.transactionHash;
    tx.outputs.resize(kv.second.outputIndex + 1);
    tx.outputs[kv.second.outputIndex] = KeyOutput{kv.second.publicKey};
    PackedOutIndex fakePoi;
    fakePoi.outputIndex = kv.second.outputIndex;

    //TODO: change the interface of extractKeyOutputs to return vector of structures instead of passing callback as predicate
    auto ret = callback(tx, fakePoi, kv.first.second);
    if (ret != ExtractOutputKeysResult::SUCCESS) {
      logger(Logging::DEBUGGING) << "extractKeyOutputs failed : callback returned error";
      return ret;
    }
  }

  return ExtractOutputKeysResult::SUCCESS;
}

std::vector<Crypto::Hash> DatabaseBlockchainCache::getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) const {
  auto countBatch = BlockchainReadBatch().requestTransactionCountByPaymentId(paymentId);
  uint32_t transactionsCountByPaymentId = readDatabase(countBatch).getTransactionCountByPaymentIds().at(paymentId);

  BlockchainReadBatch transactionBatch;
  for (uint32_t i = 0; i < transactionsCountByPaymentId; ++i) {
    transactionBatch.requestTransactionHashByPaymentId(paymentId, i);
  }

  auto result = readDatabase(transactionBatch);
  std::vector<Crypto::Hash> transactionHashes;
  transactionHashes.reserve(result.getTransactionHashesByPaymentIds().size());
  for(const auto& kv: result.getTransactionHashesByPaymentIds()) {
    transactionHashes.emplace_back(kv.second);
  }

  return transactionHashes;
}

std::vector<Crypto::Hash> DatabaseBlockchainCache::getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const {
  std::vector<Crypto::Hash> blockHashes;
  if (secondsCount == 0) {
    return blockHashes;
  }

  BlockchainReadBatch batch;
  for (uint64_t timestamp = timestampBegin; timestamp < timestampBegin + static_cast<uint64_t>(secondsCount); ++timestamp) {
    batch.requestBlockHashesByTimestamp(timestamp);
  }

  auto result = readDatabase(batch);
  for (uint64_t timestamp = timestampBegin; timestamp < timestampBegin + static_cast<uint64_t>(secondsCount); ++timestamp) {
    if (result.getBlockHashesByTimestamp().count(timestamp) == 0) {
      continue;
    }

    const auto& hashes = result.getBlockHashesByTimestamp().at(timestamp);
    blockHashes.insert(blockHashes.end(), hashes.begin(), hashes.end());
  }

  return blockHashes;
}

DatabaseBlockchainCache::ExtendedPushedBlockInfo DatabaseBlockchainCache::getExtendedPushedBlockInfo(uint32_t blockIndex) const {
  assert(blockIndex <= getTopBlockIndex());

  auto batch = BlockchainReadBatch()
    .requestRawBlock(blockIndex)
    .requestCachedBlock(blockIndex)
    .requestSpentKeyImagesByBlock(blockIndex)
    .requestSpentMultisignatureOutputGlobalIndexesByBlock(blockIndex);

  if (blockIndex > 0) {
    batch.requestCachedBlock(blockIndex - 1);
  }

  auto dbResult = readDatabase(batch);
  const CachedBlockInfo& blockInfo = dbResult.getCachedBlocks().at(blockIndex);
  const CachedBlockInfo& previousBlockInfo = blockIndex > 0 ? dbResult.getCachedBlocks().at(blockIndex - 1) : NULL_CACHED_BLOCK_INFO;

  ExtendedPushedBlockInfo extendedInfo;

  extendedInfo.pushedBlockInfo.rawBlock = dbResult.getRawBlocks().at(blockIndex);
  extendedInfo.pushedBlockInfo.blockSize = blockInfo.blockSize;
  extendedInfo.pushedBlockInfo.blockDifficulty = blockInfo.cumulativeDifficulty - previousBlockInfo.cumulativeDifficulty;
  extendedInfo.pushedBlockInfo.generatedCoins = blockInfo.alreadyGeneratedCoins - previousBlockInfo.alreadyGeneratedCoins;

  const auto& spentKeyImages = dbResult.getSpentKeyImagesByBlock().at(blockIndex);
  const auto& spentMultisignatures = dbResult.getSpentMultisignatureOutputGlobalIndexesByBlocks().at(blockIndex);

  extendedInfo.pushedBlockInfo.validatorState.spentKeyImages.insert(spentKeyImages.begin(), spentKeyImages.end());
  extendedInfo.pushedBlockInfo.validatorState.spentMultisignatureGlobalIndexes.insert(spentMultisignatures.begin(), spentMultisignatures.end());

  extendedInfo.timestamp = blockInfo.timestamp;

  return extendedInfo;
}

void DatabaseBlockchainCache::setParent(IBlockchainCache* ptr) {
  assert(false);
}

void DatabaseBlockchainCache::addChild(IBlockchainCache* ptr) {
  if (std::find(children.begin(), children.end(), ptr) == children.end()) {
    children.push_back(ptr);
  }
}

bool DatabaseBlockchainCache::deleteChild(IBlockchainCache* ptr) {
  auto it = std::remove(children.begin(), children.end(), ptr);
  auto res = it != children.end();
  children.erase(it, children.end());
  return res;
}

BlockchainReadResult DatabaseBlockchainCache::readDatabase(BlockchainReadBatch& batch) const {
  auto result = database.read(batch);
  if (result) {
    logger(Logging::ERROR) << "failed to read database, error is " << result.message();
    throw std::runtime_error(result.message());
  }

  return batch.extractResult();
}

void DatabaseBlockchainCache::addGenesisBlock(CachedBlock&& genesisBlock) {
  uint64_t minerReward = 0;
  for (const TransactionOutput& output : genesisBlock.getBlock().baseTransaction.outputs) {
    minerReward += output.amount;
  }

  assert(minerReward > 0);

  uint64_t baseTransactionSize = getObjectBinarySize(genesisBlock.getBlock().baseTransaction);
  assert(baseTransactionSize < std::numeric_limits<uint32_t>::max());

  BlockchainWriteBatch batch;

  CachedBlockInfo blockInfo{genesisBlock.getBlockHash(), genesisBlock.getBlock().timestamp, 1,
                            minerReward, 1, uint32_t(baseTransactionSize)};

  auto baseTransaction = genesisBlock.getBlock().baseTransaction;
  auto cachedBaseTransaction = CachedTransaction{std::move(baseTransaction)};

  pushTransaction(cachedBaseTransaction, 0, 0, batch);

  batch.insertCachedBlock(blockInfo, 0, {cachedBaseTransaction.getTransactionHash()});
  batch.insertRawBlock(0, {toBinaryArray(genesisBlock.getBlock()), {}});
  batch.insertClosestTimestampBlockIndex(roundToMidnight(genesisBlock.getBlock().timestamp), 0);

  auto res = database.write(batch);
  if (res) {
    logger(Logging::ERROR) << "addGenesisBlock failed: failed to write to database, " << res.message();
    throw std::runtime_error(res.message());
  }

  topBlockHash = genesisBlock.getBlockHash();

  unitsCache.push_back(blockInfo);
}

}
