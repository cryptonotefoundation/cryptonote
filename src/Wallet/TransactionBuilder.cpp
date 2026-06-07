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

#include "TransactionBuilder.h"

#include <algorithm>

#include <Common/BinaryArray.hpp>
#include <Common/StringTools.h>
#include "crypto/random.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "Wallet/WalletErrors.h"

namespace CryptoNote {

std::unique_ptr<ITransaction> buildTransaction(
    std::vector<TxBuildInput>& inputs,
    std::vector<TxBuildOutput>& outputs,
    const Crypto::SecretKey& viewSecretKey,
    const std::string& extra,
    uint64_t unlockTimestamp,
    uint64_t sizeLimit,
    Crypto::SecretKey& txSecretKey) {

  std::unique_ptr<ITransaction> tx = createTransaction();

  // 1. Set unlock time
  tx->setUnlockTime(unlockTimestamp);

  // 2. Add inputs — this generates key images and populates inputs[i].ephKeys
  for (auto& input : inputs) {
    tx->addInput(input.senderKeys, input.keyInfo, input.ephKeys);
  }

  // 3. Generate deterministic transaction keys (must come after inputs, before outputs).
  //    r = Hs(viewSecretKey || inputsHash). Enables tx key recovery for sending proofs.
  //    If viewSecretKey is null, keep the random keypair — original CryptoNote protocol, safe.
  if (viewSecretKey != NULL_SECRET_KEY) {
    tx->generateDeterministicTransactionKeys(viewSecretKey);
  }
  // else: random tx keypair from TransactionImpl() constructor is used (safe, original protocol)

  // 4. Sort outputs ascending by amount (shuffle first for privacy, then stable sort)
  std::shuffle(outputs.begin(), outputs.end(), Random::generator());
  std::sort(outputs.begin(), outputs.end(), [](const TxBuildOutput& a, const TxBuildOutput& b) {
    return a.amount < b.amount;
  });

  for (const auto& out : outputs) {
    tx->addOutput(out.amount, out.destination);
  }

  // 5. Append extra data (payment ID, etc.)
  if (!extra.empty()) {
    tx->appendExtra(Common::asBinaryArray(extra));
  }

  // 6. Sign each input
  size_t i = 0;
  for (const auto& input : inputs) {
    tx->signInputKey(i++, input.keyInfo, input.ephKeys);
  }

  // 7. Enforce size limit if requested
  if (sizeLimit > 0) {
    size_t txSize = tx->getTransactionData().size();
    if (txSize >= sizeLimit) {
      throw std::system_error(make_error_code(error::TRANSACTION_SIZE_TOO_BIG));
    }
  }

  // 8. Extract and return the deterministic secret key
  tx->getTransactionSecretKey(txSecretKey);

  return tx;
}

} // namespace CryptoNote
