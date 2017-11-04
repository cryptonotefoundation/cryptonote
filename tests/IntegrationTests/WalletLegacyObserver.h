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

#include "IWalletLegacy.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <unordered_map>

#include "../IntegrationTestLib/ObservableValue.h"

namespace CryptoNote {

class WalletLegacyObserver: public IWalletLegacyObserver {
public:

  WalletLegacyObserver() :
    m_actualBalance(0),
    m_actualBalancePrev(0),
    m_pendingBalance(0),
    m_pendingBalancePrev(0),
    m_syncResult(m_mutex, m_cv) {}

  virtual void actualBalanceUpdated(uint64_t actualBalance) override {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_actualBalance = actualBalance;
    lk.unlock();
    m_cv.notify_all();
  }

  virtual void pendingBalanceUpdated(uint64_t pendingBalance) override {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_pendingBalance = pendingBalance;
    lk.unlock();
    m_cv.notify_all();
  }

  virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) override {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_sendResults[transactionId] = result;
    m_cv.notify_all();
  }

  virtual void synchronizationCompleted(std::error_code result) override {
    m_syncResult.set(result);
  }

  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_syncProgress.emplace_back(current, total);
    m_currentHeight = current;
    m_cv.notify_all();
  }

  virtual void externalTransactionCreated(TransactionId transactionId) override {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_externalTransactions.push_back(transactionId);
    m_cv.notify_all();
  }
  
  uint64_t getCurrentHeight() {
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_currentHeight;
  }

  uint64_t waitPendingBalanceChange() {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (m_pendingBalance == m_pendingBalancePrev) {
      m_cv.wait(lk);
    }
    m_pendingBalancePrev = m_pendingBalance;
    return m_pendingBalance;
  }

  uint64_t waitTotalBalanceChange() {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (m_pendingBalance == m_pendingBalancePrev && m_actualBalance == m_actualBalancePrev) {
      m_cv.wait(lk);
    }

    m_actualBalancePrev = m_actualBalance;
    m_pendingBalancePrev = m_pendingBalance;

    return m_actualBalance + m_pendingBalance;
  }

  CryptoNote::TransactionId waitExternalTransaction() {
    std::unique_lock<std::mutex> lk(m_mutex);

    while (m_externalTransactions.empty()) {
      m_cv.wait(lk);
    }

    CryptoNote::TransactionId txId = m_externalTransactions.front();
    m_externalTransactions.pop_front();
    return txId;
  }

  template<class Rep, class Period>
  std::pair<bool, uint64_t> waitPendingBalanceChangeFor(const std::chrono::duration<Rep, Period>& timePeriod) {
    std::unique_lock<std::mutex> lk(m_mutex);
    bool result = m_cv.wait_for(lk, timePeriod, [&] { return m_pendingBalance != m_pendingBalancePrev; });
    m_pendingBalancePrev = m_pendingBalance;
    return std::make_pair(result, m_pendingBalance);
  }

  uint64_t waitActualBalanceChange() {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (m_actualBalance == m_actualBalancePrev) {
      m_cv.wait(lk);
    }
    m_actualBalancePrev = m_actualBalance;
    return m_actualBalance;
  }

  std::error_code waitSendResult(CryptoNote::TransactionId txid) {
    std::unique_lock<std::mutex> lk(m_mutex);

    std::unordered_map<CryptoNote::TransactionId, std::error_code>::iterator it;

    while ((it = m_sendResults.find(txid)) == m_sendResults.end()) {
      m_cv.wait(lk);
    }

    return it->second;
  }

  uint64_t totalBalance() {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_pendingBalancePrev = m_pendingBalance;
    m_actualBalancePrev = m_actualBalance;
    return m_pendingBalance + m_actualBalance;
  }

  std::vector<std::pair<uint32_t, uint32_t>> getSyncProgress() {
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_syncProgress;
  }

  ObservableValueBase<std::error_code> m_syncResult;

private:
    
  std::mutex m_mutex;
  std::condition_variable m_cv;

  uint64_t m_actualBalance;
  uint64_t m_actualBalancePrev;
  uint64_t m_pendingBalance;
  uint64_t m_pendingBalancePrev;

  uint32_t m_currentHeight;

  std::vector<std::pair<uint32_t, uint32_t>> m_syncProgress;

  std::unordered_map<CryptoNote::TransactionId, std::error_code> m_sendResults;
  std::deque<CryptoNote::TransactionId> m_externalTransactions;
};

}
