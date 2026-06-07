// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include <algorithm>
#include <future>
#include "crypto/crypto.h"
#include "crypto/random.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"

#include "WalletLegacy/WalletTransactionSender.h"
#include "WalletLegacy/WalletUtils.h"

#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

#include <Logging/LoggerGroup.h>

using namespace Crypto;

namespace {

using namespace CryptoNote;

uint64_t countNeededMoney(uint64_t fee, const std::vector<WalletLegacyTransfer>& transfers) {
  uint64_t needed_money = fee;
  for (auto& transfer: transfers) {
    throwIf(transfer.amount == 0, error::ZERO_DESTINATION);
    throwIf(transfer.amount < 0, error::WRONG_AMOUNT);

    needed_money += transfer.amount;
    throwIf(static_cast<int64_t>(needed_money) < transfer.amount, error::SUM_OVERFLOW);
  }

  return needed_money;
}

void createChangeDestinations(const AccountPublicAddress& address, uint64_t neededMoney, uint64_t foundMoney, CryptoNote::TxBuildOutput& changeDts) {
  if (neededMoney < foundMoney) {
    changeDts.destination = address;
    changeDts.amount = foundMoney - neededMoney;
  }
}

std::shared_ptr<WalletLegacyEvent> makeCompleteEvent(WalletUserTransactionsCache& transactionCache, size_t transactionId, std::error_code ec) {
  transactionCache.updateTransactionSendingState(transactionId, ec);
  return std::make_shared<WalletSendTransactionCompletedEvent>(transactionId, ec);
}

} //namespace

namespace CryptoNote {

WalletTransactionSender::WalletTransactionSender(const Currency& currency, WalletUserTransactionsCache& transactionsCache, AccountKeys keys, ITransfersContainer& transfersContainer, INode& node) :
  m_currency(currency),
  m_node(node),
  m_transactionsCache(transactionsCache),
  m_isStoping(false),
  m_keys(keys),
  m_transferDetails(transfersContainer),
  m_upperTransactionSizeLimit(m_currency.maxTransactionSizeLimit()) {
}

void WalletTransactionSender::stop() {
  m_isStoping = true;
}

bool WalletTransactionSender::validateDestinationAddress(const std::string& address) {
  AccountPublicAddress ignore;
  return m_currency.parseAccountAddressString(address, ignore);
}

void WalletTransactionSender::validateTransfersAddresses(const std::vector<WalletLegacyTransfer>& transfers) {
  for (const WalletLegacyTransfer& tr : transfers) {
    if (!validateDestinationAddress(tr.address)) {
      throw std::system_error(make_error_code(error::BAD_ADDRESS));
    }
  }
}

std::shared_ptr<WalletRequest> WalletTransactionSender::makeSendRequest(TransactionId& transactionId, std::deque<std::shared_ptr<WalletLegacyEvent>>& events,
    const std::vector<WalletLegacyTransfer>& transfers, const std::list<TransactionOutputInformation>& selectedOuts, uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp) {

  using namespace CryptoNote;

  throwIf(transfers.empty(), error::ZERO_DESTINATION);
  validateTransfersAddresses(transfers);
  uint64_t neededMoney = countNeededMoney(fee, transfers);

  std::shared_ptr<SendTransactionContext> context = std::make_shared<SendTransactionContext>();

  if (selectedOuts.size() > 0) {
    for (auto& out : selectedOuts) {
      context->foundMoney += out.amount;
    }
    context->selectedTransfers = selectedOuts;
  }
  else {
    context->foundMoney = selectTransfersToSend(neededMoney, 0 == mixIn, context->dustPolicy.dustThreshold, context->selectedTransfers);
  }

  throwIf(context->foundMoney < neededMoney, error::WRONG_AMOUNT);

  transactionId = m_transactionsCache.addNewTransaction(neededMoney, fee, extra, transfers, unlockTimestamp);
  context->transactionId = transactionId;
  context->mixIn = mixIn;

  if(context->mixIn) {
    std::shared_ptr<WalletRequest> request = makeGetRandomOutsRequest(context);
    return request;
  }

  return doSendTransaction(context, events);
}

std::string WalletTransactionSender::makeRawTransaction(TransactionId& transactionId, std::deque<std::shared_ptr<WalletLegacyEvent>>& events,
  const std::vector<WalletLegacyTransfer>& transfers, const std::list<CryptoNote::TransactionOutputInformation>& selectedOuts,
  uint64_t fee, const std::string& extra, uint64_t mixIn, uint64_t unlockTimestamp)
{

  std::string raw_tx;

  using namespace CryptoNote;

  throwIf(transfers.empty(), error::ZERO_DESTINATION);
  validateTransfersAddresses(transfers);
  uint64_t neededMoney = countNeededMoney(fee, transfers);

  std::shared_ptr<SendTransactionContext> context = std::make_shared<SendTransactionContext>();

  if (selectedOuts.size() > 0) {
    for (auto& out : selectedOuts) {
      context->foundMoney += out.amount;
    }
    context->selectedTransfers = selectedOuts;
  }
  else {
     context->foundMoney = selectTransfersToSend(neededMoney, 0 == mixIn, context->dustPolicy.dustThreshold, context->selectedTransfers);
  }

  throwIf(context->foundMoney < neededMoney, error::WRONG_AMOUNT);

  // add tx to wallet cache to prevent reuse of outputs used in this tx
  transactionId = m_transactionsCache.addNewTransaction(neededMoney, fee, extra, transfers, unlockTimestamp);
  context->transactionId = transactionId;
  context->mixIn = mixIn;

  if (context->mixIn) {
    uint64_t outsCount = mixIn + 1; // add one to make possible (if need) to skip real output key
    std::vector<uint64_t> amounts;

    for (const auto& td : context->selectedTransfers) {
      amounts.push_back(td.amount);
    }

    auto queryAmountsCompleted = std::promise<std::error_code>();
    auto queryAmountsWaitFuture = queryAmountsCompleted.get_future();

    m_node.getRandomOutsByAmounts(std::move(amounts),
      outsCount,
      std::ref(context->outs),
      [&queryAmountsCompleted](std::error_code ec) {
      auto detachedPromise = std::move(queryAmountsCompleted);
      detachedPromise.set_value(ec);
    });

    queryAmountsWaitFuture.get();

    auto scanty_it = std::find_if(context->outs.begin(), context->outs.end(),
      [&](COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& out) {
      return out.outs.size() < mixIn;
    });

    if (scanty_it != context->outs.end()) {
      throw std::system_error(make_error_code(error::MIXIN_COUNT_TOO_BIG));
      return raw_tx;
    }
  }

  // instead of doSendTransaction prepare tx here to prevent relay
  try
  {
    WalletLegacyTransaction& transaction = m_transactionsCache.getTransaction(context->transactionId);

    uint64_t totalAmount = 0;
    Transaction tx = buildTransactionFromContext(context, transaction, totalAmount);

    m_transactionsCache.updateTransaction(context->transactionId, tx, totalAmount, context->selectedTransfers, context->tx_key);

    notifyBalanceChanged(events);

    raw_tx = Common::toHex(toBinaryArray(tx));

    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, std::error_code()));
  }
  catch (std::system_error& ec) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec.code()));
  }
  catch (std::exception&) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(error::INTERNAL_WALLET_ERROR)));
  }

  return raw_tx;
}

std::shared_ptr<WalletRequest> WalletTransactionSender::makeGetRandomOutsRequest(std::shared_ptr<SendTransactionContext> context) {
  uint64_t outsCount = context->mixIn + 1;// add one to make possible (if need) to skip real output key
  std::vector<uint64_t> amounts;

  for (const auto& td : context->selectedTransfers) {
    amounts.push_back(td.amount);
  }

  return std::make_shared<WalletGetRandomOutsByAmountsRequest>(amounts, outsCount, context, std::bind(&WalletTransactionSender::sendTransactionRandomOutsByAmount,
      this, context, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void WalletTransactionSender::sendTransactionRandomOutsByAmount(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletLegacyEvent>>& events,
    boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec) {
  
  if (m_isStoping) {
    ec = make_error_code(error::TX_CANCELLED);
  }

  if (ec) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec));
    return;
  }

  auto scanty_it = std::find_if(context->outs.begin(), context->outs.end(), 
    [&] (COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& out) {return out.outs.size() < context->mixIn;});

  if (scanty_it != context->outs.end()) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(error::MIXIN_COUNT_TOO_BIG)));
    return;
  }

  std::shared_ptr<WalletRequest> req = doSendTransaction(context, events);
  if (req)
    nextRequest = req;
}

Transaction WalletTransactionSender::buildTransactionFromContext(std::shared_ptr<SendTransactionContext> context, WalletLegacyTransaction& transaction, uint64_t& totalAmount) {
  std::vector<TxBuildInput> inputs;
  prepareInputs(context->selectedTransfers, context->outs, inputs, context->mixIn);

  TxBuildOutput changeDts;
  changeDts.amount = 0;
  totalAmount = -transaction.totalAmount;
  createChangeDestinations(m_keys.address, totalAmount, context->foundMoney, changeDts);

  std::vector<TxBuildOutput> splittedDests;
  splitDestinations(transaction.firstTransferId, transaction.transferCount, changeDts, context->dustPolicy, splittedDests);

  auto itx = buildTransaction(inputs, splittedDests, m_keys.viewSecretKey,
      transaction.extra, transaction.unlockTime, m_upperTransactionSizeLimit, context->tx_key);
  Transaction tx;
  if (!fromBinaryArray(tx, itx->getTransactionData()))
    throw std::system_error(make_error_code(error::INTERNAL_WALLET_ERROR));

  getObjectHash(tx, transaction.hash);
  transaction.secretKey = context->tx_key;

  return tx;
}

std::shared_ptr<WalletRequest> WalletTransactionSender::doSendTransaction(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletLegacyEvent>>& events) {
  if (m_isStoping) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(error::TX_CANCELLED)));
    return std::shared_ptr<WalletRequest>();
  }

  try
  {
    WalletLegacyTransaction& transaction = m_transactionsCache.getTransaction(context->transactionId);

    uint64_t totalAmount = 0;
    Transaction tx = buildTransactionFromContext(context, transaction, totalAmount);

    m_transactionsCache.updateTransaction(context->transactionId, tx, totalAmount, context->selectedTransfers, context->tx_key);

    notifyBalanceChanged(events);

    return std::make_shared<WalletRelayTransactionRequest>(tx, std::bind(&WalletTransactionSender::relayTransactionCallback, this, context,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }
  catch(std::system_error& ec) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec.code()));
  }
  catch(std::exception&) {
    events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, make_error_code(error::INTERNAL_WALLET_ERROR)));
  }

  return std::shared_ptr<WalletRequest>();
}

void WalletTransactionSender::relayTransactionCallback(std::shared_ptr<SendTransactionContext> context, std::deque<std::shared_ptr<WalletLegacyEvent>>& events,
                                                       boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec) {
  if (m_isStoping) {
    return;
  }

  events.push_back(makeCompleteEvent(m_transactionsCache, context->transactionId, ec));
}


void WalletTransactionSender::splitDestinations(TransferId firstTransferId, size_t transfersCount, const TxBuildOutput& changeDts,
  const TxDustPolicy& dustPolicy, std::vector<TxBuildOutput>& splittedDests) {
  uint64_t dust = 0;

  digitSplitStrategy(firstTransferId, transfersCount, changeDts, dustPolicy.dustThreshold, splittedDests, dust);

  throwIf(dustPolicy.dustThreshold < dust, error::INTERNAL_WALLET_ERROR);
  if (0 != dust && !dustPolicy.addToFee) {
    splittedDests.push_back(TxBuildOutput{dustPolicy.addrForDust, dust});
  }
}


void WalletTransactionSender::digitSplitStrategy(TransferId firstTransferId, size_t transfersCount,
  const TxBuildOutput& change_dst, uint64_t dust_threshold,
  std::vector<TxBuildOutput>& splitted_dsts, uint64_t& dust) {
  splitted_dsts.clear();
  dust = 0;

  for (TransferId idx = firstTransferId; idx < firstTransferId + transfersCount; ++idx) {
    WalletLegacyTransfer& de = m_transactionsCache.getTransfer(idx);

    AccountPublicAddress addr;
    if (!m_currency.parseAccountAddressString(de.address, addr)) {
      throw std::system_error(make_error_code(error::BAD_ADDRESS));
    }

    decompose_amount_into_digits(de.amount, dust_threshold,
      [&](uint64_t chunk) { splitted_dsts.push_back(TxBuildOutput{addr, chunk}); },
      [&](uint64_t a_dust) { splitted_dsts.push_back(TxBuildOutput{addr, a_dust}); });
  }

  decompose_amount_into_digits(change_dst.amount, dust_threshold,
    [&](uint64_t chunk) { splitted_dsts.push_back(TxBuildOutput{change_dst.destination, chunk}); },
    [&](uint64_t a_dust) { dust = a_dust; } );
}


void WalletTransactionSender::prepareInputs(
  const std::list<TransactionOutputInformation>& selectedTransfers,
  std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs,
  std::vector<TxBuildInput>& inputs, uint64_t mixIn) {

  size_t i = 0;

  for (const auto& td: selectedTransfers) {
    inputs.resize(inputs.size() + 1);
    TxBuildInput& inp = inputs.back();

    inp.keyInfo.amount = td.amount;
    inp.senderKeys = m_keys;

    //paste mixin transaction
    if (outs.size()) {
      std::sort(outs[i].outs.begin(), outs[i].outs.end(),
        [](const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& a, const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& b){ return a.global_amount_index < b.global_amount_index; });
      for (auto& daemon_oe: outs[i].outs) {
        if (td.globalOutputIndex == daemon_oe.global_amount_index)
          continue;
        TransactionTypes::GlobalOutput go;
        go.outputIndex = static_cast<uint32_t>(daemon_oe.global_amount_index);
        go.targetKey = daemon_oe.out_key;
        inp.keyInfo.outputs.push_back(go);
        if (inp.keyInfo.outputs.size() >= mixIn)
          break;
      }
    }

    //paste real transaction to the random index
    auto it_to_insert = std::find_if(inp.keyInfo.outputs.begin(), inp.keyInfo.outputs.end(),
      [&](const TransactionTypes::GlobalOutput& a) { return a.outputIndex >= td.globalOutputIndex; });

    TransactionTypes::GlobalOutput real_go;
    real_go.outputIndex = td.globalOutputIndex;
    real_go.targetKey = td.outputKey;

    auto inserted_it = inp.keyInfo.outputs.insert(it_to_insert, real_go);

    inp.keyInfo.realOutput.transactionPublicKey = td.transactionPublicKey;
    inp.keyInfo.realOutput.transactionIndex = inserted_it - inp.keyInfo.outputs.begin();
    inp.keyInfo.realOutput.outputInTransaction = td.outputInTransaction;
    ++i;
  }
}

void WalletTransactionSender::notifyBalanceChanged(std::deque<std::shared_ptr<WalletLegacyEvent>>& events) {
  uint64_t unconfirmedOutsAmount = m_transactionsCache.unconfrimedOutsAmount();
  uint64_t change = unconfirmedOutsAmount - m_transactionsCache.unconfirmedTransactionsAmount();

  uint64_t actualBalance = m_transferDetails.balance(ITransfersContainer::IncludeKeyUnlocked) - unconfirmedOutsAmount;
  uint64_t pendingBalance = m_transferDetails.balance(ITransfersContainer::IncludeKeyNotUnlocked) + change;

  events.push_back(std::make_shared<WalletActualBalanceUpdatedEvent>(actualBalance));
  events.push_back(std::make_shared<WalletPendingBalanceUpdatedEvent>(pendingBalance));
}

namespace {

template<typename URNG, typename T>
T popRandomValue(URNG& randomGenerator, std::vector<T>& vec) {
  assert(!vec.empty());

  if (vec.empty()) {
    return T();
  }

  std::uniform_int_distribution<size_t> distribution(0, vec.size() - 1);
  size_t idx = distribution(randomGenerator);

  T res = vec[idx];
  if (idx + 1 != vec.size()) {
    vec[idx] = vec.back();
  }
  vec.resize(vec.size() - 1);

  return res;
}

}


uint64_t WalletTransactionSender::selectTransfersToSend(uint64_t neededMoney, bool addUnmixable, uint64_t dust, std::list<TransactionOutputInformation>& selectedTransfers) {

  std::vector<size_t> unusedTransfers;
  std::vector<size_t> unusedDust;
  std::vector<size_t> unusedUnmixable;
  
  std::vector<TransactionOutputInformation> outputs;
  m_transferDetails.getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);

  for (size_t i = 0; i < outputs.size(); ++i) {
    const auto& out = outputs[i];
    if (!m_transactionsCache.isUsed(out)) {
      if (is_valid_decomposed_amount(out.amount)) {
        if (dust < out.amount) {
          unusedTransfers.push_back(i);
        } else {
          unusedDust.push_back(i);
        }
      } else {
        unusedUnmixable.push_back(i);
      }
    }
  }

  uint64_t foundMoney = 0;

  while (foundMoney < neededMoney && (!unusedTransfers.empty() || !unusedDust.empty() || (addUnmixable && !unusedUnmixable.empty()))) {
    size_t idx;
    std::mt19937 urng = Random::generator();
    if (addUnmixable && !unusedUnmixable.empty()) {
      idx = popRandomValue(urng, unusedUnmixable);
    } else {
      idx = !unusedTransfers.empty() ? popRandomValue(urng, unusedTransfers) : popRandomValue(urng, unusedDust);
    }
    selectedTransfers.push_back(outputs[idx]);
    foundMoney += outputs[idx].amount;
  }

  return foundMoney;

}

} /* namespace CryptoNote */
