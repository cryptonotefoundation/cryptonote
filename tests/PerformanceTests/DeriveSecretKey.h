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

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"

#include "SingleTransactionTestBase.h"

class test_derive_secret_key : public single_tx_test_base
{
public:
  static const size_t loop_count = 1000000;

  bool init()
  {
    if (!single_tx_test_base::init())
      return false;

    Crypto::generate_key_derivation(m_tx_pub_key, m_bob.getAccountKeys().viewSecretKey, m_key_derivation);
    m_spend_secret_key = m_bob.getAccountKeys().spendSecretKey;

    return true;
  }

  bool test()
  {
    CryptoNote::KeyPair in_ephemeral;
    Crypto::derive_secret_key(m_key_derivation, 0, m_spend_secret_key, in_ephemeral.secretKey);
    return true;
  }

private:
  Crypto::KeyDerivation m_key_derivation;
  Crypto::SecretKey m_spend_secret_key;
};
