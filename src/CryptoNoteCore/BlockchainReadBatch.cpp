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

#include "BlockchainReadBatch.h"

#include <boost/range/combine.hpp>

#include "DBUtils.h"

using namespace CryptoNote;


BlockchainReadBatch::BlockchainReadBatch() {

}

BlockchainReadBatch::~BlockchainReadBatch() {

}

BlockchainReadBatch& BlockchainReadBatch::requestSpentKeyImagesByBlock(uint32_t blockIndex) {
  state.spentKeyImagesByBlock.emplace(blockIndex, std::vector<Crypto::KeyImage>());
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestBlockIndexBySpentKeyImage(const Crypto::KeyImage& keyImage) {
  state.blockIndexesBySpentKeyImages.emplace(keyImage, 0);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestCachedTransaction(const Crypto::Hash& txHash) {
  state.cachedTransactions.emplace(txHash, ExtendedTransactionInfo());
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestTransactionHashesByBlock(uint32_t blockIndex) {
  state.transactionHashesByBlocks.emplace(blockIndex, std::vector<Crypto::Hash>());
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestCachedBlock(uint32_t blockIndex) {
  state.cachedBlocks.emplace(blockIndex, CachedBlockInfo());
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestBlockIndexByBlockHash(const Crypto::Hash& blockHash) {
  state.blockIndexesByBlockHashes.emplace(blockHash, 0);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestKeyOutputGlobalIndexesCountForAmount(IBlockchainCache::Amount amount) {
  state.keyOutputGlobalIndexesCountForAmounts.emplace(amount, 0);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestKeyOutputGlobalIndexForAmount(IBlockchainCache::Amount amount, uint32_t outputIndexWithinAmout) {
  state.keyOutputGlobalIndexesForAmounts.emplace(std::make_pair(amount, outputIndexWithinAmout), PackedOutIndex());
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestMultisignatureOutputGlobalIndexesCountForAmount(IBlockchainCache::Amount amount) {
  state.multisignatureOutputGlobalIndexesCountForAmounts.emplace(amount, 0);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestMultisignatureOutputGlobalIndexForAmount(IBlockchainCache::Amount amount, uint32_t outputIndexWithinAmout) {
  state.multisignatureOutputGlobalIndexesForAmounts.emplace(std::make_pair(amount, outputIndexWithinAmout), PackedOutIndex());
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestSpentMultisignatureOutputGlobalIndexesByBlock(uint32_t blockIndex) {
  state.spentMultisignatureOutputGlobalIndexesByBlocks.insert({blockIndex, {}});
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestMultisignatureOutputSpendingStatus(IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex index) {
  state.multisignatureOutputsSpendingStatuses.emplace(std::make_pair(amount, index), false);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestRawBlock(uint32_t blockIndex) {
  state.rawBlocks.emplace(blockIndex, RawBlock());
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestLastBlockIndex() {
  state.lastBlockIndex.second = true;
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestClosestTimestampBlockIndex(uint64_t timestamp) {
  state.closestTimestampBlockIndex[timestamp];
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestKeyOutputAmountsCount() {
  state.keyOutputAmountsCount.second = true;
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestMultisignatureOutputAmountsCount() {
  state.multisignatureOutputAmountsCount.second = true;
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestKeyOutputAmount(uint32_t index) {
  state.keyOutputAmounts.emplace(index, 0);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestMultisignatureOutputAmount(uint32_t index) {
  state.multisignatureOutputAmounts.emplace(index, 0);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestTransactionCountByPaymentId(const Crypto::Hash& paymentId) {
  state.transactionCountsByPaymentIds.emplace(paymentId, 0);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestTransactionHashByPaymentId(const Crypto::Hash& paymentId, uint32_t transactionIndexWithinPaymentId) {
  state.transactionHashesByPaymentIds.emplace(std::make_pair(paymentId, transactionIndexWithinPaymentId), NULL_HASH);
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestBlockHashesByTimestamp(uint64_t timestamp) {
  state.blockHashesByTimestamp.emplace(timestamp, std::vector<Crypto::Hash>());
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestTransactionsCount() {
  state.transactionsCount.second = true;
  return *this;
}

BlockchainReadBatch& BlockchainReadBatch::requestKeyOutputInfo(IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex globalIndex) {
  state.keyOutputKeys.emplace(std::make_pair(amount, globalIndex), KeyOutputInfo{});
  return *this;
}

BlockchainReadResult BlockchainReadBatch::extractResult() {
  assert(resultSubmitted);
  auto st = std::move(state);
  state.lastBlockIndex = {0, false};
  state.keyOutputAmountsCount = {{}, false};
  state.multisignatureOutputAmountsCount = {{}, false};

  resultSubmitted = false;
  return BlockchainReadResult(st);
}

std::vector<std::string> BlockchainReadBatch::getRawKeys() const {
  std::vector<std::string> rawKeys;
  rawKeys.reserve(state.size());

  DB::serializeKeys(rawKeys, DB::BLOCK_INDEX_TO_KEY_IMAGE_PREFIX, state.spentKeyImagesByBlock);
  DB::serializeKeys(rawKeys, DB::KEY_IMAGE_TO_BLOCK_INDEX_PREFIX, state.blockIndexesBySpentKeyImages);
  DB::serializeKeys(rawKeys, DB::TRANSACTION_HASH_TO_TRANSACTION_INFO_PREFIX, state.cachedTransactions);
  DB::serializeKeys(rawKeys, DB::BLOCK_INDEX_TO_TX_HASHES_PREFIX, state.transactionHashesByBlocks);
  DB::serializeKeys(rawKeys, DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX, state.cachedBlocks);
  DB::serializeKeys(rawKeys, DB::BLOCK_HASH_TO_BLOCK_INDEX_PREFIX, state.blockIndexesByBlockHashes);
  DB::serializeKeys(rawKeys, DB::KEY_OUTPUT_AMOUNT_PREFIX, state.keyOutputGlobalIndexesCountForAmounts);
  DB::serializeKeys(rawKeys, DB::KEY_OUTPUT_AMOUNT_PREFIX, state.keyOutputGlobalIndexesForAmounts);
  DB::serializeKeys(rawKeys, DB::MULTISIGNATURE_OUTPUT_AMOUNT_PREFIX, state.multisignatureOutputGlobalIndexesCountForAmounts);
  DB::serializeKeys(rawKeys, DB::MULTISIGNATURE_OUTPUT_AMOUNT_PREFIX, state.multisignatureOutputGlobalIndexesForAmounts);
  DB::serializeKeys(rawKeys, DB::BLOCK_INDEX_TO_SPENT_MULTISIGNATURE_PREFIX, state.spentMultisignatureOutputGlobalIndexesByBlocks);
  DB::serializeKeys(rawKeys, DB::SPENT_MULTISIGNATURE_OUTPUT_AMOUNT_PREFIX, state.multisignatureOutputsSpendingStatuses);
  DB::serializeKeys(rawKeys, DB::BLOCK_INDEX_TO_RAW_BLOCK_PREFIX, state.rawBlocks);
  DB::serializeKeys(rawKeys, DB::CLOSEST_TIMESTAMP_BLOCK_INDEX_PREFIX, state.closestTimestampBlockIndex);
  DB::serializeKeys(rawKeys, DB::KEY_OUTPUT_AMOUNTS_COUNT_PREFIX, state.keyOutputAmounts);
  DB::serializeKeys(rawKeys, DB::MULTISIGNATURE_OUTPUT_AMOUNTS_COUNT_PREFIX, state.multisignatureOutputAmounts);
  DB::serializeKeys(rawKeys, DB::PAYMENT_ID_TO_TX_HASH_PREFIX, state.transactionCountsByPaymentIds);
  DB::serializeKeys(rawKeys, DB::PAYMENT_ID_TO_TX_HASH_PREFIX, state.transactionHashesByPaymentIds);
  DB::serializeKeys(rawKeys, DB::TIMESTAMP_TO_BLOCKHASHES_PREFIX, state.blockHashesByTimestamp);
  DB::serializeKeys(rawKeys, DB::KEY_OUTPUT_KEY_PREFIX, state.keyOutputKeys);

  if (state.lastBlockIndex.second) {
    rawKeys.emplace_back(DB::serializeKey(DB::BLOCK_INDEX_TO_BLOCK_HASH_PREFIX, DB::LAST_BLOCK_INDEX_KEY));
  }

  if (state.keyOutputAmountsCount.second) {
    rawKeys.emplace_back(DB::serializeKey(DB::KEY_OUTPUT_AMOUNTS_COUNT_PREFIX, DB::KEY_OUTPUT_AMOUNTS_COUNT_KEY));
  }

  if (state.multisignatureOutputAmountsCount.second) {
    rawKeys.emplace_back(DB::serializeKey(DB::MULTISIGNATURE_OUTPUT_AMOUNTS_COUNT_PREFIX, DB::MULTISIGNATURE_OUTPUT_AMOUNTS_COUNT_KEY));
  }

  if (state.transactionsCount.second) {
    rawKeys.emplace_back(DB::serializeKey(DB::TRANSACTION_HASH_TO_TRANSACTION_INFO_PREFIX, DB::TRANSACTIONS_COUNT_KEY));
  }

  assert(!rawKeys.empty());
  return rawKeys;
}

BlockchainReadResult::BlockchainReadResult(BlockchainReadState _state) : state(std::move(_state)) {

}

BlockchainReadResult::~BlockchainReadResult() {

}

const std::unordered_map<uint32_t, std::vector<Crypto::KeyImage>>& BlockchainReadResult::getSpentKeyImagesByBlock() const {
  return state.spentKeyImagesByBlock;
}

const std::unordered_map<Crypto::KeyImage, uint32_t>& BlockchainReadResult::getBlockIndexesBySpentKeyImages() const {
  return state.blockIndexesBySpentKeyImages;
}

const std::unordered_map<Crypto::Hash, ExtendedTransactionInfo>& BlockchainReadResult::getCachedTransactions() const {
  return state.cachedTransactions;
}

const std::unordered_map<uint32_t, std::vector<Crypto::Hash>>& BlockchainReadResult::getTransactionHashesByBlocks() const {
  return state.transactionHashesByBlocks;
}

const std::unordered_map<uint32_t, CachedBlockInfo>& BlockchainReadResult::getCachedBlocks() const {
  return state.cachedBlocks;
}

const std::unordered_map<Crypto::Hash, uint32_t>& BlockchainReadResult::getBlockIndexesByBlockHashes() const {
  return state.blockIndexesByBlockHashes;
}

const std::unordered_map<IBlockchainCache::Amount, uint32_t>& BlockchainReadResult::getKeyOutputGlobalIndexesCountForAmounts() const {
  return state.keyOutputGlobalIndexesCountForAmounts;
}

const std::unordered_map<std::pair<IBlockchainCache::Amount, uint32_t>, PackedOutIndex>& BlockchainReadResult::getKeyOutputGlobalIndexesForAmounts() const {
  return state.keyOutputGlobalIndexesForAmounts;
}

const std::unordered_map<IBlockchainCache::Amount, uint32_t>& BlockchainReadResult::getMultisignatureOutputGlobalIndexesCountForAmounts() const {
  return state.multisignatureOutputGlobalIndexesCountForAmounts;
}

const std::unordered_map<std::pair<IBlockchainCache::Amount, uint32_t>, PackedOutIndex>& BlockchainReadResult::getMultisignatureOutputGlobalIndexesForAmounts() const {
  return state.multisignatureOutputGlobalIndexesForAmounts;
}

const std::unordered_map<uint32_t, std::vector<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>>>& BlockchainReadResult::getSpentMultisignatureOutputGlobalIndexesByBlocks() const {
  return state.spentMultisignatureOutputGlobalIndexesByBlocks;
}

const std::unordered_map<std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>, bool>& BlockchainReadResult::getMultisignatureOutputsSpendingStatuses() const {
  return state.multisignatureOutputsSpendingStatuses;
}

const std::unordered_map<uint32_t, RawBlock>& BlockchainReadResult::getRawBlocks() const {
  return state.rawBlocks;
}

const std::pair<uint32_t, bool>& BlockchainReadResult::getLastBlockIndex() const {
  return state.lastBlockIndex;
}

const std::unordered_map<uint64_t, uint32_t>& BlockchainReadResult::getClosestTimestampBlockIndex() const {
  return state.closestTimestampBlockIndex;
}

uint32_t BlockchainReadResult::getKeyOutputAmountsCount() const {
  return state.keyOutputAmountsCount.first;
}

uint32_t BlockchainReadResult::getMultisignatureOutputAmountsCount() const {
  return state.multisignatureOutputAmountsCount.first;
}

const std::unordered_map<uint32_t, IBlockchainCache::Amount>& BlockchainReadResult::getKeyOutputAmounts() const {
  return state.keyOutputAmounts;
}

const std::unordered_map<uint32_t, IBlockchainCache::Amount>& BlockchainReadResult::getMultisignatureOutputAmounts() const {
  return state.multisignatureOutputAmounts;
}

const std::unordered_map<Crypto::Hash, uint32_t>& BlockchainReadResult::getTransactionCountByPaymentIds() const {
  return state.transactionCountsByPaymentIds;
}

const std::unordered_map<std::pair<Crypto::Hash, uint32_t>, Crypto::Hash>& BlockchainReadResult::getTransactionHashesByPaymentIds() const {
  return state.transactionHashesByPaymentIds;
}

const std::unordered_map<uint64_t, std::vector<Crypto::Hash>>& BlockchainReadResult::getBlockHashesByTimestamp() const {
  return state.blockHashesByTimestamp;
}

const std::pair<uint64_t, bool>& BlockchainReadResult::getTransactionsCount() const {
  return state.transactionsCount;
}

const KeyOutputKeyResult& BlockchainReadResult::getKeyOutputInfo() const {
  return state.keyOutputKeys;
}

void BlockchainReadBatch::submitRawResult(const std::vector<std::string>& values, const std::vector<bool>& resultStates) {
  assert(state.size() == values.size());
  assert(values.size() == resultStates.size());
  auto range = boost::combine(values, resultStates);
  auto iter = range.begin();

  DB::deserializeValues(state.spentKeyImagesByBlock, iter, DB::BLOCK_INDEX_TO_KEY_IMAGE_PREFIX);
  DB::deserializeValues(state.blockIndexesBySpentKeyImages, iter, DB::KEY_IMAGE_TO_BLOCK_INDEX_PREFIX);
  DB::deserializeValues(state.cachedTransactions, iter, DB::TRANSACTION_HASH_TO_TRANSACTION_INFO_PREFIX);
  DB::deserializeValues(state.transactionHashesByBlocks, iter, DB::BLOCK_INDEX_TO_TX_HASHES_PREFIX);
  DB::deserializeValues(state.cachedBlocks, iter, DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX);
  DB::deserializeValues(state.blockIndexesByBlockHashes, iter, DB::BLOCK_HASH_TO_BLOCK_INDEX_PREFIX);
  DB::deserializeValues(state.keyOutputGlobalIndexesCountForAmounts, iter, DB::KEY_OUTPUT_AMOUNT_PREFIX);
  DB::deserializeValues(state.keyOutputGlobalIndexesForAmounts, iter, DB::KEY_OUTPUT_AMOUNT_PREFIX);
  DB::deserializeValues(state.multisignatureOutputGlobalIndexesCountForAmounts, iter, DB::MULTISIGNATURE_OUTPUT_AMOUNT_PREFIX);
  DB::deserializeValues(state.multisignatureOutputGlobalIndexesForAmounts, iter, DB::MULTISIGNATURE_OUTPUT_AMOUNT_PREFIX);
  DB::deserializeValues(state.spentMultisignatureOutputGlobalIndexesByBlocks, iter, DB::BLOCK_INDEX_TO_SPENT_MULTISIGNATURE_PREFIX);
  DB::deserializeValues(state.multisignatureOutputsSpendingStatuses, iter, DB::SPENT_MULTISIGNATURE_OUTPUT_AMOUNT_PREFIX);
  DB::deserializeValues(state.rawBlocks, iter, DB::BLOCK_INDEX_TO_RAW_BLOCK_PREFIX);
  DB::deserializeValues(state.closestTimestampBlockIndex, iter, DB::CLOSEST_TIMESTAMP_BLOCK_INDEX_PREFIX);
  DB::deserializeValues(state.keyOutputAmounts, iter, DB::KEY_OUTPUT_AMOUNTS_COUNT_PREFIX);
  DB::deserializeValues(state.multisignatureOutputAmounts, iter, DB::MULTISIGNATURE_OUTPUT_AMOUNTS_COUNT_PREFIX);
  DB::deserializeValues(state.transactionCountsByPaymentIds, iter, DB::PAYMENT_ID_TO_TX_HASH_PREFIX);
  DB::deserializeValues(state.transactionHashesByPaymentIds, iter, DB::PAYMENT_ID_TO_TX_HASH_PREFIX);
  DB::deserializeValues(state.blockHashesByTimestamp, iter, DB::TIMESTAMP_TO_BLOCKHASHES_PREFIX);
  DB::deserializeValues(state.keyOutputKeys, iter, DB::KEY_OUTPUT_KEY_PREFIX);

  DB::deserializeValue(state.lastBlockIndex, iter, DB::BLOCK_INDEX_TO_BLOCK_HASH_PREFIX);
  DB::deserializeValue(state.keyOutputAmountsCount, iter, DB::KEY_OUTPUT_AMOUNTS_COUNT_PREFIX);
  DB::deserializeValue(state.multisignatureOutputAmountsCount, iter, DB::MULTISIGNATURE_OUTPUT_AMOUNTS_COUNT_PREFIX);
  DB::deserializeValue(state.transactionsCount, iter, DB::TRANSACTION_HASH_TO_TRANSACTION_INFO_PREFIX);

  assert(iter == range.end());
  
  resultSubmitted = true;
}

BlockchainReadState::BlockchainReadState(BlockchainReadState&& state) :
spentKeyImagesByBlock(std::move(state.spentKeyImagesByBlock)),
blockIndexesBySpentKeyImages(std::move(state.blockIndexesBySpentKeyImages)),
cachedTransactions(std::move(state.cachedTransactions)),
transactionHashesByBlocks(std::move(state.transactionHashesByBlocks)),
cachedBlocks(std::move(state.cachedBlocks)),
blockIndexesByBlockHashes(std::move(state.blockIndexesByBlockHashes)),
keyOutputGlobalIndexesCountForAmounts(std::move(state.keyOutputGlobalIndexesCountForAmounts)),
keyOutputGlobalIndexesForAmounts(std::move(state.keyOutputGlobalIndexesForAmounts)),
multisignatureOutputGlobalIndexesCountForAmounts(std::move(state.multisignatureOutputGlobalIndexesCountForAmounts)),
multisignatureOutputGlobalIndexesForAmounts(std::move(state.multisignatureOutputGlobalIndexesForAmounts)),
spentMultisignatureOutputGlobalIndexesByBlocks(std::move(state.spentMultisignatureOutputGlobalIndexesByBlocks)),
multisignatureOutputsSpendingStatuses(std::move(state.multisignatureOutputsSpendingStatuses)),
rawBlocks(std::move(state.rawBlocks)),
blockHashesByTimestamp(std::move(state.blockHashesByTimestamp)),
keyOutputKeys(std::move(state.keyOutputKeys)),
closestTimestampBlockIndex(std::move(state.closestTimestampBlockIndex)),
lastBlockIndex(std::move(state.lastBlockIndex)),
keyOutputAmountsCount(std::move(state.keyOutputAmountsCount)),
multisignatureOutputAmountsCount(std::move(state.multisignatureOutputAmountsCount)),
keyOutputAmounts(std::move(state.keyOutputAmounts)),
multisignatureOutputAmounts(std::move(state.multisignatureOutputAmounts)),
transactionCountsByPaymentIds(std::move(state.transactionCountsByPaymentIds)),
transactionHashesByPaymentIds(std::move(state.transactionHashesByPaymentIds)),
transactionsCount(std::move(state.transactionsCount)) {
}

size_t BlockchainReadState::size() const {
  return spentKeyImagesByBlock.size() +
    blockIndexesBySpentKeyImages.size() +
    cachedTransactions.size() +
    transactionHashesByBlocks.size() +
    cachedBlocks.size() +
    blockIndexesByBlockHashes.size() +
    keyOutputGlobalIndexesCountForAmounts.size() +
    keyOutputGlobalIndexesForAmounts.size() +
    multisignatureOutputGlobalIndexesCountForAmounts.size() +
    multisignatureOutputGlobalIndexesForAmounts.size() +
    spentMultisignatureOutputGlobalIndexesByBlocks.size() +
    multisignatureOutputsSpendingStatuses.size() +
    rawBlocks.size() +
    closestTimestampBlockIndex.size() +
    keyOutputAmounts.size() +
    multisignatureOutputAmounts.size() +
    transactionCountsByPaymentIds.size() +
    transactionHashesByPaymentIds.size() +
    blockHashesByTimestamp.size() +
    keyOutputKeys.size() +
    (lastBlockIndex.second ? 1 : 0) +
    (keyOutputAmountsCount.second ? 1 : 0) +
    (multisignatureOutputAmountsCount.second ? 1 : 0) +
    (transactionsCount.second ? 1 : 0);
}

BlockchainReadResult::BlockchainReadResult(BlockchainReadResult&& result) : state(std::move(result.state)) {
}
