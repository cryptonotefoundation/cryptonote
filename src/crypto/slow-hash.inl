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

static void
#if defined(AESNI)
cn_slow_hash_aesni
#else
cn_slow_hash_noaesni
#endif
(void *restrict context, const void *restrict data, size_t length, void *restrict hash)
{
#define ctx ((struct cn_ctx *) context)
  ALIGNED_DECL(uint8_t ExpandedKey[256], 16);
  size_t i;
  __m128i *longoutput, *expkey, *xmminput, b_x;
  ALIGNED_DECL(uint64_t a[2], 16);
  hash_process(&ctx->state.hs, (const uint8_t*) data, length);

  memcpy(ctx->text, ctx->state.init, INIT_SIZE_BYTE);
#if defined(AESNI)
  memcpy(ExpandedKey, ctx->state.hs.b, AES_KEY_SIZE);
  ExpandAESKey256(ExpandedKey);
#else
  ctx->aes_ctx = oaes_alloc();
  oaes_key_import_data(ctx->aes_ctx, ctx->state.hs.b, AES_KEY_SIZE);
  memcpy(ExpandedKey, ctx->aes_ctx->key->exp_data, ctx->aes_ctx->key->exp_data_len);
#endif

  longoutput = (__m128i *) ctx->long_state;
  expkey = (__m128i *) ExpandedKey;
  xmminput = (__m128i *) ctx->text;

  //for (i = 0; likely(i < MEMORY); i += INIT_SIZE_BYTE)
  //    aesni_parallel_noxor(&ctx->long_state[i], ctx->text, ExpandedKey);

  for (i = 0; likely(i < MEMORY); i += INIT_SIZE_BYTE)
  {
#if defined(AESNI)
    for(size_t j = 0; j < 10; j++)
    {
      xmminput[0] = _mm_aesenc_si128(xmminput[0], expkey[j]);
      xmminput[1] = _mm_aesenc_si128(xmminput[1], expkey[j]);
      xmminput[2] = _mm_aesenc_si128(xmminput[2], expkey[j]);
      xmminput[3] = _mm_aesenc_si128(xmminput[3], expkey[j]);
      xmminput[4] = _mm_aesenc_si128(xmminput[4], expkey[j]);
      xmminput[5] = _mm_aesenc_si128(xmminput[5], expkey[j]);
      xmminput[6] = _mm_aesenc_si128(xmminput[6], expkey[j]);
      xmminput[7] = _mm_aesenc_si128(xmminput[7], expkey[j]);
    }
#else
    aesb_pseudo_round((uint8_t *) &xmminput[0], (uint8_t *) &xmminput[0], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[1], (uint8_t *) &xmminput[1], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[2], (uint8_t *) &xmminput[2], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[3], (uint8_t *) &xmminput[3], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[4], (uint8_t *) &xmminput[4], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[5], (uint8_t *) &xmminput[5], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[6], (uint8_t *) &xmminput[6], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[7], (uint8_t *) &xmminput[7], (uint8_t *) expkey);
#endif
    _mm_store_si128(&(longoutput[(i >> 4)]), xmminput[0]);
    _mm_store_si128(&(longoutput[(i >> 4) + 1]), xmminput[1]);
    _mm_store_si128(&(longoutput[(i >> 4) + 2]), xmminput[2]);
    _mm_store_si128(&(longoutput[(i >> 4) + 3]), xmminput[3]);
    _mm_store_si128(&(longoutput[(i >> 4) + 4]), xmminput[4]);
    _mm_store_si128(&(longoutput[(i >> 4) + 5]), xmminput[5]);
    _mm_store_si128(&(longoutput[(i >> 4) + 6]), xmminput[6]);
    _mm_store_si128(&(longoutput[(i >> 4) + 7]), xmminput[7]);
  }

  for (i = 0; i < 2; i++)
  {
    ctx->a[i] = ((uint64_t *)ctx->state.k)[i] ^  ((uint64_t *)ctx->state.k)[i+4];
    ctx->b[i] = ((uint64_t *)ctx->state.k)[i+2] ^  ((uint64_t *)ctx->state.k)[i+6];
  }

  b_x = _mm_load_si128((__m128i *)ctx->b);
  a[0] = ctx->a[0];
  a[1] = ctx->a[1];

  for(i = 0; likely(i < 0x80000); i++)
  {
    __m128i c_x = _mm_load_si128((__m128i *)&ctx->long_state[a[0] & 0x1FFFF0]);
    __m128i a_x = _mm_load_si128((__m128i *)a);
    ALIGNED_DECL(uint64_t c[2], 16);
    ALIGNED_DECL(uint64_t b[2], 16);
    uint64_t *nextblock, *dst;

#if defined(AESNI)
    c_x = _mm_aesenc_si128(c_x, a_x);
#else
    aesb_single_round((uint8_t *) &c_x, (uint8_t *) &c_x, (uint8_t *) &a_x);
#endif

    _mm_store_si128((__m128i *)c, c_x);
    //__builtin_prefetch(&ctx->long_state[c[0] & 0x1FFFF0], 0, 1);

    b_x = _mm_xor_si128(b_x, c_x);
    _mm_store_si128((__m128i *)&ctx->long_state[a[0] & 0x1FFFF0], b_x);

    nextblock = (uint64_t *)&ctx->long_state[c[0] & 0x1FFFF0];
    b[0] = nextblock[0];
    b[1] = nextblock[1];

    {
      uint64_t hi, lo;
      // hi,lo = 64bit x 64bit multiply of c[0] and b[0]

#if defined(__GNUC__) && defined(__x86_64__)
      __asm__("mulq %3\n\t"
        : "=d" (hi),
        "=a" (lo)
        : "%a" (c[0]),
        "rm" (b[0])
        : "cc" );
#else
      lo = mul128(c[0], b[0], &hi);
#endif

      a[0] += hi;
      a[1] += lo;
    }
    dst = (uint64_t *) &ctx->long_state[c[0] & 0x1FFFF0];
    dst[0] = a[0];
    dst[1] = a[1];

    a[0] ^= b[0];
    a[1] ^= b[1];
    b_x = c_x;
    //__builtin_prefetch(&ctx->long_state[a[0] & 0x1FFFF0], 0, 3);
  }

  memcpy(ctx->text, ctx->state.init, INIT_SIZE_BYTE);
#if defined(AESNI)
  memcpy(ExpandedKey, &ctx->state.hs.b[32], AES_KEY_SIZE);
  ExpandAESKey256(ExpandedKey);
#else
  oaes_key_import_data(ctx->aes_ctx, &ctx->state.hs.b[32], AES_KEY_SIZE);
  memcpy(ExpandedKey, ctx->aes_ctx->key->exp_data, ctx->aes_ctx->key->exp_data_len);
#endif

  //for (i = 0; likely(i < MEMORY); i += INIT_SIZE_BYTE)
  //    aesni_parallel_xor(&ctx->text, ExpandedKey, &ctx->long_state[i]);

  for (i = 0; likely(i < MEMORY); i += INIT_SIZE_BYTE)
  {
    xmminput[0] = _mm_xor_si128(longoutput[(i >> 4)], xmminput[0]);
    xmminput[1] = _mm_xor_si128(longoutput[(i >> 4) + 1], xmminput[1]);
    xmminput[2] = _mm_xor_si128(longoutput[(i >> 4) + 2], xmminput[2]);
    xmminput[3] = _mm_xor_si128(longoutput[(i >> 4) + 3], xmminput[3]);
    xmminput[4] = _mm_xor_si128(longoutput[(i >> 4) + 4], xmminput[4]);
    xmminput[5] = _mm_xor_si128(longoutput[(i >> 4) + 5], xmminput[5]);
    xmminput[6] = _mm_xor_si128(longoutput[(i >> 4) + 6], xmminput[6]);
    xmminput[7] = _mm_xor_si128(longoutput[(i >> 4) + 7], xmminput[7]);

#if defined(AESNI)
    for(size_t j = 0; j < 10; j++)
    {
      xmminput[0] = _mm_aesenc_si128(xmminput[0], expkey[j]);
      xmminput[1] = _mm_aesenc_si128(xmminput[1], expkey[j]);
      xmminput[2] = _mm_aesenc_si128(xmminput[2], expkey[j]);
      xmminput[3] = _mm_aesenc_si128(xmminput[3], expkey[j]);
      xmminput[4] = _mm_aesenc_si128(xmminput[4], expkey[j]);
      xmminput[5] = _mm_aesenc_si128(xmminput[5], expkey[j]);
      xmminput[6] = _mm_aesenc_si128(xmminput[6], expkey[j]);
      xmminput[7] = _mm_aesenc_si128(xmminput[7], expkey[j]);
    }
#else
    aesb_pseudo_round((uint8_t *) &xmminput[0], (uint8_t *) &xmminput[0], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[1], (uint8_t *) &xmminput[1], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[2], (uint8_t *) &xmminput[2], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[3], (uint8_t *) &xmminput[3], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[4], (uint8_t *) &xmminput[4], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[5], (uint8_t *) &xmminput[5], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[6], (uint8_t *) &xmminput[6], (uint8_t *) expkey);
    aesb_pseudo_round((uint8_t *) &xmminput[7], (uint8_t *) &xmminput[7], (uint8_t *) expkey);
#endif

  }

#if !defined(AESNI)
  oaes_free((OAES_CTX **) &ctx->aes_ctx);
#endif

  memcpy(ctx->state.init, ctx->text, INIT_SIZE_BYTE);
  hash_permutation(&ctx->state.hs);
  extra_hashes[ctx->state.hs.b[0] & 3](&ctx->state, 200, hash);
}
