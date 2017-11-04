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

#include <cstddef>
#include <limits>
#include <mutex>
#include <type_traits>
#include <vector>

#include <CryptoTypes.h>

#include "generic-ops.h"
#include "hash.h"

namespace Crypto {

  extern "C" {
#include "random.h"
  }

  extern std::mutex random_lock;

struct EllipticCurvePoint {
  uint8_t data[32];
};

struct EllipticCurveScalar {
  uint8_t data[32];
};

  class crypto_ops {
    crypto_ops();
    crypto_ops(const crypto_ops &);
    void operator=(const crypto_ops &);
    ~crypto_ops();

    static void generate_keys(PublicKey &, SecretKey &);
    friend void generate_keys(PublicKey &, SecretKey &);
    static bool check_key(const PublicKey &);
    friend bool check_key(const PublicKey &);
    static bool secret_key_to_public_key(const SecretKey &, PublicKey &);
    friend bool secret_key_to_public_key(const SecretKey &, PublicKey &);
    static bool generate_key_derivation(const PublicKey &, const SecretKey &, KeyDerivation &);
    friend bool generate_key_derivation(const PublicKey &, const SecretKey &, KeyDerivation &);
    static bool derive_public_key(const KeyDerivation &, size_t, const PublicKey &, PublicKey &);
    friend bool derive_public_key(const KeyDerivation &, size_t, const PublicKey &, PublicKey &);
    friend bool derive_public_key(const KeyDerivation &, size_t, const PublicKey &, const uint8_t*, size_t, PublicKey &);
    static bool derive_public_key(const KeyDerivation &, size_t, const PublicKey &, const uint8_t*, size_t, PublicKey &);
    //hack for pg
    static bool underive_public_key_and_get_scalar(const KeyDerivation &, std::size_t, const PublicKey &, PublicKey &, EllipticCurveScalar &);
    friend bool underive_public_key_and_get_scalar(const KeyDerivation &, std::size_t, const PublicKey &, PublicKey &, EllipticCurveScalar &);
    static void generate_incomplete_key_image(const PublicKey &, EllipticCurvePoint &);
    friend void generate_incomplete_key_image(const PublicKey &, EllipticCurvePoint &);
    //
    static void derive_secret_key(const KeyDerivation &, size_t, const SecretKey &, SecretKey &);
    friend void derive_secret_key(const KeyDerivation &, size_t, const SecretKey &, SecretKey &);
    static void derive_secret_key(const KeyDerivation &, size_t, const SecretKey &, const uint8_t*, size_t, SecretKey &);
    friend void derive_secret_key(const KeyDerivation &, size_t, const SecretKey &, const uint8_t*, size_t, SecretKey &);
    static bool underive_public_key(const KeyDerivation &, size_t, const PublicKey &, PublicKey &);
    friend bool underive_public_key(const KeyDerivation &, size_t, const PublicKey &, PublicKey &);
    static bool underive_public_key(const KeyDerivation &, size_t, const PublicKey &, const uint8_t*, size_t, PublicKey &);
    friend bool underive_public_key(const KeyDerivation &, size_t, const PublicKey &, const uint8_t*, size_t, PublicKey &);
    static void generate_signature(const Hash &, const PublicKey &, const SecretKey &, Signature &);
    friend void generate_signature(const Hash &, const PublicKey &, const SecretKey &, Signature &);
    static bool check_signature(const Hash &, const PublicKey &, const Signature &);
    friend bool check_signature(const Hash &, const PublicKey &, const Signature &);
    static void generate_key_image(const PublicKey &, const SecretKey &, KeyImage &);
    friend void generate_key_image(const PublicKey &, const SecretKey &, KeyImage &);
    static KeyImage scalarmultKey(const KeyImage & P, const KeyImage & a);
    friend KeyImage scalarmultKey(const KeyImage & P, const KeyImage & a);
    static void hash_data_to_ec(const uint8_t*, std::size_t, PublicKey&);
    friend void hash_data_to_ec(const uint8_t*, std::size_t, PublicKey&);
    static void generate_ring_signature(const Hash &, const KeyImage &,
      const PublicKey *const *, size_t, const SecretKey &, size_t, Signature *);
    friend void generate_ring_signature(const Hash &, const KeyImage &,
      const PublicKey *const *, size_t, const SecretKey &, size_t, Signature *);
    static bool check_ring_signature(const Hash &, const KeyImage &,
      const PublicKey *const *, size_t, const Signature *);
    friend bool check_ring_signature(const Hash &, const KeyImage &,
      const PublicKey *const *, size_t, const Signature *);
  };

  /* Generate a value filled with random bytes.
   */
  template<typename T>
  typename std::enable_if<std::is_pod<T>::value, T>::type rand() {
    typename std::remove_cv<T>::type res;
    std::lock_guard<std::mutex> lock(random_lock);
    generate_random_bytes(sizeof(T), &res);
    return res;
  }

  /* Random number engine based on Crypto::rand()
   */
  template <typename T>
  class random_engine {
  public:
    typedef T result_type;

#ifdef __clang__
    constexpr static T min() {
      return (std::numeric_limits<T>::min)();
    }

    constexpr static T max() {
      return (std::numeric_limits<T>::max)();
    }
#else
    static T(min)() {
      return (std::numeric_limits<T>::min)();
    }

    static T(max)() {
      return (std::numeric_limits<T>::max)();
    }
#endif
    typename std::enable_if<std::is_unsigned<T>::value, T>::type operator()() {
      return rand<T>();
    }
  };

  /* Generate a new key pair
   */
  inline void generate_keys(PublicKey &pub, SecretKey &sec) {
    crypto_ops::generate_keys(pub, sec);
  }

  /* Check a public key. Returns true if it is valid, false otherwise.
   */
  inline bool check_key(const PublicKey &key) {
    return crypto_ops::check_key(key);
  }

  /* Checks a private key and computes the corresponding public key.
   */
  inline bool secret_key_to_public_key(const SecretKey &sec, PublicKey &pub) {
    return crypto_ops::secret_key_to_public_key(sec, pub);
  }

  /* To generate an ephemeral key used to send money to:
   * * The sender generates a new key pair, which becomes the transaction key. The public transaction key is included in "extra" field.
   * * Both the sender and the receiver generate key derivation from the transaction key and the receivers' "view" key.
   * * The sender uses key derivation, the output index, and the receivers' "spend" key to derive an ephemeral public key.
   * * The receiver can either derive the public key (to check that the transaction is addressed to him) or the private key (to spend the money).
   */
  inline bool generate_key_derivation(const PublicKey &key1, const SecretKey &key2, KeyDerivation &derivation) {
    return crypto_ops::generate_key_derivation(key1, key2, derivation);
  }

  inline bool derive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &base, const uint8_t* prefix, size_t prefixLength, PublicKey &derived_key) {
    return crypto_ops::derive_public_key(derivation, output_index, base, prefix, prefixLength, derived_key);
  }

  inline bool derive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &base, PublicKey &derived_key) {
    return crypto_ops::derive_public_key(derivation, output_index, base, derived_key);
  }


  inline bool underive_public_key_and_get_scalar(const KeyDerivation &derivation, std::size_t output_index,
    const PublicKey &derived_key, PublicKey &base, EllipticCurveScalar &hashed_derivation) {
    return crypto_ops::underive_public_key_and_get_scalar(derivation, output_index, derived_key, base, hashed_derivation);
  }
  
  inline void derive_secret_key(const KeyDerivation &derivation, std::size_t output_index,
    const SecretKey &base, const uint8_t* prefix, size_t prefixLength, SecretKey &derived_key) {
    crypto_ops::derive_secret_key(derivation, output_index, base, prefix, prefixLength, derived_key);
  }

  inline void derive_secret_key(const KeyDerivation &derivation, std::size_t output_index,
    const SecretKey &base, SecretKey &derived_key) {
    crypto_ops::derive_secret_key(derivation, output_index, base, derived_key);
  }


  /* Inverse function of derive_public_key. It can be used by the receiver to find which "spend" key was used to generate a transaction. This may be useful if the receiver used multiple addresses which only differ in "spend" key.
   */
  inline bool underive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &derived_key, const uint8_t* prefix, size_t prefixLength, PublicKey &base) {
    return crypto_ops::underive_public_key(derivation, output_index, derived_key, prefix, prefixLength, base);
  }

  inline bool underive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &derived_key, PublicKey &base) {
    return crypto_ops::underive_public_key(derivation, output_index, derived_key, base);
  }

  /* Generation and checking of a standard signature.
   */
  inline void generate_signature(const Hash &prefix_hash, const PublicKey &pub, const SecretKey &sec, Signature &sig) {
    crypto_ops::generate_signature(prefix_hash, pub, sec, sig);
  }
  inline bool check_signature(const Hash &prefix_hash, const PublicKey &pub, const Signature &sig) {
    return crypto_ops::check_signature(prefix_hash, pub, sig);
  }

  /* To send money to a key:
   * * The sender generates an ephemeral key and includes it in transaction output.
   * * To spend the money, the receiver generates a key image from it.
   * * Then he selects a bunch of outputs, including the one he spends, and uses them to generate a ring signature.
   * To check the signature, it is necessary to collect all the keys that were used to generate it. To detect double spends, it is necessary to check that each key image is used at most once.
   */
  inline void generate_key_image(const PublicKey &pub, const SecretKey &sec, KeyImage &image) {
    crypto_ops::generate_key_image(pub, sec, image);
  }

  inline KeyImage scalarmultKey(const KeyImage & P, const KeyImage & a) {
    return crypto_ops::scalarmultKey(P, a);
  }

  inline void hash_data_to_ec(const uint8_t* data, std::size_t len, PublicKey& key) {
    crypto_ops::hash_data_to_ec(data, len, key);
  }

  inline void generate_ring_signature(const Hash &prefix_hash, const KeyImage &image,
    const PublicKey *const *pubs, std::size_t pubs_count,
    const SecretKey &sec, std::size_t sec_index,
    Signature *sig) {
    crypto_ops::generate_ring_signature(prefix_hash, image, pubs, pubs_count, sec, sec_index, sig);
  }
  inline bool check_ring_signature(const Hash &prefix_hash, const KeyImage &image,
    const PublicKey *const *pubs, size_t pubs_count,
    const Signature *sig) {
    return crypto_ops::check_ring_signature(prefix_hash, image, pubs, pubs_count, sig);
  }

  /* Variants with vector<const PublicKey *> parameters.
   */
  inline void generate_ring_signature(const Hash &prefix_hash, const KeyImage &image,
    const std::vector<const PublicKey *> &pubs,
    const SecretKey &sec, size_t sec_index,
    Signature *sig) {
    generate_ring_signature(prefix_hash, image, pubs.data(), pubs.size(), sec, sec_index, sig);
  }
  inline bool check_ring_signature(const Hash &prefix_hash, const KeyImage &image,
    const std::vector<const PublicKey *> &pubs,
    const Signature *sig) {
    return check_ring_signature(prefix_hash, image, pubs.data(), pubs.size(), sig);
  }

}

CRYPTO_MAKE_HASHABLE(PublicKey)
CRYPTO_MAKE_HASHABLE(KeyImage)
CRYPTO_MAKE_COMPARABLE(Signature)
CRYPTO_MAKE_COMPARABLE(SecretKey)
