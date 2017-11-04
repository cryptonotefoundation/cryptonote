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

#include "TransactionPool.h"

#include "Common/int-util.h"
#include "CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/TransactionExtra.h"

namespace CryptoNote {

// lhs > hrs
bool TransactionPool::TransactionPriorityComparator::operator()(const PendingTransactionInfo& lhs, const PendingTransactionInfo& rhs) const {
  const CachedTransaction& left = lhs.cachedTransaction;
  const CachedTransaction& right = rhs.cachedTransaction;

  // price(lhs) = lhs.fee / lhs.blobSize
  // price(lhs) > price(rhs) -->
  // lhs.fee / lhs.blobSize > rhs.fee / rhs.blobSize -->
  // lhs.fee * rhs.blobSize > rhs.fee * lhs.blobSize
  uint64_t lhs_hi, lhs_lo = mul128(left.getTransactionFee(), right.getTransactionBinaryArray().size(), &lhs_hi);
  uint64_t rhs_hi, rhs_lo = mul128(right.getTransactionFee(), left.getTransactionBinaryArray().size(), &rhs_hi);

  return
    // prefer more profitable transactions
    (lhs_hi >  rhs_hi) ||
    (lhs_hi == rhs_hi && lhs_lo >  rhs_lo) ||
    // prefer smaller
    (lhs_hi == rhs_hi && lhs_lo == rhs_lo && left.getTransactionBinaryArray().size() <  right.getTransactionBinaryArray().size()) ||
    // prefer older
    (lhs_hi == rhs_hi && lhs_lo == rhs_lo && left.getTransactionBinaryArray().size() == right.getTransactionBinaryArray().size() && lhs.receiveTime < rhs.receiveTime);
}

const Crypto::Hash& TransactionPool::PendingTransactionInfo::getTransactionHash() const {
  return cachedTransaction.getTransactionHash();
}

size_t TransactionPool::PaymentIdHasher::operator() (const boost::optional<Crypto::Hash>& paymentId) const {
  if (!paymentId) {
    return std::numeric_limits<size_t>::max();
  }

  return std::hash<Crypto::Hash>{}(*paymentId);
}

TransactionPool::TransactionPool(Logging::ILogger& logger) :
  transactionHashIndex(transactions.get<TransactionHashTag>()),
  transactionCostIndex(transactions.get<TransactionCostTag>()),
  paymentIdIndex(transactions.get<PaymentIdTag>()),
  logger(logger, "TransactionPool") {
}

bool TransactionPool::pushTransaction(CachedTransaction&& transaction, TransactionValidatorState&& transactionState) {
  auto pendingTx = PendingTransactionInfo{static_cast<uint64_t>(time(nullptr)), std::move(transaction)};

  Crypto::Hash paymentId;
  if(getPaymentIdFromTxExtra(pendingTx.cachedTransaction.getTransaction().extra, paymentId)) {
    pendingTx.paymentId = paymentId;
  }

  if (transactionHashIndex.count(pendingTx.getTransactionHash()) > 0) {
    logger(Logging::DEBUGGING) << "pushTransaction: transaction hash already present in index";
    return false;
  }

  if (!mergeStates(poolState, transactionState)) {
    logger(Logging::DEBUGGING) << "pushTransaction: failed to merge states, some keys already used";
    return false;
  }

  logger(Logging::DEBUGGING) << "pushed transaction " << pendingTx.getTransactionHash() << " to pool";
  return transactionHashIndex.emplace(std::move(pendingTx)).second;
}

const CachedTransaction& TransactionPool::getTransaction(const Crypto::Hash& hash) const {
  auto it = transactionHashIndex.find(hash);
  assert(it != transactionHashIndex.end());

  return it->cachedTransaction;
}

bool TransactionPool::removeTransaction(const Crypto::Hash& hash) {
  auto it = transactionHashIndex.find(hash);
  if (it == transactionHashIndex.end()) {
    logger(Logging::DEBUGGING) << "removeTransaction: transaction not found";
    return false;
  }

  excludeFromState(poolState, it->cachedTransaction);
  transactionHashIndex.erase(it);

  logger(Logging::DEBUGGING) << "transaction " << hash << " removed from pool";
  return true;
}

size_t TransactionPool::getTransactionCount() const {
  return transactionHashIndex.size();
}

std::vector<Crypto::Hash> TransactionPool::getTransactionHashes() const {
  std::vector<Crypto::Hash> hashes;
  for (auto it = transactionCostIndex.begin(); it != transactionCostIndex.end(); ++it) {
    hashes.push_back(it->getTransactionHash());
  }

  return hashes;
}

bool TransactionPool::checkIfTransactionPresent(const Crypto::Hash& hash) const {
  return transactionHashIndex.find(hash) != transactionHashIndex.end();
}

const TransactionValidatorState& TransactionPool::getPoolTransactionValidationState() const {
  return poolState;
}

std::vector<CachedTransaction> TransactionPool::getPoolTransactions() const {
  std::vector<CachedTransaction> result;
  result.reserve(transactionCostIndex.size());

  for (const auto& transactionItem: transactionCostIndex) {
    result.emplace_back(transactionItem.cachedTransaction);
  }

  return result;
}

uint64_t TransactionPool::getTransactionReceiveTime(const Crypto::Hash& hash) const {
  auto it = transactionHashIndex.find(hash);
  assert(it != transactionHashIndex.end());

  return it->receiveTime;
}

std::vector<Crypto::Hash> TransactionPool::getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) const {
  boost::optional<Crypto::Hash> p(paymentId);

  auto range = paymentIdIndex.equal_range(p);
  std::vector<Crypto::Hash> transactionHashes;
  transactionHashes.reserve(std::distance(range.first, range.second));
  for (auto it = range.first; it != range.second; ++it) {
    transactionHashes.push_back(it->getTransactionHash());
  }

  return transactionHashes;
}

}
