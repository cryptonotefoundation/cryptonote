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

#include <cstring>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "crypto/random.h"

extern "C" {
#include "crypto/crypto-ops.h"
}

#include "crypto-tests.h"

bool check_scalar(const Crypto::EllipticCurveScalar &scalar) {
  return sc_check(reinterpret_cast<const unsigned char*>(&scalar)) == 0;
}

void random_scalar(Crypto::EllipticCurveScalar &res) {
  uint8_t tmp[64];
  Random::randomBytes(sizeof(tmp), tmp);
  sc_reduce(tmp);
  std::memcpy(&res, tmp, sizeof(res));
}

void hash_to_scalar(const void *data, size_t length, Crypto::EllipticCurveScalar &res) {
  Crypto::hash_to_scalar(data, length, res);
}

void hash_to_point(const Crypto::Hash &h, Crypto::EllipticCurvePoint &res) {
  ge_p2 point;
  ge_fromfe_frombytes_vartime(&point, reinterpret_cast<const unsigned char *>(&h));
  ge_tobytes(reinterpret_cast<unsigned char*>(&res), &point);
}

void hash_to_ec(const Crypto::PublicKey &key, Crypto::EllipticCurvePoint &res) {
  Crypto::Hash h;
  ge_p2 point;
  ge_p1p1 point2;
  ge_p3 tmp;
  Crypto::cn_fast_hash(&key, sizeof(Crypto::PublicKey), h);
  ge_fromfe_frombytes_vartime(&point, reinterpret_cast<const unsigned char *>(&h));
  ge_mul8(&point2, &point);
  ge_p1p1_to_p3(&tmp, &point2);
  ge_p3_tobytes(reinterpret_cast<unsigned char*>(&res), &tmp);
}
