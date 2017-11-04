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

class test_generate_key_image : public single_tx_test_base
{
public:
  static const size_t loop_count = 1000;

  bool init()
  {
    using namespace CryptoNote;

    if (!single_tx_test_base::init())
      return false;

    AccountKeys bob_keys = m_bob.getAccountKeys();

    Crypto::KeyDerivation recv_derivation;
    Crypto::generate_key_derivation(m_tx_pub_key, bob_keys.viewSecretKey, recv_derivation);

    Crypto::derive_public_key(recv_derivation, 0, bob_keys.address.spendPublicKey, m_in_ephemeral.publicKey);
    Crypto::derive_secret_key(recv_derivation, 0, bob_keys.spendSecretKey, m_in_ephemeral.secretKey);

    return true;
  }

  bool test()
  {
    Crypto::KeyImage ki;
    Crypto::generate_key_image(m_in_ephemeral.publicKey, m_in_ephemeral.secretKey, ki);
    return true;
  }

private:
  CryptoNote::KeyPair m_in_ephemeral;
};
