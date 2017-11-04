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

#include "WalletHelper.h"
#include "Common/PathTools.h"

#include <fstream>
#include <boost/filesystem.hpp>

using namespace CryptoNote;

namespace {

void openOutputFileStream(const std::string& filename, std::ofstream& file) {
  file.open(filename, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
  if (file.fail()) {
    throw std::runtime_error("error opening file: " + filename);
  }
}

std::error_code walletSaveWrapper(CryptoNote::IWalletLegacy& wallet, std::ofstream& file, bool saveDetailes, bool saveCache) {
  CryptoNote::WalletHelper::SaveWalletResultObserver o;

  std::error_code e;
  try {
    std::future<std::error_code> f = o.saveResult.get_future();
    wallet.addObserver(&o);
    wallet.save(file, saveDetailes, saveCache);
    e = f.get();
  } catch (std::exception&) {
    wallet.removeObserver(&o);
    return make_error_code(std::errc::invalid_argument);
  }

  wallet.removeObserver(&o);
  return e;
}

}

void WalletHelper::prepareFileNames(const std::string& file_path, std::string& keys_file, std::string& wallet_file) {
  if (Common::GetExtension(file_path) == ".wallet") {
    keys_file = Common::RemoveExtension(file_path) + ".keys";
    wallet_file = file_path;
  } else if (Common::GetExtension(file_path) == ".keys") {
    keys_file = file_path;
    wallet_file = Common::RemoveExtension(file_path) + ".wallet";
  } else {
    keys_file = file_path + ".keys";
    wallet_file = file_path + ".wallet";
  }
}

void WalletHelper::SendCompleteResultObserver::sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_finishedTransactions[transactionId] = result;
  m_condition.notify_one();
}

std::error_code WalletHelper::SendCompleteResultObserver::wait(CryptoNote::TransactionId transactionId) {
  std::unique_lock<std::mutex> lock(m_mutex);

  m_condition.wait(lock, [this, &transactionId] {
    auto it = m_finishedTransactions.find(transactionId);
    if (it == m_finishedTransactions.end()) {
      return false;
    }

    m_result = it->second;
    return true;
  });

  return m_result;
}

WalletHelper::IWalletRemoveObserverGuard::IWalletRemoveObserverGuard(CryptoNote::IWalletLegacy& wallet, CryptoNote::IWalletLegacyObserver& observer) :
  m_wallet(wallet),
  m_observer(observer),
  m_removed(false) {
  m_wallet.addObserver(&m_observer);
}

WalletHelper::IWalletRemoveObserverGuard::~IWalletRemoveObserverGuard() {
  if (!m_removed) {
    m_wallet.removeObserver(&m_observer);
  }
}

void WalletHelper::IWalletRemoveObserverGuard::removeObserver() {
  m_wallet.removeObserver(&m_observer);
  m_removed = true;
}

void WalletHelper::storeWallet(CryptoNote::IWalletLegacy& wallet, const std::string& walletFilename) {
  boost::filesystem::path tempFile = boost::filesystem::unique_path(walletFilename + ".tmp.%%%%-%%%%");

  if (boost::filesystem::exists(walletFilename)) {
    boost::filesystem::rename(walletFilename, tempFile);
  }

  std::ofstream file;
  try {
    openOutputFileStream(walletFilename, file);
  } catch (std::exception&) {
    if (boost::filesystem::exists(tempFile)) {
      boost::filesystem::rename(tempFile, walletFilename);
    }
    throw;
  }

  std::error_code saveError = walletSaveWrapper(wallet, file, true, true);
  if (saveError) {
    file.close();
    boost::filesystem::remove(walletFilename);
    boost::filesystem::rename(tempFile, walletFilename);
    throw std::system_error(saveError);
  }

  file.close();

  boost::system::error_code ignore;
  boost::filesystem::remove(tempFile, ignore);
}
