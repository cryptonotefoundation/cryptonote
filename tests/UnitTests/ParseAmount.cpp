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

#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include <Logging/LoggerGroup.h>

using namespace CryptoNote;

namespace
{
  const size_t TEST_NUMBER_OF_DECIMAL_PLACES = 8;

  void do_pos_test(uint64_t expected, const std::string& str)
  {
    Logging::LoggerGroup logger;
    CryptoNote::Currency currency = CryptoNote::CurrencyBuilder(logger).numberOfDecimalPlaces(TEST_NUMBER_OF_DECIMAL_PLACES).currency();
    uint64_t val;
    std::string number_str = str;
    std::replace(number_str.begin(), number_str.end(), '_', '.');
    number_str.erase(std::remove(number_str.begin(), number_str.end(), '~'), number_str.end());
    ASSERT_TRUE(currency.parseAmount(number_str, val));
    ASSERT_EQ(expected, val);
  }

  void do_neg_test(const std::string& str)
  {
    Logging::LoggerGroup logger;
    CryptoNote::Currency currency = CryptoNote::CurrencyBuilder(logger).numberOfDecimalPlaces(TEST_NUMBER_OF_DECIMAL_PLACES).currency();
    uint64_t val;
    std::string number_str = str;
    std::replace(number_str.begin(), number_str.end(), '_', '.');
    number_str.erase(std::remove(number_str.begin(), number_str.end(), '~'), number_str.end());
    ASSERT_FALSE(currency.parseAmount(number_str, val));
  }
}

#define TEST_pos(expected, str)            \
  TEST(parse_amount, handles_pos_ ## str)  \
  {                                        \
    do_pos_test(UINT64_C(expected), #str); \
  }

#define TEST_neg(str)                      \
  TEST(parse_amount, handles_neg_ ## str)  \
  {                                        \
    do_neg_test(#str);                     \
  }

#define TEST_neg_n(str, name)              \
  TEST(parse_amount, handles_neg_ ## name) \
  {                                        \
    do_neg_test(#str);                     \
  }


TEST_pos(0, 0);
TEST_pos(0, 00);
TEST_pos(0, 00000000);
TEST_pos(0, 000000000);
TEST_pos(0, 00000000000000000000000000000000);

TEST_pos(0, _0);
TEST_pos(0, _00);
TEST_pos(0, _00000000);
TEST_pos(0, _000000000);
TEST_pos(0, _00000000000000000000000000000000);

TEST_pos(0, 00000000_);
TEST_pos(0, 000000000_);
TEST_pos(0, 00000000000000000000000000000000_);

TEST_pos(0, 0_);
TEST_pos(0, 0_0);
TEST_pos(0, 0_00);
TEST_pos(0, 0_00000000);
TEST_pos(0, 0_000000000);
TEST_pos(0, 0_00000000000000000000000000000000);

TEST_pos(0, 00_);
TEST_pos(0, 00_0);
TEST_pos(0, 00_00);
TEST_pos(0, 00_00000000);
TEST_pos(0, 00_000000000);
TEST_pos(0, 00_00000000000000000000000000000000);

TEST_pos(1, 0_00000001);
TEST_pos(1, 0_000000010);
TEST_pos(1, 0_000000010000000000000000000000000);
TEST_pos(9, 0_00000009);
TEST_pos(9, 0_000000090);
TEST_pos(9, 0_000000090000000000000000000000000);

TEST_pos(           100000000,            1);
TEST_pos(       6553500000000,        65535);
TEST_pos(  429496729500000000,   4294967295);
TEST_pos(18446744073700000000, 184467440737_);
TEST_pos(18446744073700000000, 184467440737_0);
TEST_pos(18446744073700000000, 184467440737_00000000);
TEST_pos(18446744073700000000, 184467440737_000000000);
TEST_pos(18446744073700000000, 184467440737_0000000000000000000);
TEST_pos(18446744073709551615, 184467440737_09551615);

// Invalid numbers
TEST_neg_n(~, empty_string);
TEST_neg_n(-0, minus_0);
TEST_neg_n(+0, plus_0);
TEST_neg_n(-1, minus_1);
TEST_neg_n(+1, plus_1);
TEST_neg_n(_, only_point);

// A lot of fraction digits
TEST_neg(0_000000001);
TEST_neg(0_000000009);
TEST_neg(184467440737_000000001);

// Overflow
TEST_neg(184467440737_09551616);
TEST_neg(184467440738);
TEST_neg(18446744073709551616);

// Two or more points
TEST_neg(__);
TEST_neg(0__);
TEST_neg(__0);
TEST_neg(0__0);
TEST_neg(0_0_);
TEST_neg(_0_0);
TEST_neg(0_0_0);
