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

#include "TestBlockchainGenerator.h"

#include <numeric>
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
  generator(currency) {
  std::unique_lock<std::mutex> lock(m_mutex);

  miner_acc.generate();
  addGenesisBlock();
  addMiningBlock();
}

std::vector<CryptoNote::BlockTemplate>& TestBlockchainGenerator::getBlockchain()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  return m_blockchain;
}

std::vector<CryptoNote::BlockTemplate> TestBlockchainGenerator::getBlockchainCopy() {
  std::unique_lock<std::mutex> lock(m_mutex);

  std::vector<CryptoNote::BlockTemplate> blockchain(m_blockchain);
  return blockchain;
}

CryptoNote::Transaction TestBlockchainGenerator::getTransactionByHash(const Crypto::Hash& hash, bool checkTxPool)
{
  Transaction tx;
  if (!getTransactionByHash(hash, tx, checkTxPool)) {
    throw std::runtime_error("no transaction for hash found");
  }
  return tx;
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
  CryptoNote::CachedBlock genesisBlock(m_currency.genesisBlock());
  generator.addBlock(genesisBlock, 0, 0, bsizes, 0);

  m_blockchain.push_back(m_currency.genesisBlock());
  addTx(m_currency.genesisBlock().baseTransaction);
}

void TestBlockchainGenerator::addMiningBlock() {
  CryptoNote::BlockTemplate block;

  uint64_t timestamp = time(NULL);
  CryptoNote::BlockTemplate& prev_block = m_blockchain.back();
  uint32_t height = boost::get<BaseInput>(prev_block.baseTransaction.inputs.front()).blockIndex + 1;
  Crypto::Hash prev_id = CryptoNote::CachedBlock(prev_block).getBlockHash();

  std::vector<size_t> block_sizes;
  std::list<CryptoNote::Transaction> tx_list;

  generator.constructBlock(block, height, prev_id, miner_acc, timestamp, 0, block_sizes, tx_list);
  m_blockchain.push_back(block);
  addTx(block.baseTransaction);
}

void TestBlockchainGenerator::generateEmptyBlocks(size_t count)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  for (size_t i = 0; i < count; ++i)
  {
    CryptoNote::BlockTemplate& prev_block = m_blockchain.back();
    CryptoNote::BlockTemplate block;
    generator.constructBlock(block, prev_block, miner_acc);
    m_blockchain.push_back(block);
    addTx(block.baseTransaction);
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

size_t TestBlockchainGenerator::getGeneratedTransactionsNumber(uint32_t index) {
  auto top = std::min(size_t(index + 1), m_blockchain.size());
  return std::accumulate(
      std::begin(m_blockchain), std::next(std::begin(m_blockchain), top), size_t(0),
      [](size_t sum, const CryptoNote::BlockTemplate& block) { return sum + block.transactionHashes.size() + 1; });
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
  }

  CryptoNote::BlockTemplate block;
  generator.constructBlock(block, m_blockchain.back(), minerAddress, txsToBlock);
  m_blockchain.push_back(block);
  addTx(block.baseTransaction);
}

void TestBlockchainGenerator::getPoolSymmetricDifference(std::vector<Crypto::Hash>&& known_pool_tx_ids, Crypto::Hash known_block_id, bool& is_bc_actual,
  std::vector<CryptoNote::Transaction>& new_txs, std::vector<Crypto::Hash>& deleted_tx_ids)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  if (known_block_id != CryptoNote::CachedBlock(m_blockchain.back()).getBlockHash()) {
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

void TestBlockchainGenerator::setMinerAccount(const CryptoNote::AccountBase& account) {
  miner_acc = account;
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
