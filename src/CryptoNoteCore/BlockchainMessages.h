// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include <vector>

#include <CryptoNote.h>

namespace CryptoNote {

class NewBlockMessage {
public:
  NewBlockMessage(const Crypto::Hash& hash);
  NewBlockMessage() = default;
  void get(Crypto::Hash& hash) const;
private:
  Crypto::Hash blockHash;
};

class NewAlternativeBlockMessage {
public:
  NewAlternativeBlockMessage(const Crypto::Hash& hash);
  NewAlternativeBlockMessage() = default;
  void get(Crypto::Hash& hash) const;
private:
  Crypto::Hash blockHash;
};

class ChainSwitchMessage {
public:
  ChainSwitchMessage(std::vector<Crypto::Hash>&& hashes);
  ChainSwitchMessage(const ChainSwitchMessage& other);
  void get(std::vector<Crypto::Hash>& hashes) const;
private:
  std::vector<Crypto::Hash> blocksFromCommonRoot;
};

class BlockchainMessage {
public:
  enum class MessageType {
    NEW_BLOCK_MESSAGE,
    NEW_ALTERNATIVE_BLOCK_MESSAGE,
    CHAIN_SWITCH_MESSAGE
  };

  BlockchainMessage(NewBlockMessage&& message);
  BlockchainMessage(NewAlternativeBlockMessage&& message);
  BlockchainMessage(ChainSwitchMessage&& message);

  BlockchainMessage(const BlockchainMessage& other);

  ~BlockchainMessage();

  MessageType getType() const;

  bool getNewBlockHash(Crypto::Hash& hash) const;
  bool getNewAlternativeBlockHash(Crypto::Hash& hash) const;
  bool getChainSwitch(std::vector<Crypto::Hash>& hashes) const;
private:
  const MessageType type;

  union {
    NewBlockMessage newBlockMessage;
    NewAlternativeBlockMessage newAlternativeBlockMessage;
    ChainSwitchMessage* chainSwitchMessage;
  };
};

}
