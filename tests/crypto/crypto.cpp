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

#include "crypto/crypto.cpp"

#include "crypto-tests.h"

bool check_scalar(const Crypto::EllipticCurveScalar &scalar) {
  return Crypto::sc_check(reinterpret_cast<const unsigned char*>(&scalar)) == 0;
}

void random_scalar(Crypto::EllipticCurveScalar &res) {
  Crypto::random_scalar(res);
}

void hash_to_scalar(const void *data, size_t length, Crypto::EllipticCurveScalar &res) {
  Crypto::hash_to_scalar(data, length, res);
}

void hash_to_point(const Crypto::Hash &h, Crypto::EllipticCurvePoint &res) {
  Crypto::ge_p2 point;
  Crypto::ge_fromfe_frombytes_vartime(&point, reinterpret_cast<const unsigned char *>(&h));
  Crypto::ge_tobytes(reinterpret_cast<unsigned char*>(&res), &point);
}

void hash_to_ec(const Crypto::PublicKey &key, Crypto::EllipticCurvePoint &res) {
  Crypto::ge_p3 tmp;
  Crypto::hash_to_ec(key, tmp);
  Crypto::ge_p3_tobytes(reinterpret_cast<unsigned char*>(&res), &tmp);
}
