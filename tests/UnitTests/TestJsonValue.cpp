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

#include "gtest/gtest.h"
#include <Common/JsonValue.h>

using Common::JsonValue;

namespace {

std::vector<std::string> goodPatterns{
  "{}",
  "   {}   ",
  "   {   }   ",
  "100",
  "[10,20,30]",
  "  [  10  , \n 20  , \n  30  ]  ",
  "{\"prop\": 100}",
  "{\"prop\": 100, \"prop2\": [100, 20, 30] }",
  "{\"prop\": 100, \"prop2\": { \"p\":\"test\" } }",

};


std::vector<std::string> badPatterns{
  "",
  "1..2",
  "\n\n",
  "{",
  "[",
  "[100,",
  "[[]",
  "\"",
  "{\"prop: 100 }",
  "{\"prop\" 100 }",
  "{ prop: 100 }",
};

}

TEST(JsonValue, testGoodPatterns) {
  for (const auto& p : goodPatterns) {
    std::cout << "Pattern: " << p << std::endl;
    ASSERT_NO_THROW(Common::JsonValue::fromString(p));
  }
}

TEST(JsonValue, testBadPatterns) {
  for (const auto& p : badPatterns) {
    ASSERT_ANY_THROW(Common::JsonValue::fromString(p));
  }
}

