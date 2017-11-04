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
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"

class TransactionBuilder {
public:

  typedef std::vector<CryptoNote::AccountKeys> KeysVector;
  typedef std::vector<Crypto::Signature> SignatureVector;
  typedef std::vector<SignatureVector> SignatureMultivector;

  struct MultisignatureSource {
    CryptoNote::MultisignatureInput input;
    KeysVector keys;
    Crypto::PublicKey srcTxPubKey;
    size_t srcOutputIndex;
  };

  TransactionBuilder(const CryptoNote::Currency& currency, uint64_t unlockTime = 0);

  // regenerate transaction keys
  TransactionBuilder& newTxKeys();
  TransactionBuilder& setTxKeys(const CryptoNote::KeyPair& txKeys);

  // inputs
  TransactionBuilder& setInput(const std::vector<CryptoNote::TransactionSourceEntry>& sources, const CryptoNote::AccountKeys& senderKeys);
  TransactionBuilder& addMultisignatureInput(const MultisignatureSource& source);

  // outputs
  TransactionBuilder& setOutput(const std::vector<CryptoNote::TransactionDestinationEntry>& destinations);
  TransactionBuilder& addOutput(const CryptoNote::TransactionDestinationEntry& dest);
  TransactionBuilder& addMultisignatureOut(uint64_t amount, const KeysVector& keys, uint32_t required);

  CryptoNote::Transaction build() const;

  std::vector<CryptoNote::TransactionSourceEntry> m_sources;
  std::vector<CryptoNote::TransactionDestinationEntry> m_destinations;

private:

  void fillInputs(CryptoNote::Transaction& tx, std::vector<CryptoNote::KeyPair>& contexts) const;
  void fillOutputs(CryptoNote::Transaction& tx) const;
  void signSources(const Crypto::Hash& prefixHash, const std::vector<CryptoNote::KeyPair>& contexts, CryptoNote::Transaction& tx) const;

  struct MultisignatureDestination {
    uint64_t amount;
    uint32_t requiredSignatures;
    KeysVector keys;
  };

  CryptoNote::AccountKeys m_senderKeys;

  std::vector<MultisignatureSource> m_msigSources;
  std::vector<MultisignatureDestination> m_msigDestinations;

  size_t m_version;
  uint64_t m_unlockTime;
  CryptoNote::KeyPair m_txKey;
  const CryptoNote::Currency& m_currency;
};
