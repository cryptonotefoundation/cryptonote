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

#include <list>
#include <vector>

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "IWalletLegacy.h"
#include "ITransfersContainer.h"

namespace CryptoNote {

struct TxDustPolicy
{
  uint64_t dustThreshold;
  bool addToFee;
  CryptoNote::AccountPublicAddress addrForDust;

  TxDustPolicy(uint64_t a_dust_threshold = 0, bool an_add_to_fee = true, CryptoNote::AccountPublicAddress an_addr_for_dust = CryptoNote::AccountPublicAddress())
    : dustThreshold(a_dust_threshold), addToFee(an_add_to_fee), addrForDust(an_addr_for_dust) {}
};

struct SendTransactionContext
{
  TransactionId transactionId;
  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> outs;
  uint64_t foundMoney;
  std::list<TransactionOutputInformation> selectedTransfers;
  TxDustPolicy dustPolicy;
  uint64_t mixIn;
};

} //namespace CryptoNote
