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

#include <string>

#include "gtest/gtest.h"

#include <unordered_set>
#include "crypto/crypto.h"
#include "Common/ShuffleGenerator.h"

class ShuffleTest : public ::testing::Test {
public:

  typedef ShuffleGenerator<size_t, std::default_random_engine> DefaultShuffleGenerator;
  typedef ShuffleGenerator<size_t, Crypto::random_engine<size_t>> CryptoShuffleGenerator;

  template <typename Gen>
  void checkUniqueness(Gen& gen, size_t count) {

    std::unordered_set<size_t> values;

    for (auto i = 0; i < count; ++i) {
      auto value = gen();
      bool inserted = values.insert(value).second;
      EXPECT_TRUE(inserted);
    }
  }

  template <typename Gen>
  void consume(Gen& gen, size_t count) {
    for (auto i = 0; i < count; ++i) {
      gen();
    }
  }

  template <typename ShuffleT>
  void checkEngine(size_t N, size_t consumeCount, bool check) {
    ShuffleT gen(N);
    check ? checkUniqueness(gen, consumeCount) : consume(gen, consumeCount);
  }

};


namespace {
const size_t ITERATIONS = 10000;
}

TEST_F(ShuffleTest, correctness) {
  checkEngine<DefaultShuffleGenerator>(ITERATIONS, ITERATIONS, true);
}

TEST_F(ShuffleTest, correctness_fractionalSize) {
  checkEngine<DefaultShuffleGenerator>(ITERATIONS, ITERATIONS, true);
  checkEngine<DefaultShuffleGenerator>(ITERATIONS, ITERATIONS/2, true);
  checkEngine<DefaultShuffleGenerator>(ITERATIONS, ITERATIONS/3, true);
}


TEST_F(ShuffleTest, cryptoGenerator) {
  checkEngine<CryptoShuffleGenerator>(ITERATIONS * 3, ITERATIONS, false);
}
