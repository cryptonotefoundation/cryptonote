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

#include <functional>
#include <vector>

#include <CryptoNote.h>

namespace CryptoNote {

namespace Messages {
// immutable messages
struct NewBlock {
  uint32_t blockIndex;
  Crypto::Hash blockHash;
};

struct NewAlternativeBlock {
  uint32_t blockIndex;
  Crypto::Hash blockHash;
};

struct ChainSwitch {
  uint32_t commonRootIndex;
  std::vector<Crypto::Hash> blocksFromCommonRoot;
};

struct AddTransaction {
  std::vector<Crypto::Hash> hashes;
};

struct DeleteTransaction {
  std::vector<Crypto::Hash> hashes;
  enum class Reason {
    InBlock,
    Outdated,
    NotActual
  } reason;
};
}

class BlockchainMessage {
public:
  enum class Type {
    NewBlock,
    NewAlternativeBlock,
    ChainSwitch,
    AddTransaction,
    DeleteTransaction
  };

  using NewBlock = Messages::NewBlock;
  using NewAlternativeBlock = Messages::NewAlternativeBlock;
  using ChainSwitch = Messages::ChainSwitch;
  using AddTransaction = Messages::AddTransaction;
  using DeleteTransaction = Messages::DeleteTransaction;

  BlockchainMessage(const NewBlock& message);
  BlockchainMessage(const NewAlternativeBlock& message);
  BlockchainMessage(const ChainSwitch& message);
  BlockchainMessage(const AddTransaction& message);
  BlockchainMessage(const DeleteTransaction& message);

  BlockchainMessage(const BlockchainMessage& other);

  ~BlockchainMessage();

  // pattern matchin API
  void match(std::function<void(const NewBlock&)>, std::function<void(const NewAlternativeBlock&)>,
             std::function<void(const ChainSwitch&)>, std::function<void(const AddTransaction&)>,
             std::function<void(const DeleteTransaction&)>) const;

  // API with explicit type handling
  Type getType() const;
  const NewBlock& getNewBlock() const;
  const NewAlternativeBlock& getNewAlternativeBlock() const;
  const ChainSwitch& getChainSwitch() const;
  const AddTransaction& getAddTransaction() const;
  const DeleteTransaction& getDeleteTransaction() const;

private:
  const Type type;
  union {
    NewBlock newBlock;
    NewAlternativeBlock newAlternativeBlock;
    ChainSwitch* chainSwitch;
    AddTransaction* addTransaction;
    DeleteTransaction* deleteTransaction;
  };
};

// factory functions
BlockchainMessage makeChainSwitchMessage(uint32_t index, std::vector<Crypto::Hash>&& hashes);
BlockchainMessage makeNewAlternativeBlockMessage(uint32_t index, const Crypto::Hash& hash);
BlockchainMessage makeNewBlockMessage(uint32_t index, const Crypto::Hash& hash);
BlockchainMessage makeAddTransactionMessage(std::vector<Crypto::Hash>&& hash);
BlockchainMessage makeDelTransactionMessage(std::vector<Crypto::Hash>&& hash, Messages::DeleteTransaction::Reason r);
}
