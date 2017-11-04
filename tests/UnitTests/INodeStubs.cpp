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

#include "INodeStubs.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "Wallet/WalletErrors.h"

#include <functional>
#include <thread>
#include <iterator>
#include <cassert>
#include <unordered_set>

#include <system_error>

#include "crypto/crypto.h"

#include "CryptoNoteCore/ICore.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteCore/TransactionApiExtra.h"

using namespace CryptoNote;
using namespace Common;

namespace {

class ContextCounterHolder {
public:
  ContextCounterHolder(WalletAsyncContextCounter& shutdowner) : m_shutdowner(shutdowner) {}
  ~ContextCounterHolder() { m_shutdowner.delAsyncContext(); }

private:
  WalletAsyncContextCounter& m_shutdowner;
};

}

TransactionDetails toDetails(Transaction tx, const Crypto::Hash& blockHash, uint32_t index) {
  TransactionDetails td;
  auto cachedTx = CachedTransaction(Transaction(tx));
  td.hash = cachedTx.getTransactionHash();
  td.fee = cachedTx.getTransactionFee();
  td.size = cachedTx.getTransactionBinaryArray().size();
  td.blockIndex = index;
  td.blockHash = blockHash;
  td.signatures = std::move(tx.signatures);
  td.timestamp = time(0);
  td.unlockTime = tx.unlockTime;
  td.extra.raw = tx.extra;
  TransactionExtra ext{tx.extra};
  TransactionExtraNonce nonce;
  if (ext.get(nonce)) {
    td.extra.nonce = std::move(nonce.nonce);
    if (getPaymentIdFromTransactionExtraNonce(td.extra.nonce, td.paymentId)) {
      td.hasPaymentId = true;
    }
  }

  std::transform(std::begin(tx.outputs), std::end(tx.outputs), std::back_inserter(td.outputs),
                 [&](const TransactionOutput& to) {
                   td.totalOutputsAmount += to.amount;
                   return TransactionOutputDetails{to, 0}; // TODO
                 });

  std::transform(std::begin(tx.inputs), std::end(tx.inputs), std::back_inserter(td.inputs),
                 [&](const TransactionInput& ti) {
                   TransactionInputDetails tid;
                   if (ti.type() == typeid(KeyInput)) {
                     auto ki = boost::get<KeyInput>(ti);
                     td.totalInputsAmount += ki.amount;
                     td.mixin = std::max(static_cast<size_t>(td.mixin), ki.outputIndexes.size());
                     KeyInputDetails kid;
                     kid.input = ki;
                     kid.mixin = ki.outputIndexes.size();
                     return TransactionInputDetails{kid};
                   } else if (ti.type() == typeid(BaseInput)) {
                     auto bi = boost::get<BaseInput>(ti);
                     return TransactionInputDetails{BaseInputDetails{bi, 0}}; // TODO
                   } else if (ti.type() == typeid(MultisignatureInput)) {
                     auto mi = boost::get<MultisignatureInput>(ti);
                     td.totalInputsAmount += mi.amount;
                     MultisignatureInputDetails det;
                     det.input = mi;
                     return TransactionInputDetails{det};
                   } else {
                     assert(false);
                     throw std::runtime_error("unknown type");
                   }
                   return TransactionInputDetails();
                 });
  return td;
}


void INodeDummyStub::updateObservers() {
  observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, getLastKnownBlockHeight());
}

bool INodeDummyStub::addObserver(INodeObserver* observer) {
  return observerManager.add(observer);
}

bool INodeDummyStub::removeObserver(INodeObserver* observer) {
  return observerManager.remove(observer);
}

void INodeTrivialRefreshStub::getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<RawBlock>& newBlocks, uint32_t& startHeight, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();

  std::unique_lock<std::mutex> lock(m_walletLock);
  auto blockchain = m_blockchainGenerator.getBlockchainCopy();
  lock.unlock();

  std::thread task(std::bind(&INodeTrivialRefreshStub::doGetNewBlocks, this, std::move(knownBlockIds), std::ref(newBlocks),
          std::ref(startHeight), std::move(blockchain), callback));
  task.detach();
}

void INodeTrivialRefreshStub::waitForAsyncContexts() {
  m_asyncCounter.waitAsyncContextsFinish();
}

void INodeTrivialRefreshStub::doGetNewBlocks(std::vector<Crypto::Hash> knownBlockIds, std::vector<RawBlock>& newBlocks,
        uint32_t& startHeight, std::vector<BlockTemplate> blockchain, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  auto start = blockchain.end();

  for (const auto& id : knownBlockIds) {
    start = std::find_if(blockchain.begin(), blockchain.end(), 
      [&id](BlockTemplate& block) { return CachedBlock(block).getBlockHash() == id; });
    if (start != blockchain.end())
      break;
  }

  if (start == blockchain.end()) {
    lock.unlock();
    callback(std::error_code());
    return;
  }

  m_lastHeight = static_cast<uint32_t>(std::distance(blockchain.begin(), start));
  startHeight = m_lastHeight; 

  for (; m_lastHeight < blockchain.size(); ++m_lastHeight)
  {
    RawBlock e;
    e.block = toBinaryArray(blockchain[m_lastHeight]);

    for (auto hash : blockchain[m_lastHeight].transactionHashes)
    {
      Transaction tx;
      if (!m_blockchainGenerator.getTransactionByHash(hash, tx))
        continue;

      e.transactions.push_back(toBinaryArray(tx));
    }

    newBlocks.push_back(e);

    if (newBlocks.size() >= m_getMaxBlocks) {
      break;
    }
  }

  m_lastHeight = startHeight + static_cast<uint32_t>(newBlocks.size());
  // m_lastHeight = startHeight + blockchain.size() - 1;

  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::unique_lock<std::mutex> lock(m_walletLock);
  calls_getTransactionOutsGlobalIndices.push_back(transactionHash);
  std::thread task(&INodeTrivialRefreshStub::doGetTransactionOutsGlobalIndices, this, transactionHash, std::ref(outsGlobalIndices), callback);
  task.detach();
}

void INodeTrivialRefreshStub::doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  bool success = m_blockchainGenerator.getTransactionGlobalIndexesByHash(transactionHash, outsGlobalIndices);

  lock.unlock();

  if (consumerTests) {
    outsGlobalIndices.clear();
    outsGlobalIndices.resize(20);
    getGlobalOutsFunctor(transactionHash, outsGlobalIndices);
    callback(std::error_code());
  } else {
    if (success) {
      callback(std::error_code());
    } else {
      callback(std::make_error_code(std::errc::invalid_argument));
    }
  }
}

void INodeTrivialRefreshStub::relayTransaction(const Transaction& transaction, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(&INodeTrivialRefreshStub::doRelayTransaction, this, transaction, callback);
  task.detach();
}

void INodeTrivialRefreshStub::doRelayTransaction(const Transaction& transaction, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  if (m_nextTxError)
  {
    m_nextTxError = false;
    lock.unlock();
    callback(make_error_code(error::INTERNAL_WALLET_ERROR));
    return;
  }

  if (m_nextTxToPool) {
    m_nextTxToPool = false;
    m_blockchainGenerator.putTxToPool(transaction);
    lock.unlock();
    callback(std::error_code());
    return;
  }

  m_blockchainGenerator.addTxToBlockchain(transaction);
  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint16_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(&INodeTrivialRefreshStub::doGetRandomOutsByAmounts, this, amounts, outsCount, std::ref(result), callback);
  task.detach();
}

void INodeTrivialRefreshStub::doGetRandomOutsByAmounts(std::vector<uint64_t> amounts, uint16_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  std::sort(amounts.begin(), amounts.end());

  for (uint64_t amount: amounts)
  {
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount out;
    out.amount = amount;

    uint64_t count = std::min(outsCount, m_maxMixin);

    for (uint32_t i = 0; i < count; ++i)
    {
      Crypto::PublicKey key;
      Crypto::SecretKey sk;
      generate_keys(key, sk);

      COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry e;
      e.global_amount_index = i;
      e.out_key = key;

      out.outs.push_back(e);
    }

    result.push_back(std::move(out));
  }

  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp,
        std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const Callback& callback) {
  auto resultHolder = std::make_shared<std::vector<RawBlock>>();

  getNewBlocks(std::move(knownBlockIds), *resultHolder, startHeight, [resultHolder, callback, &startHeight, &newBlocks](std::error_code ec)
  {
    if (ec) {
      callback(ec);
      return;
    }

    for (const auto& item : *resultHolder) {
      BlockShortEntry entry;

      if (!fromBinaryArray(entry.block, item.block)) {
        callback(std::make_error_code(std::errc::invalid_argument));
        return;
      }

      entry.hasBlock = true;
      entry.blockHash = CachedBlock(entry.block).getBlockHash();

      for (const auto& txBlob: item.transactions) {
        try {
          CachedTransaction cachedTransaction{txBlob};
          
          TransactionShortInfo tsi;
          tsi.txId = cachedTransaction.getTransactionHash();
          tsi.txPrefix = cachedTransaction.getTransaction();

          entry.txsShortInfo.push_back(std::move(tsi));
        } catch (std::exception&) {
          callback(std::make_error_code(std::errc::invalid_argument));
          return;
        }
      }

      newBlocks.push_back(std::move(entry));
    }

    callback(ec);
  });

}

void INodeTrivialRefreshStub::startAlternativeChain(uint32_t height)
{
  m_blockchainGenerator.cutBlockchain(height);
  m_lastHeight = height;
}

void INodeTrivialRefreshStub::setNextTransactionError()
{
  m_nextTxError = true;
}

void INodeTrivialRefreshStub::setNextTransactionToPool() {
  m_nextTxToPool = true;
}

void INodeTrivialRefreshStub::cleanTransactionPool() {
  m_blockchainGenerator.clearTxPool();
}

void INodeTrivialRefreshStub::getPoolSymmetricDifference(std::vector<Crypto::Hash>&& known_pool_tx_ids, Crypto::Hash known_block_id, bool& is_bc_actual,
        std::vector<std::unique_ptr<ITransactionReader>>& new_txs, std::vector<Crypto::Hash>& deleted_tx_ids, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(
    [this, known_pool_tx_ids, known_block_id, &is_bc_actual, &new_txs, &deleted_tx_ids, callback] () mutable {
      this->doGetPoolSymmetricDifference(std::move(known_pool_tx_ids), known_block_id, is_bc_actual, new_txs, deleted_tx_ids, callback);
    }
  );
  task.detach();
}

void INodeTrivialRefreshStub::doGetPoolSymmetricDifference(std::vector<Crypto::Hash>&& known_pool_tx_ids,
                                                           Crypto::Hash known_block_id, bool& is_bc_actual,
                                                           std::vector<std::unique_ptr<ITransactionReader>>& new_txs,
                                                           std::vector<Crypto::Hash>& deleted_tx_ids,
                                                           const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  std::vector<Transaction> txs;
  m_blockchainGenerator.getPoolSymmetricDifference(std::move(known_pool_tx_ids), known_block_id, is_bc_actual, txs,
                                                   deleted_tx_ids);
  lock.unlock();

  std::error_code ec;
  try {
    for (const auto& tx : txs) {
      new_txs.push_back(createTransactionPrefix(tx));
    }
  } catch (std::system_error& ex) {
    ec = ex.code();
  } catch (std::exception&) {
    ec = make_error_code(std::errc::invalid_argument);
  }

  callback(ec);
}

void INodeTrivialRefreshStub::includeTransactionsFromPoolToBlock() {
  m_blockchainGenerator.putTxPoolToBlockchain();
}

INodeTrivialRefreshStub::~INodeTrivialRefreshStub() {
  m_asyncCounter.waitAsyncContextsFinish();
}

void INodeTrivialRefreshStub::setMaxMixinCount(uint16_t maxMixin) {
  m_maxMixin = maxMixin;
}

void INodeTrivialRefreshStub::getBlocks(const std::vector<uint32_t>& blockHeights,
                                        std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  m_asyncCounter.addAsyncContext();

  std::thread task([=, &blocks]() mutable { doGetBlocks(blockHeights, blocks, callback); });
  task.detach();
}

void INodeTrivialRefreshStub::doGetBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  for (auto height : blockHeights) {
    if (m_blockchainGenerator.getBlockchain().size() <= height) {
      lock.unlock();
      callback(std::error_code(EDOM, std::generic_category()));
      return;
    }
    BlockDetails b = BlockDetails();
    b.index = height;
    b.isAlternative = false;
    auto cached = CachedBlock(m_blockchainGenerator.getBlockchain()[height]);
    b.hash = cached.getBlockHash();
    b.timestamp = cached.getBlock().timestamp;
    b.alreadyGeneratedTransactions = m_blockchainGenerator.getGeneratedTransactionsNumber(height);
    std::vector<BlockDetails> v;

    std::transform(cached.getBlock().transactionHashes.begin(), cached.getBlock().transactionHashes.end(),
                   std::back_inserter(b.transactions), [&](const Crypto::Hash& hash) {
                     return toDetails(m_blockchainGenerator.getTransactionByHash(hash), b.hash, b.index);
                   });
    v.push_back(b);
    blocks.push_back(v);
  }

  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  m_asyncCounter.addAsyncContext();

  std::thread{ [&blockHashes, &blocks, callback, this] { doGetBlocks(blockHashes, blocks, callback); } }.detach();
}

void INodeTrivialRefreshStub::doGetBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  for (const Crypto::Hash& hash: blockHashes) {
    auto iter = std::find_if(
        m_blockchainGenerator.getBlockchain().begin(), 
        m_blockchainGenerator.getBlockchain().end(), 
        [&hash](const BlockTemplate& block) -> bool {
          return hash == CachedBlock(block).getBlockHash();
        }
    );
    if (iter == m_blockchainGenerator.getBlockchain().end()) {
      lock.unlock();
      callback(std::error_code(EDOM, std::generic_category()));
      return;
    }
    BlockDetails b = BlockDetails();
    b.hash = CachedBlock(*iter).getBlockHash();
    b.isAlternative = false;
    blocks.push_back(b);
  }

  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::doGetBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) {
  assert(false);
/*
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  std::vector<Crypto::Hash> blockHashes;
  if (!m_blockchainGenerator.getBlockIdsByTimestamp(timestampBegin, timestampEnd, blocksNumberLimit, blockHashes, blocksNumberWithinTimestamps)) {
    callback(std::error_code(EDOM, std::generic_category()));
    return;
  }

  for (const auto& hash: blockHashes) {
    auto iter = std::find_if(
        m_blockchainGenerator.getBlockchain().begin(), 
        m_blockchainGenerator.getBlockchain().end(), 
        [&hash](const BlockTemplate& block) -> bool {
          return hash == CachedBlock(block).getBlockHash();
        }
    );
    if (iter == m_blockchainGenerator.getBlockchain().end()) {
      callback(std::error_code(EDOM, std::generic_category()));
      return;
    }
    BlockDetails b = BlockDetails();
    Crypto::Hash actualHash = get_block_hash(*iter);
    b.hash = actualHash;
    b.isOrphaned = false;
    b.timestamp = iter->timestamp;
    blocks.push_back(b);
  }

  callback(std::error_code());
*/
}
  
void INodeTrivialRefreshStub::doGetTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<CryptoNote::TransactionDetails>& transactions, const Callback& callback) {
  assert(false);
}
  
void INodeTrivialRefreshStub::getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  m_asyncCounter.addAsyncContext();

  std::thread task([=, &transactions]() mutable { doGetTransactions(transactionHashes, transactions, callback); });
  task.detach();
}

void INodeTrivialRefreshStub::doGetTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  for (const Crypto::Hash& hash : transactionHashes) {
    Transaction tx;
    TransactionDetails txDetails = TransactionDetails();
    if (m_blockchainGenerator.getTransactionByHash(hash, tx, false)) {
      auto detail = toDetails(tx, Crypto::Hash{}, 0);
      detail.inBlockchain = true;
      transactions.push_back(std::move(detail));
    } else if (m_blockchainGenerator.getTransactionByHash(hash, tx, true)) {
      auto detail = toDetails(tx, Crypto::Hash{}, 0);
      detail.inBlockchain = false;
      transactions.push_back(std::move(detail));
    } else {
      lock.unlock();
      callback(std::error_code(EDOM, std::generic_category()));
      return;
    }
  }

  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::isSynchronized(bool& syncStatus, const Callback& callback) {
  syncStatus = m_synchronized;
  callback(std::error_code());
}

void INodeTrivialRefreshStub::setSynchronizedStatus(bool status) {
  m_synchronized = status;
  if (m_synchronized) {
    observerManager.notify(&INodeObserver::blockchainSynchronized, getLastLocalBlockHeight());
  }
}

void INodeTrivialRefreshStub::sendPoolChanged() {
  observerManager.notify(&INodeObserver::poolChanged);
}

void INodeTrivialRefreshStub::sendLocalBlockchainUpdated(){
  observerManager.notify(&INodeObserver::localBlockchainUpdated, getLastLocalBlockHeight());
}

void INodeTrivialRefreshStub::getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, CryptoNote::MultisignatureOutput& out, const Callback& callback) {
  m_asyncCounter.addAsyncContext();
  std::unique_lock<std::mutex> lock(m_walletLock);
  std::thread task(&INodeTrivialRefreshStub::doGetOutByMSigGIndex, this, amount, gindex, std::ref(out), callback);
  task.detach();
}

void INodeTrivialRefreshStub::doGetOutByMSigGIndex(uint64_t amount, uint32_t gindex, CryptoNote::MultisignatureOutput& out, const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_walletLock);

  bool success = m_blockchainGenerator.getMultisignatureOutputByGlobalIndex(amount, gindex, out);

  lock.unlock();

  if (success) {
    callback(std::error_code());
  } else {
    callback(std::make_error_code(std::errc::invalid_argument));
  }
}
