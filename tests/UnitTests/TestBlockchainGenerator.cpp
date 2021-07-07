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

#include "TestBlockchainGenerator.h"

#include <time.h>
#include <unordered_set>

#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"


#include "../PerformanceTests/MultiTransactionTestBase.h"

using namespace CryptoNote;

class TransactionForAddressCreator : public multi_tx_test_base<5>
{
  typedef multi_tx_test_base<5> base_class;
public:
  TransactionForAddressCreator() {}

  bool init()
  {
    return base_class::init();
  }

  void generate(const AccountPublicAddress& address, Transaction& tx, uint64_t unlockTime = 0)
  {
    std::vector<CryptoNote::TransactionDestinationEntry> destinations;

    CryptoNote::decompose_amount_into_digits(this->m_source_amount, 0,
      [&](uint64_t chunk) { destinations.push_back(CryptoNote::TransactionDestinationEntry(chunk, address)); },
      [&](uint64_t a_dust) { destinations.push_back(CryptoNote::TransactionDestinationEntry(a_dust, address)); });

    CryptoNote::constructTransaction(this->m_miners[this->real_source_idx].getAccountKeys(), this->m_sources, destinations, std::vector<uint8_t>(), tx, unlockTime, m_logger);
  }

  void generateSingleOutputTx(const AccountPublicAddress& address, uint64_t amount, Transaction& tx) {
    std::vector<TransactionDestinationEntry> destinations;
    destinations.push_back(TransactionDestinationEntry(amount, address));
    constructTransaction(this->m_miners[this->real_source_idx].getAccountKeys(), this->m_sources, destinations, std::vector<uint8_t>(), tx, 0, m_logger);
  }
};

TestBlockchainGenerator::TestBlockchainGenerator(const CryptoNote::Currency& currency) :
  m_currency(currency),
  generator(currency),
  m_paymentIdIndex(true),
  m_timestampIndex(true),
  m_generatedTransactionsIndex(true),
  m_orthanBlocksIndex(true) {
  std::unique_lock<std::mutex> lock(m_mutex);

  miner_acc.generate();
  addGenesisBlock();
  addMiningBlock();
}

std::vector<CryptoNote::Block>& TestBlockchainGenerator::getBlockchain()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  return m_blockchain;
}

std::vector<CryptoNote::Block> TestBlockchainGenerator::getBlockchainCopy() {
  std::unique_lock<std::mutex> lock(m_mutex);

  std::vector<CryptoNote::Block> blockchain(m_blockchain);
  return blockchain;
}

bool TestBlockchainGenerator::getTransactionByHash(const Crypto::Hash& hash, CryptoNote::Transaction& tx, bool checkTxPool)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  auto it = m_txs.find(hash);
  if (it != m_txs.end()) {
    tx = it->second;
    return true;
  } else if (checkTxPool) {
    auto poolIt = m_txPool.find(hash);
    if (poolIt != m_txPool.end()) {
      tx = poolIt->second;
      return true;
    }
  }
  return false;
}

const CryptoNote::AccountBase& TestBlockchainGenerator::getMinerAccount() const {
  std::unique_lock<std::mutex> lock(m_mutex);
  return miner_acc;
}

void TestBlockchainGenerator::addGenesisBlock() {
  std::vector<size_t> bsizes;
  generator.addBlock(m_currency.genesisBlock(), 0, 0, bsizes, 0);

  m_blockchain.push_back(m_currency.genesisBlock());
  addTx(m_currency.genesisBlock().baseTransaction);

  m_timestampIndex.add(m_currency.genesisBlock().timestamp, CryptoNote::get_block_hash(m_currency.genesisBlock()));
  m_generatedTransactionsIndex.add(m_currency.genesisBlock());
}

void TestBlockchainGenerator::addMiningBlock() {
  CryptoNote::Block block;

  uint64_t timestamp = time(NULL);
  CryptoNote::Block& prev_block = m_blockchain.back();
  uint32_t height = boost::get<BaseInput>(prev_block.baseTransaction.inputs.front()).blockIndex + 1;
  Crypto::Hash prev_id = get_block_hash(prev_block);

  std::vector<size_t> block_sizes;
  std::list<CryptoNote::Transaction> tx_list;

  generator.constructBlock(block, height, prev_id, miner_acc, timestamp, 0, block_sizes, tx_list);
  m_blockchain.push_back(block);
  addTx(block.baseTransaction);

  m_timestampIndex.add(block.timestamp, CryptoNote::get_block_hash(block));
  m_generatedTransactionsIndex.add(block);
}

void TestBlockchainGenerator::generateEmptyBlocks(size_t count)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  for (size_t i = 0; i < count; ++i)
  {
    CryptoNote::Block& prev_block = m_blockchain.back();
    CryptoNote::Block block;
    generator.constructBlock(block, prev_block, miner_acc);
    m_blockchain.push_back(block);
    addTx(block.baseTransaction);

    m_timestampIndex.add(block.timestamp, CryptoNote::get_block_hash(block));
    m_generatedTransactionsIndex.add(block);
  }
}

void TestBlockchainGenerator::addTxToBlockchain(const CryptoNote::Transaction& transaction)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  addToBlockchain(transaction);
}

bool TestBlockchainGenerator::getBlockRewardForAddress(const CryptoNote::AccountPublicAddress& address)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  doGenerateTransactionsInOneBlock(address, 1);
  return true;
}

bool TestBlockchainGenerator::generateTransactionsInOneBlock(const CryptoNote::AccountPublicAddress& address, size_t n) {
  std::unique_lock<std::mutex> lock(m_mutex);

  return doGenerateTransactionsInOneBlock(address, n);
}

bool TestBlockchainGenerator::doGenerateTransactionsInOneBlock(const AccountPublicAddress &address, size_t n) {
  assert(n > 0);

  TransactionForAddressCreator creator;
  if (!creator.init())
    return false;

  std::vector<Transaction> txs;
  for (size_t i = 0; i < n; ++i) {
    Transaction tx;
    creator.generate(address, tx, m_blockchain.size() + 10);
    txs.push_back(tx);
  }

  addToBlockchain(txs);

  return true;
}

bool TestBlockchainGenerator::getSingleOutputTransaction(const CryptoNote::AccountPublicAddress& address, uint64_t amount) {
  std::unique_lock<std::mutex> lock(m_mutex);

  TransactionForAddressCreator creator;
  if (!creator.init())
    return false;

  CryptoNote::Transaction tx;
  creator.generateSingleOutputTx(address, amount, tx);

  addToBlockchain(tx);

  return true;
}

void TestBlockchainGenerator::addToBlockchain(const CryptoNote::Transaction& tx) {
  addToBlockchain(std::vector<CryptoNote::Transaction> {tx});
}

void TestBlockchainGenerator::addToBlockchain(const std::vector<CryptoNote::Transaction>& txs) {
  addToBlockchain(txs, miner_acc);
}

void TestBlockchainGenerator::addToBlockchain(const std::vector<CryptoNote::Transaction>& txs, const CryptoNote::AccountBase& minerAddress) {
  std::list<CryptoNote::Transaction> txsToBlock;

  for (const auto& tx: txs) {
    addTx(tx);

    txsToBlock.push_back(tx);
    m_paymentIdIndex.add(tx);
  }

  CryptoNote::Block& prev_block = m_blockchain.back();
  CryptoNote::Block block;

  generator.constructBlock(block, prev_block, minerAddress, txsToBlock);
  m_blockchain.push_back(block);
  addTx(block.baseTransaction);

  m_timestampIndex.add(block.timestamp, CryptoNote::get_block_hash(block));
  m_generatedTransactionsIndex.add(block);
}

void TestBlockchainGenerator::getPoolSymmetricDifference(std::vector<Crypto::Hash>&& known_pool_tx_ids, Crypto::Hash known_block_id, bool& is_bc_actual,
  std::vector<CryptoNote::Transaction>& new_txs, std::vector<Crypto::Hash>& deleted_tx_ids)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  if (known_block_id != CryptoNote::get_block_hash(m_blockchain.back())) {
    is_bc_actual = false;
    return;
  }

  is_bc_actual = true;

  std::unordered_set<Crypto::Hash> txIds;
  for (const auto& kv : m_txPool) {
    txIds.insert(kv.first);
  }

  std::unordered_set<Crypto::Hash> known_set(known_pool_tx_ids.begin(), known_pool_tx_ids.end());
  for (auto it = txIds.begin(), e = txIds.end(); it != e;) {
    auto known_it = known_set.find(*it);
    if (known_it != known_set.end()) {
      known_set.erase(known_it);
      it = txIds.erase(it);
    }
    else {
      new_txs.push_back(m_txPool[*it]);
      ++it;
    }
  }

  deleted_tx_ids.assign(known_set.begin(), known_set.end());
}

void TestBlockchainGenerator::putTxToPool(const CryptoNote::Transaction& tx) {
  std::unique_lock<std::mutex> lock(m_mutex);

  Crypto::Hash txHash = CryptoNote::getObjectHash(tx);
  m_txPool[txHash] = tx;
}

void TestBlockchainGenerator::putTxPoolToBlockchain() {
  std::unique_lock<std::mutex> lock(m_mutex);
  std::vector<CryptoNote::Transaction> txs;
  for (auto& kv : m_txPool) {
    txs.push_back(kv.second);
  }

  addToBlockchain(txs);
  m_txPool.clear();
}

void TestBlockchainGenerator::clearTxPool() {
  std::unique_lock<std::mutex> lock(m_mutex);

  m_txPool.clear();
}

void TestBlockchainGenerator::cutBlockchain(uint32_t height) {
  std::unique_lock<std::mutex> lock(m_mutex);

  assert(height < m_blockchain.size());
  //assert(height > m_lastHeight);

  auto it = m_blockchain.begin();
  std::advance(it, height);

  m_blockchain.erase(it, m_blockchain.end());

  //TODO: delete transactions from m_txs
}

bool TestBlockchainGenerator::addOrphan(const Crypto::Hash& hash, uint32_t height) {
  CryptoNote::Block block;
  uint64_t timestamp = time(NULL);
  generator.constructBlock(block, miner_acc, timestamp);
  return m_orthanBlocksIndex.add(block);
}

void TestBlockchainGenerator::setMinerAccount(const CryptoNote::AccountBase& account) {
  miner_acc = account;
}

bool TestBlockchainGenerator::getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions) {
  return m_generatedTransactionsIndex.find(height, generatedTransactions);
}

bool TestBlockchainGenerator::getOrphanBlockIdsByHeight(uint32_t height, std::vector<Crypto::Hash>& blockHashes) {
  return m_orthanBlocksIndex.find(height, blockHashes);
}

bool TestBlockchainGenerator::getBlockIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<Crypto::Hash>& hashes, uint32_t& blocksNumberWithinTimestamps) {
  uint64_t blockCount;
  if (!m_timestampIndex.find(timestampBegin, timestampEnd, blocksNumberLimit, hashes, blockCount)) {
    return false;
  }

  blocksNumberWithinTimestamps = static_cast<uint32_t>(blockCount);
  return true;
}

bool TestBlockchainGenerator::getPoolTransactionIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<Crypto::Hash>& hashes, uint64_t& transactionsNumberWithinTimestamps) {
  std::vector<Crypto::Hash> blockHashes;
  if (!m_timestampIndex.find(timestampBegin, timestampEnd, transactionsNumberLimit, blockHashes, transactionsNumberWithinTimestamps)) {
    return false;
  }
  transactionsNumberWithinTimestamps = m_txPool.size();
  uint32_t c = 0;
  for (auto i : m_txPool) {
    if (c >= transactionsNumberLimit) {
      return true;
    }
    hashes.push_back(CryptoNote::getObjectHash(i.second));
    ++c;
  }
  return true;
}

bool TestBlockchainGenerator::getTransactionIdsByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes) {
  return m_paymentIdIndex.find(paymentId, transactionHashes);
}

void TestBlockchainGenerator::addTx(const CryptoNote::Transaction& tx) {
  Crypto::Hash txHash = getObjectHash(tx);
  m_txs[txHash] = tx;
  auto& globalIndexes = transactionGlobalOuts[txHash];
  for (uint16_t outIndex = 0; outIndex < tx.outputs.size(); ++outIndex) {
    const auto& out = tx.outputs[outIndex];
    if (out.target.type() == typeid(KeyOutput)) {
      auto& keyOutsContainer = keyOutsIndex[out.amount];
      globalIndexes.push_back(static_cast<uint32_t>(keyOutsContainer.size()));
      keyOutsContainer.push_back({ txHash, outIndex });
    } else if (out.target.type() == typeid(MultisignatureOutput)) {
      auto& msigOutsContainer = multisignatureOutsIndex[out.amount];
      globalIndexes.push_back(static_cast<uint32_t>(msigOutsContainer.size()));
      msigOutsContainer.push_back({ txHash, outIndex });
    }
  }
}

bool TestBlockchainGenerator::getTransactionGlobalIndexesByHash(const Crypto::Hash& transactionHash, std::vector<uint32_t>& globalIndexes) {
  auto globalIndexesIt = transactionGlobalOuts.find(transactionHash);
  if (globalIndexesIt == transactionGlobalOuts.end()) {
    return false;
  }

  globalIndexes = globalIndexesIt->second;
  return true;
}

bool TestBlockchainGenerator::getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t globalIndex, MultisignatureOutput& out) {
  auto it = multisignatureOutsIndex.find(amount);
  if (it == multisignatureOutsIndex.end()) {
    return false;
  }

  if (it->second.size() <= globalIndex) {
    return false;
  }

  MultisignatureOutEntry entry = it->second[globalIndex];
  const auto& tx = m_txs[entry.transactionHash];
  assert(tx.outputs.size() > entry.indexOut);
  assert(tx.outputs[entry.indexOut].target.type() == typeid(MultisignatureOutput));
  out = boost::get<MultisignatureOutput>(tx.outputs[entry.indexOut].target);
  return true;
}

bool TestBlockchainGenerator::generateFromBaseTx(const CryptoNote::AccountBase& address) {
  std::unique_lock<std::mutex> lock(m_mutex);
  addToBlockchain({}, address);
  return true;
}