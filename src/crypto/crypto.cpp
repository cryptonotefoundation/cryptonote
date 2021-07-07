// Copyright (c) 2012-2018, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2017, The Monero Project
// Copyright (c) 2016-2020, The Karbo developers
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

#ifndef __FreeBSD__
  #include <alloca.h>
#endif
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "Common/Varint.h"
#include "crypto.h"
#include "crypto-ops.h"
#include "hash.h"
#include "random.h"

namespace Crypto {

  static inline unsigned char *operator &(EllipticCurvePoint &point) {
    return &reinterpret_cast<unsigned char &>(point);
  }

  static inline const unsigned char *operator &(const EllipticCurvePoint &point) {
    return &reinterpret_cast<const unsigned char &>(point);
  }

  static inline unsigned char *operator &(EllipticCurveScalar &scalar) {
    return &reinterpret_cast<unsigned char &>(scalar);
  }

  static inline const unsigned char *operator &(const EllipticCurveScalar &scalar) {
    return &reinterpret_cast<const unsigned char &>(scalar);
  }

  static inline void random_scalar(EllipticCurveScalar &res) {
    unsigned char tmp[64];
    Random::randomBytes(64, tmp);
    sc_reduce(tmp);
    memcpy(&res, tmp, 32);
  }

  void hash_to_scalar(const void *data, size_t length, EllipticCurveScalar &res) {
    cn_fast_hash(data, length, reinterpret_cast<Hash &>(res));
    sc_reduce32(reinterpret_cast<unsigned char*>(&res));
  }

  void crypto_ops::generate_keys(PublicKey &pub, SecretKey &sec) {
    ge_p3 point;
    random_scalar(reinterpret_cast<EllipticCurveScalar&>(sec));
    ge_scalarmult_base(&point, reinterpret_cast<unsigned char*>(&sec));
    ge_p3_tobytes(reinterpret_cast<unsigned char*>(&pub), &point);
  }

  void crypto_ops::generate_deterministic_keys(PublicKey &pub, SecretKey &sec, SecretKey& second) {
    ge_p3 point;
	sec = second;
    sc_reduce32(reinterpret_cast<unsigned char*>(&sec)); // reduce in case second round of keys (sendkeys)
    ge_scalarmult_base(&point, reinterpret_cast<unsigned char*>(&sec));
    ge_p3_tobytes(reinterpret_cast<unsigned char*>(&pub), &point);
  }
 
  SecretKey crypto_ops::generate_m_keys(PublicKey &pub, SecretKey &sec, const SecretKey& recovery_key, bool recover) {
    ge_p3 point;
    SecretKey rng;
    if (recover)
    {
      rng = recovery_key;
    }
    else
    {
      random_scalar(reinterpret_cast<EllipticCurveScalar&>(rng));
    }
    sec = rng;
    sc_reduce32(reinterpret_cast<unsigned char*>(&sec)); // reduce in case second round of keys (sendkeys)
    ge_scalarmult_base(&point, reinterpret_cast<unsigned char*>(&sec));
    ge_p3_tobytes(reinterpret_cast<unsigned char*>(&pub), &point);

    return rng;
  }


  bool crypto_ops::check_key(const PublicKey &key) {
    ge_p3 point;
    return ge_frombytes_vartime(&point, reinterpret_cast<const unsigned char*>(&key)) == 0;
  }

  bool crypto_ops::secret_key_to_public_key(const SecretKey &sec, PublicKey &pub) {
    ge_p3 point;
    if (sc_check(reinterpret_cast<const unsigned char*>(&sec)) != 0) {
      return false;
    }
    ge_scalarmult_base(&point, reinterpret_cast<const unsigned char*>(&sec));
    ge_p3_tobytes(reinterpret_cast<unsigned char*>(&pub), &point);
    return true;
  }

  bool crypto_ops::secret_key_mult_public_key(const SecretKey &sec, const PublicKey &pub, PublicKey &result) {
    if (sc_check(&sec) != 0) {
      return false;
    }
    ge_p3 point;
    if (ge_frombytes_vartime(&point, &pub) != 0) {
      return false;
    }
    ge_p2 point2;
    ge_scalarmult(&point2, &sec, &point);
    ge_tobytes(reinterpret_cast<unsigned char*>(&result), &point2);
    return true;
  }

  bool crypto_ops::generate_key_derivation(const PublicKey &key1, const SecretKey &key2, KeyDerivation &derivation) {
    ge_p3 point;
    ge_p2 point2;
    ge_p1p1 point3;
    if (!(sc_check(reinterpret_cast<const unsigned char*>(&key2)) == 0)) {
      return false;
    }
    if (ge_frombytes_vartime(&point, reinterpret_cast<const unsigned char*>(&key1)) != 0) {
      return false;
    }
    ge_scalarmult(&point2, reinterpret_cast<const unsigned char*>(&key2), &point);
    ge_mul8(&point3, &point2);
    ge_p1p1_to_p2(&point2, &point3);
    ge_tobytes(reinterpret_cast<unsigned char*>(&derivation), &point2);
    return true;
  }

  static void derivation_to_scalar(const KeyDerivation &derivation, size_t output_index, EllipticCurveScalar &res) {
    struct {
      KeyDerivation derivation;
      char output_index[(sizeof(size_t) * 8 + 6) / 7];
    } buf;
    char *end = buf.output_index;
    buf.derivation = derivation;
    Tools::write_varint(end, output_index);
    assert(end <= buf.output_index + sizeof buf.output_index);
    hash_to_scalar(&buf, end - reinterpret_cast<char *>(&buf), res);
  }

  static void derivation_to_scalar(const KeyDerivation &derivation, size_t output_index, const uint8_t* suffix, size_t suffixLength, EllipticCurveScalar &res) {
    assert(suffixLength <= 32);
    struct {
      KeyDerivation derivation;
      char output_index[(sizeof(size_t) * 8 + 6) / 7 + 32];
    } buf;
    char *end = buf.output_index;
    buf.derivation = derivation;
    Tools::write_varint(end, output_index);
    assert(end <= buf.output_index + sizeof buf.output_index);
    size_t bufSize = end - reinterpret_cast<char *>(&buf);
    memcpy(end, suffix, suffixLength);
    hash_to_scalar(&buf, bufSize + suffixLength, res);
  }

  bool crypto_ops::derive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &base, PublicKey &derived_key) {
    EllipticCurveScalar scalar;
    ge_p3 point1;
    ge_p3 point2;
    ge_cached point3;
    ge_p1p1 point4;
    ge_p2 point5;
    if (ge_frombytes_vartime(&point1, reinterpret_cast<const unsigned char*>(&base)) != 0) {
      return false;
    }
    derivation_to_scalar(derivation, output_index, scalar);
    ge_scalarmult_base(&point2, reinterpret_cast<unsigned char*>(&scalar));
    ge_p3_to_cached(&point3, &point2);
    ge_add(&point4, &point1, &point3);
    ge_p1p1_to_p2(&point5, &point4);
    ge_tobytes(reinterpret_cast<unsigned char*>(&derived_key), &point5);
    return true;
  }

  bool crypto_ops::derive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &base, const uint8_t* suffix, size_t suffixLength, PublicKey &derived_key) {
    EllipticCurveScalar scalar;
    ge_p3 point1;
    ge_p3 point2;
    ge_cached point3;
    ge_p1p1 point4;
    ge_p2 point5;
    if (ge_frombytes_vartime(&point1, reinterpret_cast<const unsigned char*>(&base)) != 0) {
      return false;
    }
    derivation_to_scalar(derivation, output_index, suffix, suffixLength, scalar);
    ge_scalarmult_base(&point2, reinterpret_cast<unsigned char*>(&scalar));
    ge_p3_to_cached(&point3, &point2);
    ge_add(&point4, &point1, &point3);
    ge_p1p1_to_p2(&point5, &point4);
    ge_tobytes(reinterpret_cast<unsigned char*>(&derived_key), &point5);
    return true;
  }

  bool crypto_ops::underive_public_key_and_get_scalar(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &derived_key, PublicKey &base, EllipticCurveScalar &hashed_derivation) {
    ge_p3 point1;
    ge_p3 point2;
    ge_cached point3;
    ge_p1p1 point4;
    ge_p2 point5;
    if (ge_frombytes_vartime(&point1, reinterpret_cast<const unsigned char*>(&derived_key)) != 0) {
      return false;
    }
    derivation_to_scalar(derivation, output_index, hashed_derivation);
    ge_scalarmult_base(&point2, reinterpret_cast<unsigned char*>(&hashed_derivation));
    ge_p3_to_cached(&point3, &point2);
    ge_sub(&point4, &point1, &point3);
    ge_p1p1_to_p2(&point5, &point4);
    ge_tobytes(reinterpret_cast<unsigned char*>(&base), &point5);
    return true;
  }

  void crypto_ops::derive_secret_key(const KeyDerivation &derivation, size_t output_index,
    const SecretKey &base, SecretKey &derived_key) {
    EllipticCurveScalar scalar;
    assert(sc_check(reinterpret_cast<const unsigned char*>(&base)) == 0);
    derivation_to_scalar(derivation, output_index, scalar);
    sc_add(reinterpret_cast<unsigned char*>(&derived_key), reinterpret_cast<const unsigned char*>(&base), reinterpret_cast<unsigned char*>(&scalar));
  }

  void crypto_ops::derive_secret_key(const KeyDerivation &derivation, size_t output_index,
    const SecretKey &base, const uint8_t* suffix, size_t suffixLength, SecretKey &derived_key) {
    EllipticCurveScalar scalar;
    assert(sc_check(reinterpret_cast<const unsigned char*>(&base)) == 0);
    derivation_to_scalar(derivation, output_index, suffix, suffixLength, scalar);
    sc_add(reinterpret_cast<unsigned char*>(&derived_key), reinterpret_cast<const unsigned char*>(&base), reinterpret_cast<unsigned char*>(&scalar));
  }


  bool crypto_ops::underive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &derived_key, PublicKey &base) {
    EllipticCurveScalar scalar;
    ge_p3 point1;
    ge_p3 point2;
    ge_cached point3;
    ge_p1p1 point4;
    ge_p2 point5;
    if (ge_frombytes_vartime(&point1, reinterpret_cast<const unsigned char*>(&derived_key)) != 0) {
      return false;
    }
    derivation_to_scalar(derivation, output_index, scalar);
    ge_scalarmult_base(&point2, reinterpret_cast<unsigned char*>(&scalar));
    ge_p3_to_cached(&point3, &point2);
    ge_sub(&point4, &point1, &point3);
    ge_p1p1_to_p2(&point5, &point4);
    ge_tobytes(reinterpret_cast<unsigned char*>(&base), &point5);
    return true;
  }

  bool crypto_ops::underive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &derived_key, const uint8_t* suffix, size_t suffixLength, PublicKey &base) {
    EllipticCurveScalar scalar;
    ge_p3 point1;
    ge_p3 point2;
    ge_cached point3;
    ge_p1p1 point4;
    ge_p2 point5;
    if (ge_frombytes_vartime(&point1, reinterpret_cast<const unsigned char*>(&derived_key)) != 0) {
      return false;
    }

    derivation_to_scalar(derivation, output_index, suffix, suffixLength, scalar);
    ge_scalarmult_base(&point2, reinterpret_cast<unsigned char*>(&scalar));
    ge_p3_to_cached(&point3, &point2);
    ge_sub(&point4, &point1, &point3);
    ge_p1p1_to_p2(&point5, &point4);
    ge_tobytes(reinterpret_cast<unsigned char*>(&base), &point5);
    return true;
  }


  struct s_comm {
    Hash h;
    EllipticCurvePoint key;
    EllipticCurvePoint comm;
  };

  struct s_comm_2 {
    Hash msg;
    EllipticCurvePoint D;
    EllipticCurvePoint X;
    EllipticCurvePoint Y;
  };

  void crypto_ops::generate_signature(const Hash &prefix_hash, const PublicKey &pub, const SecretKey &sec, Signature &sig) {
    ge_p3 tmp3;
    EllipticCurveScalar k;
    s_comm buf;
#if !defined(NDEBUG)
    {
      ge_p3 t;
      PublicKey t2;
      assert(sc_check(reinterpret_cast<const unsigned char*>(&sec)) == 0);
      ge_scalarmult_base(&t, reinterpret_cast<const unsigned char*>(&sec));
      ge_p3_tobytes(reinterpret_cast<unsigned char*>(&t2), &t);
      assert(pub == t2);
    }
#endif
    buf.h = prefix_hash;
    buf.key = reinterpret_cast<const EllipticCurvePoint&>(pub);
  try_again:
    random_scalar(k);
    if (((const uint32_t*)(&k))[7] == 0) // we don't want tiny numbers here
      goto try_again;
    ge_scalarmult_base(&tmp3, reinterpret_cast<unsigned char*>(&k));
    ge_p3_tobytes(reinterpret_cast<unsigned char*>(&buf.comm), &tmp3);
    hash_to_scalar(&buf, sizeof(s_comm), reinterpret_cast<EllipticCurveScalar&>(sig));
    if (!sc_isnonzero((const unsigned char*)reinterpret_cast<EllipticCurveScalar&>(sig).data))
      goto try_again;
    sc_mulsub(reinterpret_cast<unsigned char*>(&sig) + 32, reinterpret_cast<unsigned char*>(&sig), reinterpret_cast<const unsigned char*>(&sec), reinterpret_cast<unsigned char*>(&k));
    if (!sc_isnonzero((const unsigned char*)reinterpret_cast<unsigned char*>(&sig) + 32))
      goto try_again;
  }

  bool crypto_ops::check_signature(const Hash &prefix_hash, const PublicKey &pub, const Signature &sig) {
    ge_p2 tmp2;
    ge_p3 tmp3;
    EllipticCurveScalar c;
    s_comm buf;
    assert(check_key(pub));
    buf.h = prefix_hash;
    buf.key = reinterpret_cast<const EllipticCurvePoint&>(pub);
    if (ge_frombytes_vartime(&tmp3, reinterpret_cast<const unsigned char*>(&pub)) != 0) {
      abort();
    }
    if (sc_check(reinterpret_cast<const unsigned char*>(&sig)) != 0 || sc_check(reinterpret_cast<const unsigned char*>(&sig) + 32) != 0 || !sc_isnonzero(reinterpret_cast<const unsigned char*>(&sig))) {
      return false;
    }
    ge_double_scalarmult_base_vartime(&tmp2, reinterpret_cast<const unsigned char*>(&sig), &tmp3, reinterpret_cast<const unsigned char*>(&sig) + 32);
    ge_tobytes(reinterpret_cast<unsigned char*>(&buf.comm), &tmp2);
	static const EllipticCurvePoint infinity = { { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} };
	if (memcmp(&buf.comm, &infinity, 32) == 0)
		return false;
    hash_to_scalar(&buf, sizeof(s_comm), c);
    sc_sub(reinterpret_cast<unsigned char*>(&c), reinterpret_cast<unsigned char*>(&c), reinterpret_cast<const unsigned char*>(&sig));
    return sc_isnonzero(reinterpret_cast<unsigned char*>(&c)) == 0;
  }

  void crypto_ops::generate_tx_proof(const Hash &prefix_hash, const PublicKey &R, const PublicKey &A, const PublicKey &D, const SecretKey &r, Signature &sig) {
    // sanity check
    ge_p3 R_p3;
    ge_p3 A_p3;
    ge_p3 D_p3;
    if (ge_frombytes_vartime(&R_p3, reinterpret_cast<const unsigned char*>(&R)) != 0) throw std::runtime_error("tx pubkey is invalid");
    if (ge_frombytes_vartime(&A_p3, reinterpret_cast<const unsigned char*>(&A)) != 0) throw std::runtime_error("recipient view pubkey is invalid");
    if (ge_frombytes_vartime(&D_p3, reinterpret_cast<const unsigned char*>(&D)) != 0) throw std::runtime_error("key derivation is invalid");

    assert(sc_check(reinterpret_cast<const unsigned char*>(&r)) == 0);
    // check R == r*G
    ge_p3 dbg_R_p3;
    ge_scalarmult_base(&dbg_R_p3, reinterpret_cast<const unsigned char*>(&r));
    PublicKey dbg_R;
    ge_p3_tobytes(reinterpret_cast<unsigned char*>(&dbg_R), &dbg_R_p3);
    assert(R == dbg_R);
    // check D == r*A
    ge_p2 dbg_D_p2;
    ge_scalarmult(&dbg_D_p2, reinterpret_cast<const unsigned char*>(&r), &A_p3);
    PublicKey dbg_D;
    ge_tobytes(reinterpret_cast<unsigned char*>(&dbg_D), &dbg_D_p2);
    assert(D == dbg_D);

    // pick random k
    EllipticCurveScalar k;
    random_scalar(k);

    // compute X = k*G
    ge_p3 X_p3;
    ge_scalarmult_base(&X_p3, reinterpret_cast<unsigned char*>(&k));

    // compute Y = k*A
    ge_p2 Y_p2;
    ge_scalarmult(&Y_p2, reinterpret_cast<unsigned char*>(&k), &A_p3);

    // sig.c = Hs(Msg || D || X || Y)
    s_comm_2 buf;
    buf.msg = prefix_hash;
    buf.D = reinterpret_cast<const EllipticCurvePoint&>(D);
    ge_p3_tobytes(reinterpret_cast<unsigned char*>(&buf.X), &X_p3);
    ge_tobytes(reinterpret_cast<unsigned char*>(&buf.Y), &Y_p2);
    hash_to_scalar(&buf, sizeof(s_comm_2), reinterpret_cast<EllipticCurveScalar&>(sig));

    // sig.r = k - sig.c*r
    sc_mulsub(reinterpret_cast<unsigned char*>(&sig) + 32, reinterpret_cast<unsigned char*>(&sig), reinterpret_cast<const unsigned char*>(&r), reinterpret_cast<unsigned char*>(&k));
  }

  bool crypto_ops::check_tx_proof(const Hash &prefix_hash, const PublicKey &R, const PublicKey &A, const PublicKey &D, const Signature &sig) {
    // sanity check
    ge_p3 R_p3;
    ge_p3 A_p3;
    ge_p3 D_p3;
    if (ge_frombytes_vartime(&R_p3, reinterpret_cast<const unsigned char*>(&R)) != 0) return false;
    if (ge_frombytes_vartime(&A_p3, reinterpret_cast<const unsigned char*>(&A)) != 0) return false;
    if (ge_frombytes_vartime(&D_p3, reinterpret_cast<const unsigned char*>(&D)) != 0) return false;
    if (sc_check(reinterpret_cast<const unsigned char*>(&sig)) != 0 || sc_check(reinterpret_cast<const unsigned char*>(&sig) + 32) != 0) return false;

    // compute sig.c*R
    ge_p2 cR_p2;
    ge_scalarmult(&cR_p2, reinterpret_cast<const unsigned char*>(&sig), &R_p3);

    // compute sig.r*G
    ge_p3 rG_p3;
    ge_scalarmult_base(&rG_p3, reinterpret_cast<const unsigned char*>(&sig) + 32);

    // compute sig.c*D
    ge_p2 cD_p2;
    ge_scalarmult(&cD_p2, reinterpret_cast<const unsigned char*>(&sig), &D_p3);

    // compute sig.r*A
    ge_p2 rA_p2;
    ge_scalarmult(&rA_p2, reinterpret_cast<const unsigned char*>(&sig) + 32, &A_p3);

    // compute X = sig.c*R + sig.r*G
    PublicKey cR;
    ge_tobytes(reinterpret_cast<unsigned char*>(&cR), &cR_p2);
    ge_p3 cR_p3;
    if (ge_frombytes_vartime(&cR_p3, reinterpret_cast<const unsigned char*>(&cR)) != 0) return false;
    ge_cached rG_cached;
    ge_p3_to_cached(&rG_cached, &rG_p3);
    ge_p1p1 X_p1p1;
    ge_add(&X_p1p1, &cR_p3, &rG_cached);
    ge_p2 X_p2;
    ge_p1p1_to_p2(&X_p2, &X_p1p1);

    // compute Y = sig.c*D + sig.r*A
    PublicKey cD;
    PublicKey rA;
    ge_tobytes(reinterpret_cast<unsigned char*>(&cD), &cD_p2);
    ge_tobytes(reinterpret_cast<unsigned char*>(&rA), &rA_p2);
    ge_p3 cD_p3;
    ge_p3 rA_p3;
    if (ge_frombytes_vartime(&cD_p3, reinterpret_cast<const unsigned char*>(&cD)) != 0) return false;
    if (ge_frombytes_vartime(&rA_p3, reinterpret_cast<const unsigned char*>(&rA)) != 0) return false;
    ge_cached rA_cached;
    ge_p3_to_cached(&rA_cached, &rA_p3);
    ge_p1p1 Y_p1p1;
    ge_add(&Y_p1p1, &cD_p3, &rA_cached);
    ge_p2 Y_p2;
    ge_p1p1_to_p2(&Y_p2, &Y_p1p1);

    // compute c2 = Hs(Msg || D || X || Y)
    s_comm_2 buf;
    buf.msg = prefix_hash;
    buf.D = reinterpret_cast<const EllipticCurvePoint&>(D);
    ge_tobytes(&buf.X, &X_p2);
    ge_tobytes(&buf.Y, &Y_p2);
    EllipticCurveScalar c2;
    hash_to_scalar(&buf, sizeof(s_comm_2), c2);

    // test if c2 == sig.c
    sc_sub(reinterpret_cast<unsigned char*>(&c2), reinterpret_cast<unsigned char*>(&c2), reinterpret_cast<const unsigned char*>(&sig));
    return sc_isnonzero(&c2) == 0;
  }

  static void hash_to_ec(const PublicKey &key, ge_p3 &res) {
    Hash h;
    ge_p2 point;
    ge_p1p1 point2;
    cn_fast_hash(std::addressof(key), sizeof(PublicKey), h);
    ge_fromfe_frombytes_vartime(&point, reinterpret_cast<const unsigned char *>(&h));
    ge_mul8(&point2, &point);
    ge_p1p1_to_p3(&res, &point2);
  }

  KeyImage crypto_ops::scalarmultKey(const KeyImage & P, const KeyImage & a) {
    ge_p3 A;
    ge_p2 R;
    // maybe use assert instead?
    ge_frombytes_vartime(&A, reinterpret_cast<const unsigned char*>(&P));
    ge_scalarmult(&R, reinterpret_cast<const unsigned char*>(&a), &A);
    KeyImage aP;
    ge_tobytes(reinterpret_cast<unsigned char*>(&aP), &R);
    return aP;
  }

  void crypto_ops::hash_data_to_ec(const uint8_t* data, std::size_t len, PublicKey& key) {
    Hash h;
    ge_p2 point;
    ge_p1p1 point2;
    cn_fast_hash(data, len, h);
    ge_fromfe_frombytes_vartime(&point, reinterpret_cast<const unsigned char *>(&h));
    ge_mul8(&point2, &point);
    ge_p1p1_to_p2(&point, &point2);
    ge_tobytes(reinterpret_cast<unsigned char*>(&key), &point);
  }
  
  void crypto_ops::generate_key_image(const PublicKey &pub, const SecretKey &sec, KeyImage &image) {
    ge_p3 point;
    ge_p2 point2;
    assert(sc_check(reinterpret_cast<const unsigned char*>(&sec)) == 0);
    hash_to_ec(pub, point);
    ge_scalarmult(&point2, reinterpret_cast<const unsigned char*>(&sec), &point);
    ge_tobytes(reinterpret_cast<unsigned char*>(&image), &point2);
  }
  
  void crypto_ops::generate_incomplete_key_image(const PublicKey &pub, EllipticCurvePoint &incomplete_key_image) {
    ge_p3 point;
    hash_to_ec(pub, point);
    ge_p3_tobytes(reinterpret_cast<unsigned char*>(&incomplete_key_image), &point);
  }

#ifdef _MSC_VER
#pragma warning(disable: 4200)
#endif

  struct rs_comm {
    Hash h;
    struct {
      EllipticCurvePoint a, b;
    } ab[];
  };

  static inline size_t rs_comm_size(size_t pubs_count) {
    return sizeof(rs_comm) + pubs_count * sizeof(((rs_comm*)0)->ab[0]);
  }

  void crypto_ops::generate_ring_signature(const Hash &prefix_hash, const KeyImage &image,
    const PublicKey *const *pubs, size_t pubs_count,
    const SecretKey &sec, size_t sec_index,
    Signature *sig) {
    size_t i;
    ge_p3 image_unp;
    ge_dsmp image_pre;
    EllipticCurveScalar sum, k, h;
    rs_comm *const buf = reinterpret_cast<rs_comm *>(alloca(rs_comm_size(pubs_count)));
    assert(sec_index < pubs_count);
#if !defined(NDEBUG)
    {
      ge_p3 t;
      PublicKey t2;
      KeyImage t3;
      assert(sc_check(reinterpret_cast<const unsigned char*>(&sec)) == 0);
      ge_scalarmult_base(&t, reinterpret_cast<const unsigned char*>(&sec));
      ge_p3_tobytes(reinterpret_cast<unsigned char*>(&t2), &t);
      assert(*pubs[sec_index] == t2);
      generate_key_image(*pubs[sec_index], sec, t3);
      assert(image == t3);
      for (i = 0; i < pubs_count; i++) {
        assert(check_key(*pubs[i]));
      }
    }
#endif
    if (ge_frombytes_vartime(&image_unp, reinterpret_cast<const unsigned char*>(&image)) != 0) {
      abort();
    }
    ge_dsm_precomp(image_pre, &image_unp);
    sc_0(reinterpret_cast<unsigned char*>(&sum));
    buf->h = prefix_hash;
    for (i = 0; i < pubs_count; i++) {
      ge_p2 tmp2;
      ge_p3 tmp3;
      if (i == sec_index) {
        random_scalar(k);
        ge_scalarmult_base(&tmp3, reinterpret_cast<unsigned char*>(&k));
        ge_p3_tobytes(reinterpret_cast<unsigned char*>(&buf->ab[i].a), &tmp3);
        hash_to_ec(*pubs[i], tmp3);
        ge_scalarmult(&tmp2, reinterpret_cast<unsigned char*>(&k), &tmp3);
        ge_tobytes(reinterpret_cast<unsigned char*>(&buf->ab[i].b), &tmp2);
      } else {
        random_scalar(reinterpret_cast<EllipticCurveScalar&>(sig[i]));
        random_scalar(*reinterpret_cast<EllipticCurveScalar*>(reinterpret_cast<unsigned char*>(&sig[i]) + 32));
        if (ge_frombytes_vartime(&tmp3, reinterpret_cast<const unsigned char*>(&*pubs[i])) != 0) {
          abort();
        }
        ge_double_scalarmult_base_vartime(&tmp2, reinterpret_cast<unsigned char*>(&sig[i]), &tmp3, reinterpret_cast<unsigned char*>(&sig[i]) + 32);
        ge_tobytes(reinterpret_cast<unsigned char*>(&buf->ab[i].a), &tmp2);
        hash_to_ec(*pubs[i], tmp3);
        ge_double_scalarmult_precomp_vartime(&tmp2, reinterpret_cast<unsigned char*>(&sig[i]) + 32, &tmp3, reinterpret_cast<unsigned char*>(&sig[i]), image_pre);
        ge_tobytes(reinterpret_cast<unsigned char*>(&buf->ab[i].b), &tmp2);
        sc_add(reinterpret_cast<unsigned char*>(&sum), reinterpret_cast<unsigned char*>(&sum), reinterpret_cast<unsigned char*>(&sig[i]));
      }
    }
    hash_to_scalar(buf, rs_comm_size(pubs_count), h);
    sc_sub(reinterpret_cast<unsigned char*>(&sig[sec_index]), reinterpret_cast<unsigned char*>(&h), reinterpret_cast<unsigned char*>(&sum));
    sc_mulsub(reinterpret_cast<unsigned char*>(&sig[sec_index]) + 32, reinterpret_cast<unsigned char*>(&sig[sec_index]), reinterpret_cast<const unsigned char*>(&sec), reinterpret_cast<unsigned char*>(&k));
  }

  bool crypto_ops::check_ring_signature(const Hash &prefix_hash, const KeyImage &image,
    const PublicKey *const *pubs, size_t pubs_count,
    const Signature *sig) {
    size_t i;
    ge_p3 image_unp;
    ge_dsmp image_pre;
    EllipticCurveScalar sum, h;
    rs_comm *const buf = reinterpret_cast<rs_comm *>(alloca(rs_comm_size(pubs_count)));
#if !defined(NDEBUG)
    for (i = 0; i < pubs_count; i++) {
      assert(check_key(*pubs[i]));
    }
#endif
    if (ge_frombytes_vartime(&image_unp, reinterpret_cast<const unsigned char*>(&image)) != 0) {
      return false;
    }
    ge_dsm_precomp(image_pre, &image_unp);
    sc_0(reinterpret_cast<unsigned char*>(&sum));
    buf->h = prefix_hash;
    for (i = 0; i < pubs_count; i++) {
      ge_p2 tmp2;
      ge_p3 tmp3;
      if (sc_check(reinterpret_cast<const unsigned char*>(&sig[i])) != 0 || sc_check(reinterpret_cast<const unsigned char*>(&sig[i]) + 32) != 0) {
        return false;
      }
      if (ge_frombytes_vartime(&tmp3, reinterpret_cast<const unsigned char*>(&*pubs[i])) != 0) {
        abort();
      }
      ge_double_scalarmult_base_vartime(&tmp2, reinterpret_cast<const unsigned char*>(&sig[i]), &tmp3, reinterpret_cast<const unsigned char*>(&sig[i]) + 32);
      ge_tobytes(reinterpret_cast<unsigned char*>(&buf->ab[i].a), &tmp2);
      hash_to_ec(*pubs[i], tmp3);
      ge_double_scalarmult_precomp_vartime(&tmp2, reinterpret_cast<const unsigned char*>(&sig[i]) + 32, &tmp3, reinterpret_cast<const unsigned char*>(&sig[i]), image_pre);
      ge_tobytes(reinterpret_cast<unsigned char*>(&buf->ab[i].b), &tmp2);
      sc_add(reinterpret_cast<unsigned char*>(&sum), reinterpret_cast<unsigned char*>(&sum), reinterpret_cast<const unsigned char*>(&sig[i]));
    }
    hash_to_scalar(buf, rs_comm_size(pubs_count), h);
    sc_sub(reinterpret_cast<unsigned char*>(&h), reinterpret_cast<unsigned char*>(&h), reinterpret_cast<unsigned char*>(&sum));
    return sc_isnonzero(reinterpret_cast<unsigned char*>(&h)) == 0;
  }

}
