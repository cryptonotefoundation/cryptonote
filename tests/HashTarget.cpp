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

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "crypto/hash.h"
#include "CryptoNoteCore/Difficulty.h"

using namespace std;
using CryptoNote::check_hash;

int main(int argc, char *argv[]) {
  Crypto::Hash h;
  for (uint64_t diff = 1;; diff += 1 + (diff >> 8)) {
    for (int b = 0; b < 256; b++) {
      memset(&h, b, sizeof(Crypto::Hash));
      if (check_hash(h, diff) != (b == 0 || diff <= 255 / b)) {
        return 1;
      }
      if (b > 0) {
        memset(&h, 0, sizeof(Crypto::Hash));
        ((char *) &h)[31] = b;
        if (check_hash(h, diff) != (diff <= 255 / b)) {
          return 1;
        }
      }
    }
    if (diff < numeric_limits<uint64_t>::max() / 256) {
      uint64_t val = 0;
      for (int i = 31; i >= 0; i--) {
        val = val * 256 + 255;
        ((char *) &h)[i] = static_cast<char>(val / diff);
        val %= diff;
      }
      if (check_hash(h, diff) != true) {
        return 1;
      }
      if (diff > 1) {
        for (int i = 0;; i++) {
          if (i >= 32) {
            abort();
          }
          if (++((char *) &h)[i] != 0) {
            break;
          }
        }
        if (check_hash(h, diff) != false) {
          return 1;
        }
      }
    }
    if (diff + 1 + (diff >> 8) < diff) {
      break;
    }
  }
  return 0;
}
