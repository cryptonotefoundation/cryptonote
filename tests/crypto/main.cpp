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

#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "crypto-tests.h"
#include "../Io.h"

using namespace std;
typedef Crypto::Hash chash;

bool operator !=(const Crypto::EllipticCurveScalar &a, const Crypto::EllipticCurveScalar &b) {
  return 0 != memcmp(&a, &b, sizeof(Crypto::EllipticCurveScalar));
}

bool operator !=(const Crypto::EllipticCurvePoint &a, const Crypto::EllipticCurvePoint &b) {
  return 0 != memcmp(&a, &b, sizeof(Crypto::EllipticCurvePoint));
}

bool operator !=(const Crypto::KeyDerivation &a, const Crypto::KeyDerivation &b) {
  return 0 != memcmp(&a, &b, sizeof(Crypto::KeyDerivation));
}

int main(int argc, char *argv[]) {
  fstream input;
  string cmd;
  size_t test = 0;
  bool error = false;
  setup_random();
  if (argc != 2) {
    cerr << "invalid arguments" << endl;
    return 1;
  }
  input.open(argv[1], ios_base::in);
  for (;;) {
    ++test;
    input.exceptions(ios_base::badbit);
    if (!(input >> cmd)) {
      break;
    }
    input.exceptions(ios_base::badbit | ios_base::failbit | ios_base::eofbit);
    if (cmd == "check_scalar") {
      Crypto::EllipticCurveScalar scalar;
      bool expected, actual;
      get(input, scalar, expected);
      actual = check_scalar(scalar);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "random_scalar") {
      Crypto::EllipticCurveScalar expected, actual;
      get(input, expected);
      random_scalar(actual);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "hash_to_scalar") {
      vector<char> data;
      Crypto::EllipticCurveScalar expected, actual;
      get(input, data, expected);
      hash_to_scalar(data.data(), data.size(), actual);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "generate_keys") {
      Crypto::PublicKey expected1, actual1;
      Crypto::SecretKey expected2, actual2;
      get(input, expected1, expected2);
      generate_keys(actual1, actual2);
      if (expected1 != actual1 || expected2 != actual2) {
        goto error;
      }
    } else if (cmd == "check_key") {
      Crypto::PublicKey key;
      bool expected, actual;
      get(input, key, expected);
      actual = check_key(key);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "secret_key_to_public_key") {
      Crypto::SecretKey sec;
      bool expected1, actual1;
      Crypto::PublicKey expected2, actual2;
      get(input, sec, expected1);
      if (expected1) {
        get(input, expected2);
      }
      actual1 = secret_key_to_public_key(sec, actual2);
      if (expected1 != actual1 || (expected1 && expected2 != actual2)) {
        goto error;
      }
    } else if (cmd == "generate_key_derivation") {
      Crypto::PublicKey key1;
      Crypto::SecretKey key2;
      bool expected1, actual1;
      Crypto::KeyDerivation expected2, actual2;
      get(input, key1, key2, expected1);
      if (expected1) {
        get(input, expected2);
      }
      actual1 = generate_key_derivation(key1, key2, actual2);
      if (expected1 != actual1 || (expected1 && expected2 != actual2)) {
        goto error;
      }
    } else if (cmd == "derive_public_key") {
      Crypto::KeyDerivation derivation;
      size_t output_index;
      Crypto::PublicKey base;
      bool expected1, actual1;
      Crypto::PublicKey expected2, actual2;
      get(input, derivation, output_index, base, expected1);
      if (expected1) {
        get(input, expected2);
      }
      actual1 = derive_public_key(derivation, output_index, base, actual2);
      if (expected1 != actual1 || (expected1 && expected2 != actual2)) {
        goto error;
      }
    } else if (cmd == "derive_secret_key") {
      Crypto::KeyDerivation derivation;
      size_t output_index;
      Crypto::SecretKey base;
      Crypto::SecretKey expected, actual;
      get(input, derivation, output_index, base, expected);
      derive_secret_key(derivation, output_index, base, actual);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "underive_public_key") {
      Crypto::KeyDerivation derivation;
      size_t output_index;
      Crypto::PublicKey derived_key;
      bool expected1, actual1;
      Crypto::PublicKey expected2, actual2;
      get(input, derivation, output_index, derived_key, expected1);
      if (expected1) {
        get(input, expected2);
      }
      actual1 = underive_public_key(derivation, output_index, derived_key, actual2);
      if (expected1 != actual1 || (expected1 && expected2 != actual2)) {
        goto error;
      }
    } else if (cmd == "generate_signature") {
      chash prefix_hash;
      Crypto::PublicKey pub;
      Crypto::SecretKey sec;
      Crypto::Signature expected, actual;
      get(input, prefix_hash, pub, sec, expected);
      generate_signature(prefix_hash, pub, sec, actual);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "check_signature") {
      chash prefix_hash;
      Crypto::PublicKey pub;
      Crypto::Signature sig;
      bool expected, actual;
      get(input, prefix_hash, pub, sig, expected);
      actual = check_signature(prefix_hash, pub, sig);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "hash_to_point") {
      chash h;
      Crypto::EllipticCurvePoint expected, actual;
      get(input, h, expected);
      hash_to_point(h, actual);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "hash_to_ec") {
      Crypto::PublicKey key;
      Crypto::EllipticCurvePoint expected, actual;
      get(input, key, expected);
      hash_to_ec(key, actual);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "generate_key_image") {
      Crypto::PublicKey pub;
      Crypto::SecretKey sec;
      Crypto::KeyImage expected, actual;
      get(input, pub, sec, expected);
      generate_key_image(pub, sec, actual);
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "generate_ring_signature") {
      chash prefix_hash;
      Crypto::KeyImage image;
      vector<Crypto::PublicKey> vpubs;
      vector<const Crypto::PublicKey *> pubs;
      size_t pubs_count;
      Crypto::SecretKey sec;
      size_t sec_index;
      vector<Crypto::Signature> expected, actual;
      size_t i;
      get(input, prefix_hash, image, pubs_count);
      vpubs.resize(pubs_count);
      pubs.resize(pubs_count);
      for (i = 0; i < pubs_count; i++) {
        get(input, vpubs[i]);
        pubs[i] = &vpubs[i];
      }
      get(input, sec, sec_index);
      expected.resize(pubs_count);
      getvar(input, pubs_count * sizeof(Crypto::Signature), expected.data());
      actual.resize(pubs_count);
      generate_ring_signature(prefix_hash, image, pubs.data(), pubs_count, sec, sec_index, actual.data());
      if (expected != actual) {
        goto error;
      }
    } else if (cmd == "check_ring_signature") {
      chash prefix_hash;
      Crypto::KeyImage image;
      vector<Crypto::PublicKey> vpubs;
      vector<const Crypto::PublicKey *> pubs;
      size_t pubs_count;
      vector<Crypto::Signature> sigs;
      bool expected, actual;
      size_t i;
      get(input, prefix_hash, image, pubs_count);
      vpubs.resize(pubs_count);
      pubs.resize(pubs_count);
      for (i = 0; i < pubs_count; i++) {
        get(input, vpubs[i]);
        pubs[i] = &vpubs[i];
      }
      sigs.resize(pubs_count);
      getvar(input, pubs_count * sizeof(Crypto::Signature), sigs.data());
      get(input, expected);
      actual = check_ring_signature(prefix_hash, image, pubs.data(), pubs_count, sigs.data());
      if (expected != actual) {
        goto error;
      }
    } else {
      throw ios_base::failure("Unknown function: " + cmd);
    }
    continue;
error:
    cerr << "Wrong result on test " << test << endl;
    error = true;
  }
  return error ? 1 : 0;
}
