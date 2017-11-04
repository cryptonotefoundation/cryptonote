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

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Currency.h"

#include "INode.h"
#include "WalletLegacy/WalletSendTransactionContext.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "WalletLegacy/WalletUnconfirmedTransactions.h"
#include "WalletLegacy/WalletRequest.h"

#include "ITransfersContainer.h"

namespace CryptoNote {

class WalletTransactionSender
{
public:
  WalletTransactionSender(const Currency& currency, WalletUserTransactionsCache& transactionsCache, AccountKeys keys, ITransfersContainer& transfersContainer);

  void stop();

  std::shared_ptr<WalletRequest> makeSendRequest(TransactionId& transactionId, std::deque<std::shared_ptr<WalletLegacyEvent>>& events,
    const std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0);

private:
  std::shared_ptr<WalletRequest> makeGetRandomOutsRequest(std::shared_ptr<SendTransactionContext> context);
  std::shared_ptr<WalletRequest> doSendTransaction(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletLegacyEvent>>& events);
  void prepareInputs(const std::list<TransactionOutputInformation>& selectedTransfers, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs,
      std::vector<TransactionSourceEntry>& sources, uint64_t mixIn);
  void splitDestinations(TransferId firstTransferId, size_t transfersCount, const TransactionDestinationEntry& changeDts,
    const TxDustPolicy& dustPolicy, std::vector<TransactionDestinationEntry>& splittedDests);
  void digitSplitStrategy(TransferId firstTransferId, size_t transfersCount, const TransactionDestinationEntry& change_dst, uint64_t dust_threshold,
    std::vector<TransactionDestinationEntry>& splitted_dsts, uint64_t& dust);
  void sendTransactionRandomOutsByAmount(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletLegacyEvent>>& events,
      boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec);
  void relayTransactionCallback(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletLegacyEvent>>& events,
                                boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec);
  void notifyBalanceChanged(std::deque<std::shared_ptr<WalletLegacyEvent>>& events);

  void validateTransfersAddresses(const std::vector<WalletLegacyTransfer>& transfers);
  bool validateDestinationAddress(const std::string& address);

  uint64_t selectTransfersToSend(uint64_t neededMoney, bool addDust, uint64_t dust, std::list<TransactionOutputInformation>& selectedTransfers);

  const Currency& m_currency;
  AccountKeys m_keys;
  WalletUserTransactionsCache& m_transactionsCache;
  uint64_t m_upperTransactionSizeLimit;

  bool m_isStoping;
  ITransfersContainer& m_transferDetails;
};

} /* namespace CryptoNote */
