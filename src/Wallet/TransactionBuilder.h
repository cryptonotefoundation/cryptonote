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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CryptoNote.h"
#include "ITransaction.h"
#include "CryptoNoteCore/Account.h"

namespace CryptoNote {

// Input descriptor: holds resolved mixin outputs + sender keys needed to generate key image and sign.
// ephKeys is populated by buildTransaction() during addInput.
struct TxBuildInput {
  TransactionTypes::InputKeyInfo keyInfo;
  AccountKeys senderKeys;
  KeyPair ephKeys;
};

// Output descriptor: a single decomposed amount to a destination address.
// Callers decompose amounts (digitSplitStrategy) before calling buildTransaction().
struct TxBuildOutput {
  AccountPublicAddress destination;
  uint64_t amount;
};

// Build and sign a complete transaction using the ITransaction interface.
//
// Construction order (preserves deterministic key correctness):
//   1. setUnlockTime
//   2. addInput x N  (generates key images, stores ephKeys back into inputs[i].ephKeys)
//   3. generateDeterministicTransactionKeys(viewSecretKey)  [ITransaction method]
//   4. addOutput x N — sorted ascending by amount (derivation uses deterministic tx key)
//   5. appendExtra
//   6. signInputKey x N
//
// viewSecretKey: wallet view secret key used for deterministic tx key generation.
//           If NULL_SECRET_KEY (view-only wallet without view key), uses random keypair.
// sizeLimit: throws TRANSACTION_SIZE_TOO_BIG if the serialized size exceeds the limit.
//            Pass 0 to disable the check.
// txSecretKey: [out] the deterministic secret key, for sending-proof storage.
std::unique_ptr<ITransaction> buildTransaction(
    std::vector<TxBuildInput>& inputs,
    std::vector<TxBuildOutput>& outputs,
    const Crypto::SecretKey& viewSecretKey,
    const std::string& extra,
    uint64_t unlockTimestamp,
    uint64_t sizeLimit,
    Crypto::SecretKey& txSecretKey);

} // namespace CryptoNote
