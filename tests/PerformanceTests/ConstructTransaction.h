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
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

#include "MultiTransactionTestBase.h"

template<size_t a_in_count, size_t a_out_count>
class test_construct_tx : private multi_tx_test_base<a_in_count>
{
  static_assert(0 < a_in_count, "in_count must be greater than 0");
  static_assert(0 < a_out_count, "out_count must be greater than 0");

public:
  static const size_t loop_count = (a_in_count + a_out_count < 100) ? 100 : 10;
  static const size_t in_count  = a_in_count;
  static const size_t out_count = a_out_count;

  typedef multi_tx_test_base<a_in_count> base_class;

  bool init()
  {
    using namespace CryptoNote;

    if (!base_class::init())
      return false;

    m_alice.generate();

    for (size_t i = 0; i < out_count; ++i)
    {
      m_destinations.push_back(TransactionDestinationEntry(this->m_source_amount / out_count, m_alice.getAccountKeys().address));
    }

    return true;
  }

  bool test()
  {
    return CryptoNote::constructTransaction(this->m_miners[this->real_source_idx].getAccountKeys(), this->m_sources, m_destinations, std::vector<uint8_t>(), m_tx, 0, this->m_logger);
  }

private:
  CryptoNote::AccountBase m_alice;
  std::vector<CryptoNote::TransactionDestinationEntry> m_destinations;
  CryptoNote::Transaction m_tx;
};
