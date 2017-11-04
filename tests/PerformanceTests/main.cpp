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

#include "PerformanceTests.h"
#include "PerformanceUtils.h"

// tests
#include "ConstructTransaction.h"
#include "CheckRingSignature.h"
#include "CryptoNoteSlowHash.h"
#include "DerivePublicKey.h"
#include "DeriveSecretKey.h"
#include "GenerateKeyDerivation.h"
#include "GenerateKeyImage.h"
#include "GenerateKeyImageHelper.h"
#include "IsOutToAccount.h"

int main(int argc, char** argv)
{
  set_process_affinity(1);
  set_thread_high_priority();

  performance_timer timer;
  timer.start();

  TEST_PERFORMANCE2(test_construct_tx, 1, 1);
  TEST_PERFORMANCE2(test_construct_tx, 1, 2);
  TEST_PERFORMANCE2(test_construct_tx, 1, 10);
  TEST_PERFORMANCE2(test_construct_tx, 1, 100);
  TEST_PERFORMANCE2(test_construct_tx, 1, 1000);

  TEST_PERFORMANCE2(test_construct_tx, 2, 1);
  TEST_PERFORMANCE2(test_construct_tx, 2, 2);
  TEST_PERFORMANCE2(test_construct_tx, 2, 10);
  TEST_PERFORMANCE2(test_construct_tx, 2, 100);

  TEST_PERFORMANCE2(test_construct_tx, 10, 1);
  TEST_PERFORMANCE2(test_construct_tx, 10, 2);
  TEST_PERFORMANCE2(test_construct_tx, 10, 10);
  TEST_PERFORMANCE2(test_construct_tx, 10, 100);

  TEST_PERFORMANCE2(test_construct_tx, 100, 1);
  TEST_PERFORMANCE2(test_construct_tx, 100, 2);
  TEST_PERFORMANCE2(test_construct_tx, 100, 10);
  TEST_PERFORMANCE2(test_construct_tx, 100, 100);

  TEST_PERFORMANCE1(test_check_ring_signature, 1);
  TEST_PERFORMANCE1(test_check_ring_signature, 2);
  TEST_PERFORMANCE1(test_check_ring_signature, 10);
  TEST_PERFORMANCE1(test_check_ring_signature, 100);

  TEST_PERFORMANCE0(test_is_out_to_acc);
  TEST_PERFORMANCE0(test_generate_key_image_helper);
  TEST_PERFORMANCE0(test_generate_key_derivation);
  TEST_PERFORMANCE0(test_generate_key_image);
  TEST_PERFORMANCE0(test_derive_public_key);
  TEST_PERFORMANCE0(test_derive_secret_key);

  TEST_PERFORMANCE0(test_cn_slow_hash);

  std::cout << "Tests finished. Elapsed time: " << timer.elapsed_ms() / 1000 << " sec" << std::endl;

  return 0;
}
