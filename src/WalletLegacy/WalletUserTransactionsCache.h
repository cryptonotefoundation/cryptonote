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

#include "crypto/hash.h"
#include "IWalletLegacy.h"
#include "ITransfersContainer.h"

#include "WalletLegacy/WalletLegacyEvent.h"
#include "WalletLegacy/WalletUnconfirmedTransactions.h"

namespace CryptoNote {
class ISerializer;
}

namespace CryptoNote {

class WalletUserTransactionsCache
{
public:
  explicit WalletUserTransactionsCache(uint64_t mempoolTxLiveTime = 60 * 60 * 24);

  bool serialize(CryptoNote::ISerializer& serializer);

  uint64_t unconfirmedTransactionsAmount() const;
  uint64_t unconfrimedOutsAmount() const;
  size_t getTransactionCount() const;
  size_t getTransferCount() const;

  TransactionId addNewTransaction(uint64_t amount, uint64_t fee, const std::string& extra, const std::vector<WalletLegacyTransfer>& transfers, uint64_t unlockTime);
  void updateTransaction(TransactionId transactionId, const CryptoNote::Transaction& tx, uint64_t amount, const std::list<TransactionOutputInformation>& usedOutputs);
  void updateTransactionSendingState(TransactionId transactionId, std::error_code ec);

  std::shared_ptr<WalletLegacyEvent> onTransactionUpdated(const TransactionInformation& txInfo, int64_t txBalance);
  std::shared_ptr<WalletLegacyEvent> onTransactionDeleted(const Crypto::Hash& transactionHash);

  TransactionId findTransactionByTransferId(TransferId transferId) const;

  bool getTransaction(TransactionId transactionId, WalletLegacyTransaction& transaction) const;
  WalletLegacyTransaction& getTransaction(TransactionId transactionId);
  bool getTransfer(TransferId transferId, WalletLegacyTransfer& transfer) const;
  WalletLegacyTransfer& getTransfer(TransferId transferId);

  bool isUsed(const TransactionOutputInformation& out) const;
  void reset();

  std::vector<TransactionId> deleteOutdatedTransactions();

private:

  TransactionId findTransactionByHash(const Crypto::Hash& hash);
  TransactionId insertTransaction(WalletLegacyTransaction&& Transaction);
  TransferId insertTransfers(const std::vector<WalletLegacyTransfer>& transfers);
  void updateUnconfirmedTransactions();

  typedef std::vector<WalletLegacyTransfer> UserTransfers;
  typedef std::vector<WalletLegacyTransaction> UserTransactions;

  void getGoodItems(UserTransactions& transactions, UserTransfers& transfers);
  void getGoodTransaction(TransactionId txId, size_t offset, UserTransactions& transactions, UserTransfers& transfers);

  void getTransfersByTx(TransactionId id, UserTransfers& transfers);

  UserTransactions m_transactions;
  UserTransfers m_transfers;
  WalletUnconfirmedTransactions m_unconfirmedTransactions;
};

} //namespace CryptoNote
