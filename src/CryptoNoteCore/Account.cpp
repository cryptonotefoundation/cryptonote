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

#include "Account.h"
#include "CryptoNoteSerialization.h"
#include "crypto/crypto.h"
extern "C"
{
#include "crypto/keccak.h"
}

namespace CryptoNote {
//-----------------------------------------------------------------
AccountBase::AccountBase() {
  setNull();
}
//-----------------------------------------------------------------
void AccountBase::setNull() {
  m_keys = AccountKeys();
}
//-----------------------------------------------------------------
void AccountBase::generate() {

  Crypto::generate_keys(m_keys.address.spendPublicKey, m_keys.spendSecretKey);
  Crypto::generate_keys(m_keys.address.viewPublicKey, m_keys.viewSecretKey);

  m_creation_timestamp = time(NULL);

}

//-----------------------------------------------------------------
void AccountBase::generateDeterministic() { 
  Crypto::SecretKey second;
  Crypto::generate_keys(m_keys.address.spendPublicKey, m_keys.spendSecretKey);
  keccak((uint8_t *)&m_keys.spendSecretKey, sizeof(Crypto::SecretKey), (uint8_t *)&second, sizeof(Crypto::SecretKey));
  Crypto::generate_deterministic_keys(m_keys.address.viewPublicKey, m_keys.viewSecretKey, second);
  m_creation_timestamp = time(NULL);
}

//-----------------------------------------------------------------
void AccountBase::generateViewFromSpend(const Crypto::SecretKey &spendSecret, Crypto::SecretKey &viewSecret, Crypto::PublicKey &viewPublic) {
  Crypto::SecretKey viewKeySeed;
  keccak((uint8_t *)&spendSecret, sizeof(spendSecret), (uint8_t *)&viewKeySeed, sizeof(viewKeySeed));

  Crypto::generate_deterministic_keys(viewPublic, viewSecret, viewKeySeed);
}

void AccountBase::generateViewFromSpend(const Crypto::SecretKey &spendSecret, Crypto::SecretKey &viewSecret) {
  /* If we don't need the pub key */
  Crypto::PublicKey unused_dummy_variable;
  generateViewFromSpend(spendSecret, viewSecret, unused_dummy_variable);
}

//-----------------------------------------------------------------
const AccountKeys &AccountBase::getAccountKeys() const {
  return m_keys;
}

void AccountBase::setAccountKeys(const AccountKeys &keys) {
  m_keys = keys;
}
//-----------------------------------------------------------------

void AccountBase::serialize(ISerializer &s) {
  s(m_keys, "m_keys");
  s(m_creation_timestamp, "m_creation_timestamp");
}
}
