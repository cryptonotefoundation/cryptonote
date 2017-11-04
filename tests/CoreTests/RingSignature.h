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
#include "Chaingen.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_ring_signature_1 : public test_chain_unit_base
{
public:
  gen_ring_signature_1();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_balances_1(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_balances_2(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  CryptoNote::AccountBase m_bob_account;
  CryptoNote::AccountBase m_alice_account;
};


/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_ring_signature_2 : public test_chain_unit_base
{
public:
  gen_ring_signature_2();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_balances_1(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_balances_2(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  CryptoNote::AccountBase m_bob_account;
  CryptoNote::AccountBase m_alice_account;
};


/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_ring_signature_big : public test_chain_unit_base
{
public:
  gen_ring_signature_big();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_balances_1(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_balances_2(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  size_t m_test_size;
  uint64_t m_tx_amount;

  CryptoNote::AccountBase m_bob_account;
  CryptoNote::AccountBase m_alice_account;
};
