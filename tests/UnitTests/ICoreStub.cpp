// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include "ICoreStub.h"

#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/IBlock.h"
#include "CryptoNoteCore/VerificationContext.h"


ICoreStub::ICoreStub() :
    topHeight(0),
    globalIndicesResult(false),
    randomOutsResult(false),
    poolTxVerificationResult(true),
    poolChangesResult(true) {
}

ICoreStub::ICoreStub(const CryptoNote::Block& genesisBlock) :
    topHeight(0),
    globalIndicesResult(false),
    randomOutsResult(false),
    poolTxVerificationResult(true),
    poolChangesResult(true) {
  addBlock(genesisBlock);
}

bool ICoreStub::addObserver(CryptoNote::ICoreObserver* observer) {
  return m_observerManager.add(observer);
}

bool ICoreStub::removeObserver(CryptoNote::ICoreObserver* observer) {
  return m_observerManager.remove(observer);
}

void ICoreStub::get_blockchain_top(uint32_t& height, Crypto::Hash& top_id) {
  height = topHeight;
  top_id = topId;
}

std::vector<Crypto::Hash> ICoreStub::findBlockchainSupplement(const std::vector<Crypto::Hash>& remoteBlockIds, size_t maxCount,
  uint32_t& totalBlockCount, uint32_t& startBlockIndex) {

  //Sending all blockchain
  totalBlockCount = static_cast<uint32_t>(blocks.size());
  startBlockIndex = 0;
  std::vector<Crypto::Hash> result;
  result.reserve(std::min(blocks.size(), maxCount));
  for (uint32_t height = 0; height < static_cast<uint32_t>(std::min(blocks.size(), maxCount)); ++height) {
    assert(blockHashByHeightIndex.count(height) > 0);
    result.push_back(blockHashByHeightIndex[height]);
  }
  return result;
}

bool ICoreStub::get_random_outs_for_amounts(const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req,
    CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res) {
  res = randomOuts;
  return randomOutsResult;
}

bool ICoreStub::get_tx_outputs_gindexs(const Crypto::Hash& tx_id, std::vector<uint32_t>& indexs) {
  std::copy(globalIndices.begin(), globalIndices.end(), std::back_inserter(indexs));
  return globalIndicesResult;
}

CryptoNote::i_cryptonote_protocol* ICoreStub::get_protocol() {
  return nullptr;
}

bool ICoreStub::handle_incoming_tx(CryptoNote::BinaryArray const& tx_blob, CryptoNote::tx_verification_context& tvc, bool keeped_by_block) {
  return true;
}

void ICoreStub::set_blockchain_top(uint32_t height, const Crypto::Hash& top_id) {
  topHeight = height;
  topId = top_id;
  m_observerManager.notify(&CryptoNote::ICoreObserver::blockchainUpdated);
}

void ICoreStub::set_outputs_gindexs(const std::vector<uint32_t>& indexs, bool result) {
  globalIndices.clear();
  std::copy(indexs.begin(), indexs.end(), std::back_inserter(globalIndices));
  globalIndicesResult = result;
}

void ICoreStub::set_random_outs(const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp, bool result) {
  randomOuts = resp;
  randomOutsResult = result;
}

std::vector<CryptoNote::Transaction> ICoreStub::getPoolTransactions() {
  return std::vector<CryptoNote::Transaction>();
}

bool ICoreStub::getPoolChanges(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
                               std::vector<CryptoNote::Transaction>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) {
  std::unordered_set<Crypto::Hash> knownSet;
  for (const Crypto::Hash& txId : knownTxsIds) {
    if (transactionPool.find(txId) == transactionPool.end()) {
      deletedTxsIds.push_back(txId);
    }

    knownSet.insert(txId);
  }

  for (const std::pair<Crypto::Hash, CryptoNote::Transaction>& poolEntry : transactionPool) {
    if (knownSet.find(poolEntry.first) == knownSet.end()) {
      addedTxs.push_back(poolEntry.second);
    }
  }

  return poolChangesResult;
}

bool ICoreStub::getPoolChangesLite(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
        std::vector<CryptoNote::TransactionPrefixInfo>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) {
  std::vector<CryptoNote::Transaction> added;
  bool returnStatus = getPoolChanges(tailBlockId, knownTxsIds, added, deletedTxsIds);

  for (const auto& tx : added) {
    CryptoNote::TransactionPrefixInfo tpi;
    tpi.txPrefix = tx;
    tpi.txHash = getObjectHash(tx);

    addedTxs.push_back(std::move(tpi));
  }

  return returnStatus;
}

void ICoreStub::getPoolChanges(const std::vector<Crypto::Hash>& knownTxsIds, std::vector<CryptoNote::Transaction>& addedTxs,
                               std::vector<Crypto::Hash>& deletedTxsIds) {
}

bool ICoreStub::queryBlocks(const std::vector<Crypto::Hash>& block_ids, uint64_t timestamp,
  uint32_t& start_height, uint32_t& current_height, uint32_t& full_offset, std::vector<CryptoNote::BlockFullInfo>& entries) {
  //stub
  return true;
}

bool ICoreStub::queryBlocksLite(const std::vector<Crypto::Hash>& block_ids, uint64_t timestamp,
  uint32_t& start_height, uint32_t& current_height, uint32_t& full_offset, std::vector<CryptoNote::BlockShortInfo>& entries) {
  //stub
  return true;
}

std::vector<Crypto::Hash> ICoreStub::buildSparseChain() {
  std::vector<Crypto::Hash> result;
  result.reserve(blockHashByHeightIndex.size());
  for (auto kvPair : blockHashByHeightIndex) {
    result.emplace_back(kvPair.second);
  }

  std::reverse(result.begin(), result.end());
  return result;
}

std::vector<Crypto::Hash> ICoreStub::buildSparseChain(const Crypto::Hash& startBlockId) {
  // TODO implement
  assert(blocks.count(startBlockId) > 0);
  std::vector<Crypto::Hash> result;
  result.emplace_back(blockHashByHeightIndex[0]);
  return result;
}

size_t ICoreStub::addChain(const std::vector<const CryptoNote::IBlock*>& chain) {
  size_t blocksCounter = 0;
  for (const CryptoNote::IBlock* block : chain) {
    for (size_t txNumber = 0; txNumber < block->getTransactionCount(); ++txNumber) {
      const CryptoNote::Transaction& tx = block->getTransaction(txNumber);
      Crypto::Hash txHash = CryptoNote::NULL_HASH;
      size_t blobSize = 0;
      getObjectHash(tx, txHash, blobSize);
      addTransaction(tx);
    }
    addBlock(block->getBlock());
    ++blocksCounter;
  }

  return blocksCounter;
}

Crypto::Hash ICoreStub::getBlockIdByHeight(uint32_t height) {
  auto iter = blockHashByHeightIndex.find(height);
  if (iter == blockHashByHeightIndex.end()) {
    return CryptoNote::NULL_HASH;
  }
  return iter->second;
}

bool ICoreStub::getBlockByHash(const Crypto::Hash &h, CryptoNote::Block &blk) {
  auto iter = blocks.find(h);
  if (iter == blocks.end()) {
    return false;
  }
  blk = iter->second;
  return true;
}

bool ICoreStub::getBlockHeight(const Crypto::Hash& blockId, uint32_t& blockHeight) {
  auto it = blocks.find(blockId);
  if (it == blocks.end()) {
    return false;
  }
  blockHeight = get_block_height(it->second);
  return true;
}

void ICoreStub::getTransactions(const std::vector<Crypto::Hash>& txs_ids, std::list<CryptoNote::Transaction>& txs, std::list<Crypto::Hash>& missed_txs, bool checkTxPool) {
  for (const Crypto::Hash& hash : txs_ids) {
    auto iter = transactions.find(hash);
    if (iter != transactions.end()) {
      txs.push_back(iter->second);
    } else {
      missed_txs.push_back(hash);
    }
  }
  if (checkTxPool) {
    std::list<Crypto::Hash> pullTxIds(std::move(missed_txs));
    missed_txs.clear();
    for (const Crypto::Hash& hash : pullTxIds) {
      auto iter = transactionPool.find(hash);
      if (iter != transactionPool.end()) {
        txs.push_back(iter->second);
      }
      else {
        missed_txs.push_back(hash);
      }
    }
  }
}

bool ICoreStub::getBackwardBlocksSizes(uint32_t fromHeight, std::vector<size_t>& sizes, size_t count) {
  sizes.clear();
  if (blocks.empty()) {
    return true;
  }

  uint32_t height = fromHeight;
  while (count-- > 0) {
    auto hashIt = blockHashByHeightIndex.find(height);
    if (hashIt == blockHashByHeightIndex.end()) {
      break;
    }

    auto blockIt = blocks.find(hashIt->second);
    if (blockIt == blocks.end()) {
      break;
    }

    sizes.push_back(getObjectBinarySize(blockIt->second));
    if (height == 0) {
      break;
    }

    --height;
  }

  return true;
}

bool ICoreStub::getBlockSize(const Crypto::Hash& hash, size_t& size) {
  auto blockIt = blocks.find(hash);
  if (blockIt == blocks.end()) {
    return false;
  }

  size = getObjectBinarySize(blockIt->second);
  return true;
}

bool ICoreStub::getAlreadyGeneratedCoins(const Crypto::Hash& hash, uint64_t& generatedCoins) {
  uint32_t height = 0;
  if (!getBlockHeight(hash, height)) {
    return false;
  }

  uint64_t total = 0;
  for (uint32_t h = 0; h <= height; ++h) {
    auto hashIt = blockHashByHeightIndex.find(h);
    if (hashIt == blockHashByHeightIndex.end()) {
      return false;
    }

    auto blockIt = blocks.find(hashIt->second);
    if (blockIt == blocks.end()) {
      return false;
    }

    for (const auto& out : blockIt->second.baseTransaction.outputs) {
      total += out.amount;
    }
  }

  generatedCoins = total;
  return true;
}

bool ICoreStub::getBlockReward(uint8_t blockMajorVersion, size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee,
    uint64_t& reward, int64_t& emissionChange) {
  reward = 0;
  emissionChange = 0;
  return true;
}

bool ICoreStub::scanOutputkeysForIndices(const CryptoNote::KeyInput& txInToKey, std::list<std::pair<Crypto::Hash, size_t>>& outputReferences) {
  return true;
}

bool ICoreStub::getBlockDifficulty(uint32_t height, CryptoNote::difficulty_type& difficulty) {
  if (blockHashByHeightIndex.count(height) == 0) {
    return false;
  }

  difficulty = static_cast<CryptoNote::difficulty_type>(height + 1);
  return true;
}

bool ICoreStub::getBlockContainingTx(const Crypto::Hash& txId, Crypto::Hash& blockId, uint32_t& blockHeight) {
  auto iter = blockHashByTxHashIndex.find(txId);
  if (iter == blockHashByTxHashIndex.end()) {
    return false;
  }
  blockId = iter->second;
  auto blockIter = blocks.find(blockId);
  if (blockIter == blocks.end()) {
    return false;
  }
  blockHeight = boost::get<CryptoNote::BaseInput>(blockIter->second.baseTransaction.inputs.front()).blockIndex;
  return true;
}

void ICoreStub::addBlock(const CryptoNote::Block& block) {
  uint32_t height = boost::get<CryptoNote::BaseInput>(block.baseTransaction.inputs.front()).blockIndex;
  Crypto::Hash hash = CryptoNote::get_block_hash(block);
  if (height > topHeight || blocks.empty()) {
    topHeight = height;
    topId = hash;
  }
  blocks.emplace(std::make_pair(hash, block));
  blockHashByHeightIndex.emplace(std::make_pair(height, hash));

  blockHashByTxHashIndex.emplace(std::make_pair(CryptoNote::getObjectHash(block.baseTransaction), hash));
  for (auto txHash : block.transactionHashes) {
    blockHashByTxHashIndex.emplace(std::make_pair(txHash, hash));
  }

  m_observerManager.notify(&CryptoNote::ICoreObserver::blockchainUpdated);
}

void ICoreStub::addTransaction(const CryptoNote::Transaction& tx) {
  Crypto::Hash hash = CryptoNote::getObjectHash(tx);
  transactions.emplace(std::make_pair(hash, tx));
}

bool ICoreStub::getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions) {
  if (blocks.empty()) {
    generatedTransactions = 0;
    return true;
  }

  uint64_t total = 0;
  for (uint32_t h = 0; h <= height; ++h) {
    auto hashIt = blockHashByHeightIndex.find(h);
    if (hashIt == blockHashByHeightIndex.end()) {
      return false;
    }

    auto blockIt = blocks.find(hashIt->second);
    if (blockIt == blocks.end()) {
      return false;
    }

    total += static_cast<uint64_t>(blockIt->second.transactionHashes.size()) + 1;
  }

  generatedTransactions = total;
  return true;
}

bool ICoreStub::getOrphanBlocksByHeight(uint32_t height, std::vector<CryptoNote::Block>& blocks) {
  return true;
}

bool ICoreStub::getBlocksByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<CryptoNote::Block>& blocks, uint32_t& blocksNumberWithinTimestamps) {
  return true;
}

bool ICoreStub::getPoolTransactionsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<CryptoNote::Transaction>& transactions, uint64_t& transactionsNumberWithinTimestamps) {
  return true;
}

bool ICoreStub::getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<CryptoNote::Transaction>& transactions) {
  return true;
}

std::error_code ICoreStub::executeLocked(const std::function<std::error_code()>& func) {
  return func();
}

std::unique_ptr<CryptoNote::IBlock> ICoreStub::getBlock(const Crypto::Hash& blockId) {
  return std::unique_ptr<CryptoNote::IBlock>(nullptr);
}

bool ICoreStub::handleIncomingTransaction(const CryptoNote::Transaction& tx, const Crypto::Hash& txHash, size_t blobSize, CryptoNote::tx_verification_context& tvc, bool keptByBlock, uint32_t height) {
  auto result = transactionPool.emplace(std::make_pair(txHash, tx));
  tvc.m_verification_failed = !poolTxVerificationResult;
  tvc.m_added_to_pool = true;
  tvc.m_should_be_relayed = result.second;
  return poolTxVerificationResult;
}

bool ICoreStub::have_block(const Crypto::Hash& id) {
  return blocks.count(id) > 0;
}

void ICoreStub::setPoolTxVerificationResult(bool result) {
  poolTxVerificationResult = result;
}

bool ICoreStub::addMessageQueue(CryptoNote::MessageQueue<CryptoNote::BlockchainMessage>& messageQueuePtr) {
  return true;
}

bool ICoreStub::removeMessageQueue(CryptoNote::MessageQueue<CryptoNote::BlockchainMessage>& messageQueuePtr) {
  return true;
}

void ICoreStub::setPoolChangesResult(bool result) {
  poolChangesResult = result;
}

bool ICoreStub::haveTransaction(const Crypto::Hash& id) {
  return transactions.count(id) > 0 || transactionPool.count(id) > 0;
}

bool ICoreStub::handle_incoming_block(const CryptoNote::Block& b, CryptoNote::block_verification_context& bvc, bool control_miner, bool relay_block) {
  return false;
}

bool ICoreStub::getPoolTransaction(const Crypto::Hash& tx_hash, CryptoNote::Transaction& transaction) {
  auto it = transactionPool.find(tx_hash);
  if (it == transactionPool.end()) {
    return false;
  }

  transaction = it->second;
  return true;
}

bool ICoreStub::getTransactionHeight(const Crypto::Hash &txId, uint32_t& blockHeight) {
  auto iter = blockHashByTxHashIndex.find(txId);
  if (iter == blockHashByTxHashIndex.end()) {
    return false;
  }

  return getBlockHeight(iter->second, blockHeight);
}

bool ICoreStub::getTransactionsWithOutputGlobalIndexes(const std::vector<Crypto::Hash>& txs_ids, std::list<Crypto::Hash>& missed_txs, std::vector<std::pair<CryptoNote::Transaction, std::vector<uint32_t>>>& txs) {
  for (const auto& txId : txs_ids) {
    auto txIt = transactions.find(txId);
    if (txIt == transactions.end()) {
      missed_txs.push_back(txId);
      continue;
    }

    std::vector<uint32_t> outputIndexes;
    outputIndexes.reserve(txIt->second.outputs.size());
    for (size_t i = 0; i < txIt->second.outputs.size(); ++i) {
      outputIndexes.push_back(static_cast<uint32_t>(i));
    }

    txs.push_back(std::make_pair(txIt->second, std::move(outputIndexes)));
  }

  return true;
}

bool ICoreStub::getTransaction(const Crypto::Hash& id, CryptoNote::Transaction& tx, bool checkTxPool) {
  auto txIt = transactions.find(id);
  if (txIt != transactions.end()) {
    tx = txIt->second;
    return true;
  }

  if (!checkTxPool) {
    return false;
  }

  auto poolIt = transactionPool.find(id);
  if (poolIt == transactionPool.end()) {
    return false;
  }

  tx = poolIt->second;
  return true;
}

bool ICoreStub::getBlockCumulativeDifficulty(uint32_t height, CryptoNote::difficulty_type& difficulty) {
  if (blockHashByHeightIndex.count(height) == 0) {
    return false;
  }

  difficulty = static_cast<CryptoNote::difficulty_type>(height + 1);
  return true;
}

bool ICoreStub::getBlockTimestamp(uint32_t height, uint64_t& timestamp) {
  auto hashIt = blockHashByHeightIndex.find(height);
  if (hashIt == blockHashByHeightIndex.end()) {
    return false;
  }

  auto blockIt = blocks.find(hashIt->second);
  if (blockIt == blocks.end()) {
    return false;
  }

  timestamp = blockIt->second.timestamp;
  return true;
}

std::vector<Crypto::Hash> ICoreStub::getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) {
  return std::vector<Crypto::Hash>();
}

uint64_t ICoreStub::getMinimalFee(uint32_t height) {
  return 10000000000ULL;
}

uint64_t ICoreStub::getMinimalFee() {
  return 10000000000ULL;
}

uint64_t ICoreStub::getNextBlockDifficulty() {
  return 0;
}

uint64_t ICoreStub::getTotalGeneratedAmount() {
  if (blocks.empty()) {
    return 0;
  }

  uint64_t generatedCoins = 0;
  if (!getAlreadyGeneratedCoins(topId, generatedCoins)) {
    return 0;
  }

  return generatedCoins;
}

bool ICoreStub::check_tx_fee(const CryptoNote::Transaction& tx, const Crypto::Hash& txHash, size_t blobSize, CryptoNote::tx_verification_context& tvc, uint32_t height) {
  return true;
}

size_t ICoreStub::getPoolTransactionsCount() {
  return transactionPool.size();
}

size_t ICoreStub::getBlockchainTotalTransactions() {
  size_t total = 0;
  for (const auto& entry : blockHashByHeightIndex) {
    auto blockIt = blocks.find(entry.second);
    if (blockIt == blocks.end()) {
      continue;
    }

    total += blockIt->second.transactionHashes.size() + 1;
  }

  return total;
}

uint32_t ICoreStub::getCurrentBlockchainHeight() {
  return blocks.empty() ? 0 : topHeight + 1;
}

uint8_t ICoreStub::getBlockMajorVersionForHeight(uint32_t height) {
  return (uint8_t)4;
}

uint8_t ICoreStub::getCurrentBlockMajorVersion() {
  return (uint8_t)4;
}

size_t ICoreStub::getAlternativeBlocksCount() {
  return 0;
}

bool ICoreStub::getblockEntry(uint32_t height, uint64_t& block_cumulative_size, CryptoNote::difficulty_type& difficulty, uint64_t& already_generated_coins, uint64_t& reward, uint64_t& transactions_count, uint64_t& timestamp) {
  return false;
}

bool ICoreStub::getBlockStats(uint32_t startHeight, uint32_t endHeight, std::vector<CryptoNote::BlockStatsEntry>& stats) {
  return false;
}

bool ICoreStub::getBlockStats(const std::vector<uint32_t>& heights, std::vector<CryptoNote::BlockStatsEntry>& stats) {
  return false;
}

void ICoreStub::rollbackBlockchain(const uint32_t height) {
}

bool ICoreStub::getBlockLongHash(Crypto::cn_context &context, const CryptoNote::Block& b, Crypto::Hash& res) {
  res = CryptoNote::get_block_hash(b);
  return true;
}

bool ICoreStub::getMixin(const CryptoNote::Transaction& transaction, uint64_t& mixin) {
  mixin = 0;

  for (const auto& txIn : transaction.inputs) {
    if (txIn.type() != typeid(CryptoNote::KeyInput)) {
      continue;
    }

    const auto& keyInput = boost::get<CryptoNote::KeyInput>(txIn);
    mixin = std::max<uint64_t>(mixin, keyInput.outputIndexes.size());
  }

  return true;
}

bool ICoreStub::isInCheckpointZone(uint32_t height) const {
  return false;
}

bool ICoreStub::resolveAccountNumber(uint32_t blockHeight, uint32_t txIndex, CryptoNote::AccountPublicAddress& address) {
  return false;
}

bool ICoreStub::getAccountNumber(const CryptoNote::AccountPublicAddress& address, uint32_t& blockHeight, uint32_t& txIndex) {
  return false;
}

bool ICoreStub::getCanonicalAccountRegistrationsCount(uint64_t& count) {
  count = 0;
  return true;
}
