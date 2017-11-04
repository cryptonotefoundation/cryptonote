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

#include <vector>

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "crypto/crypto.h"

#include "MultiTransactionTestBase.h"

template<size_t a_ring_size>
class test_check_ring_signature : private multi_tx_test_base<a_ring_size>
{
  static_assert(0 < a_ring_size, "ring_size must be greater than 0");

public:
  static const size_t loop_count = a_ring_size < 100 ? 100 : 10;
  static const size_t ring_size = a_ring_size;

  typedef multi_tx_test_base<a_ring_size> base_class;

  bool init()
  {
    using namespace CryptoNote;

    if (!base_class::init())
      return false;

    m_alice.generate();

    std::vector<TransactionDestinationEntry> destinations;
    destinations.push_back(TransactionDestinationEntry(this->m_source_amount, m_alice.getAccountKeys().address));

    if (!constructTransaction(this->m_miners[this->real_source_idx].getAccountKeys(), this->m_sources, destinations, std::vector<uint8_t>(), m_tx, 0, this->m_logger))
      return false;

    getObjectHash(*static_cast<TransactionPrefix*>(&m_tx), m_tx_prefix_hash);

    return true;
  }

  bool test()
  {
    const CryptoNote::KeyInput& txin = boost::get<CryptoNote::KeyInput>(m_tx.inputs[0]);
    return Crypto::check_ring_signature(m_tx_prefix_hash, txin.keyImage, this->m_public_key_ptrs, ring_size, m_tx.signatures[0].data());
  }

private:
  CryptoNote::AccountBase m_alice;
  CryptoNote::Transaction m_tx;
  Crypto::Hash m_tx_prefix_hash;
};
