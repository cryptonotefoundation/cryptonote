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

#include "Common/IInputStream.h"
#include "Common/IOutputStream.h"
#include "Serialization/ISerializer.h"
#include "Transfers/TransfersSynchronizer.h"
#include "Wallet/WalletIndices.h"

namespace CryptoNote {

class WalletSerializerV2 {
public:
  WalletSerializerV2(
    ITransfersObserver& transfersObserver,
    Crypto::PublicKey& viewPublicKey,
    Crypto::SecretKey& viewSecretKey,
    uint64_t& actualBalance,
    uint64_t& pendingBalance,
    WalletsContainer& walletsContainer,
    TransfersSyncronizer& synchronizer,
    UnlockTransactionJobs& unlockTransactions,
    WalletTransactions& transactions,
    WalletTransfers& transfers,
    UncommitedTransactions& uncommitedTransactions,
    std::string& extra,
    uint32_t transactionSoftLockTime
  );

  void load(Common::IInputStream& source, uint8_t version);
  void save(Common::IOutputStream& destination, WalletSaveLevel saveLevel);

  std::unordered_set<Crypto::PublicKey>& addedKeys();
  std::unordered_set<Crypto::PublicKey>& deletedKeys();

  static const uint8_t MIN_VERSION = 6;
  static const uint8_t SERIALIZATION_VERSION = 6;

private:
  void loadKeyListAndBanalces(CryptoNote::ISerializer& serializer, bool saveCache);
  void saveKeyListAndBanalces(CryptoNote::ISerializer& serializer, bool saveCache);
    
  void loadTransactions(CryptoNote::ISerializer& serializer);
  void saveTransactions(CryptoNote::ISerializer& serializer);

  void loadTransfers(CryptoNote::ISerializer& serializer);
  void saveTransfers(CryptoNote::ISerializer& serializer);

  void loadTransfersSynchronizer(CryptoNote::ISerializer& serializer);
  void saveTransfersSynchronizer(CryptoNote::ISerializer& serializer);

  void loadUnlockTransactionsJobs(CryptoNote::ISerializer& serializer);
  void saveUnlockTransactionsJobs(CryptoNote::ISerializer& serializer);

  ITransfersObserver& m_transfersObserver;
  uint64_t& m_actualBalance;
  uint64_t& m_pendingBalance;
  WalletsContainer& m_walletsContainer;
  TransfersSyncronizer& m_synchronizer;
  UnlockTransactionJobs& m_unlockTransactions;
  WalletTransactions& m_transactions;
  WalletTransfers& m_transfers;
  UncommitedTransactions& m_uncommitedTransactions;
  std::string& m_extra;
  uint32_t m_transactionSoftLockTime;

  std::unordered_set<Crypto::PublicKey> m_addedKeys;
  std::unordered_set<Crypto::PublicKey> m_deletedKeys;
};

} //namespace CryptoNote
