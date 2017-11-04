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

#pragma once

#include <vector>
#include <unordered_map>

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/Currency.h"
#include "crypto/hash.h"

#include "../TestGenerator/TestGenerator.h"

class TestBlockchainGenerator
{
public:
  TestBlockchainGenerator(const CryptoNote::Currency& currency);

  //TODO: get rid of this method
  std::vector<CryptoNote::BlockTemplate>& getBlockchain();
  std::vector<CryptoNote::BlockTemplate> getBlockchainCopy();
  void generateEmptyBlocks(size_t count);
  bool getBlockRewardForAddress(const CryptoNote::AccountPublicAddress& address);
  bool generateTransactionsInOneBlock(const CryptoNote::AccountPublicAddress& address, size_t n);
  bool getSingleOutputTransaction(const CryptoNote::AccountPublicAddress& address, uint64_t amount);
  void addTxToBlockchain(const CryptoNote::Transaction& transaction);
  bool getTransactionByHash(const Crypto::Hash& hash, CryptoNote::Transaction& tx, bool checkTxPool = false);
  CryptoNote::Transaction getTransactionByHash(const Crypto::Hash& hash, bool checkTxPool = false);
  const CryptoNote::AccountBase& getMinerAccount() const;
  bool generateFromBaseTx(const CryptoNote::AccountBase& address);

  void putTxToPool(const CryptoNote::Transaction& tx);
  void getPoolSymmetricDifference(std::vector<Crypto::Hash>&& known_pool_tx_ids, Crypto::Hash known_block_id, bool& is_bc_actual,
    std::vector<CryptoNote::Transaction>& new_txs, std::vector<Crypto::Hash>& deleted_tx_ids);
  void putTxPoolToBlockchain();
  void clearTxPool();

  void cutBlockchain(uint32_t height);

  bool getTransactionGlobalIndexesByHash(const Crypto::Hash& transactionHash, std::vector<uint32_t>& globalIndexes);
  bool getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t globalIndex, CryptoNote::MultisignatureOutput& out);
  size_t getGeneratedTransactionsNumber(uint32_t index);
  void setMinerAccount(const CryptoNote::AccountBase& account);

private:
  struct MultisignatureOutEntry {
    Crypto::Hash transactionHash;
    uint16_t indexOut;
  };

  struct KeyOutEntry {
    Crypto::Hash transactionHash;
    uint16_t indexOut;
  };
  
  void addGenesisBlock();
  void addMiningBlock();

  const CryptoNote::Currency& m_currency;
  test_generator generator;
  CryptoNote::AccountBase miner_acc;
  std::vector<CryptoNote::BlockTemplate> m_blockchain;
  std::unordered_map<Crypto::Hash, CryptoNote::Transaction> m_txs;
  std::unordered_map<Crypto::Hash, std::vector<uint32_t>> transactionGlobalOuts;
  std::unordered_map<uint64_t, std::vector<MultisignatureOutEntry>> multisignatureOutsIndex;
  std::unordered_map<uint64_t, std::vector<KeyOutEntry>> keyOutsIndex;

  std::unordered_map<Crypto::Hash, CryptoNote::Transaction> m_txPool;
  mutable std::mutex m_mutex;

  void addToBlockchain(const CryptoNote::Transaction& tx);
  void addToBlockchain(const std::vector<CryptoNote::Transaction>& txs);
  void addToBlockchain(const std::vector<CryptoNote::Transaction>& txs, const CryptoNote::AccountBase& minerAddress);
  void addTx(const CryptoNote::Transaction& tx);

  bool doGenerateTransactionsInOneBlock(CryptoNote::AccountPublicAddress const &address, size_t n);
};
