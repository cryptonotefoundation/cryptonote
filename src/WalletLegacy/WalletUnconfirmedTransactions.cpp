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

#include "WalletUnconfirmedTransactions.h"
#include "WalletLegacy/WalletLegacySerialization.h"

#include "CryptoNoteCore/CryptoNoteTools.h"
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"

using namespace Crypto;

namespace CryptoNote {

inline TransactionOutputId getOutputId(const TransactionOutputInformation& out) {
  return std::make_pair(out.transactionPublicKey, out.outputInTransaction);
}

WalletUnconfirmedTransactions::WalletUnconfirmedTransactions(uint64_t uncofirmedTransactionsLiveTime):
  m_uncofirmedTransactionsLiveTime(uncofirmedTransactionsLiveTime) {

}

bool WalletUnconfirmedTransactions::serialize(ISerializer& s) {
  s(m_unconfirmedTxs, "transactions");
  if (s.type() == ISerializer::INPUT) {
    collectUsedOutputs();
  }
  return true;
}

bool WalletUnconfirmedTransactions::findTransactionId(const Hash& hash, TransactionId& id) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it == m_unconfirmedTxs.end()) {
    return false;
  }

  id = it->second.transactionId;
  return true;
}

void WalletUnconfirmedTransactions::erase(const Hash& hash) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it == m_unconfirmedTxs.end()) {
    return;
  }

  deleteUsedOutputs(it->second.usedOutputs);
  m_unconfirmedTxs.erase(it);
}

void WalletUnconfirmedTransactions::add(const Transaction& tx, TransactionId transactionId, 
  uint64_t amount, const std::list<TransactionOutputInformation>& usedOutputs) {

  UnconfirmedTransferDetails& utd = m_unconfirmedTxs[getObjectHash(tx)];

  utd.amount = amount;
  utd.sentTime = time(nullptr);
  utd.tx = tx;
  utd.transactionId = transactionId;

  uint64_t outsAmount = 0;
  // process used outputs
  utd.usedOutputs.reserve(usedOutputs.size());
  for (const auto& out : usedOutputs) {
    auto id = getOutputId(out);
    utd.usedOutputs.push_back(id);
    m_usedOutputs.insert(id);
    outsAmount += out.amount;
  }

  utd.outsAmount = outsAmount;
}

void WalletUnconfirmedTransactions::updateTransactionId(const Hash& hash, TransactionId id) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it != m_unconfirmedTxs.end()) {
    it->second.transactionId = id;
  }
}

uint64_t WalletUnconfirmedTransactions::countUnconfirmedOutsAmount() const {
  uint64_t amount = 0;

  for (auto& utx: m_unconfirmedTxs)
    amount+= utx.second.outsAmount;

  return amount;
}

uint64_t WalletUnconfirmedTransactions::countUnconfirmedTransactionsAmount() const {
  uint64_t amount = 0;

  for (auto& utx: m_unconfirmedTxs)
    amount+= utx.second.amount;

  return amount;
}

bool WalletUnconfirmedTransactions::isUsed(const TransactionOutputInformation& out) const {
  return m_usedOutputs.find(getOutputId(out)) != m_usedOutputs.end();
}

void WalletUnconfirmedTransactions::collectUsedOutputs() {
  UsedOutputsContainer used;
  for (const auto& kv : m_unconfirmedTxs) {
    used.insert(kv.second.usedOutputs.begin(), kv.second.usedOutputs.end());
  }
  m_usedOutputs = std::move(used);
}

void WalletUnconfirmedTransactions::reset() {
  m_unconfirmedTxs.clear();
  m_usedOutputs.clear();
}

void WalletUnconfirmedTransactions::deleteUsedOutputs(const std::vector<TransactionOutputId>& usedOutputs) {
  for (const auto& output: usedOutputs) {
    m_usedOutputs.erase(output);
  }
}

std::vector<TransactionId> WalletUnconfirmedTransactions::deleteOutdatedTransactions() {
  std::vector<TransactionId> deletedTransactions;

  uint64_t now = static_cast<uint64_t>(time(nullptr));
  assert(now >= m_uncofirmedTransactionsLiveTime);

  for (auto it = m_unconfirmedTxs.begin(); it != m_unconfirmedTxs.end();) {
    if (static_cast<uint64_t>(it->second.sentTime) <= now - m_uncofirmedTransactionsLiveTime) {
      deleteUsedOutputs(it->second.usedOutputs);
      deletedTransactions.push_back(it->second.transactionId);
      it = m_unconfirmedTxs.erase(it);
    } else {
      ++it;
    }
  }

  return deletedTransactions;
}

} /* namespace CryptoNote */
