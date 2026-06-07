// Copyright (c) 2012-2018, The CryptoNote developers, The Bytecoin developers.
// Licensed under the GNU Lesser General Public License. See LICENSE for details.

// Copyright (c) 2013-2018
// Frank Denis <j at pureftpd dot org>
// See https://github.com/jedisct1/libsodium/blob/master/LICENSE for details

#include <memory.h>
#include <stdio.h>

#include "crypto-util.h"

#ifdef _WIN32
#include <windows.h>
#endif

void sodium_memzero(void *pnt, size_t len) {
#ifdef _WIN32
	SecureZeroMemory(pnt, len);
#else
	volatile unsigned char *volatile pnt_ = (volatile unsigned char *volatile)pnt;
	size_t i                              = (size_t)0U;

	while (i < len) {
		pnt_[i++] = 0U;
	}
#endif
}

int sodium_compare(const void *a1, const void *a2, size_t len) {
	const volatile unsigned char *volatile b1 = (const volatile unsigned char *)a1;
	const volatile unsigned char *volatile b2 = (const volatile unsigned char *)a2;
	size_t i;
	volatile unsigned char gt = 0U;
	volatile unsigned char eq = 1U;
	uint16_t x1, x2;

	i = len;
	while (i != 0U) {
		i--;
		x1 = b1[i];
		x2 = b2[i];
		gt |= ((x2 - x1) >> 8) & eq;
		eq &= ((x2 ^ x1) - 1) >> 8;
	}
	return (int)(gt + gt + eq) - 1;
}
