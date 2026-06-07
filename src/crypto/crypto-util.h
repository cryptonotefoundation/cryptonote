// Copyright (c) 2012-2018, The CryptoNote developers, The Bytecoin developers.
// Licensed under the GNU Lesser General Public License. See LICENSE for details.

// Copyright (c) 2013-2018
// Frank Denis <j at pureftpd dot org>
// See https://github.com/jedisct1/libsodium/blob/master/LICENSE for details

#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif
// We borrow from https://libsodium.org/
void sodium_memzero(void *pnt, size_t length);
int sodium_compare(const void *a1, const void *a2, size_t length);

#if defined(__cplusplus)
}
#endif
