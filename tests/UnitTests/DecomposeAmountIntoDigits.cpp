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

#include <cstdint>
#include <vector>

#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

#define VEC_FROM_ARR(vec)                                               \
  std::vector<uint64_t> vec;                                            \
  for (size_t i = 0; i < sizeof(vec##_arr) / sizeof(vec##_arr[0]); ++i) \
  {                                                                     \
    vec.push_back(vec##_arr[i]);                                        \
  }

namespace
{
  struct chunk_handler_t
  {
    void operator()(uint64_t chunk) const
    {
      m_chunks.push_back(chunk);
    }

    mutable std::vector<uint64_t> m_chunks;
  };

  struct dust_handler_t
  {
    dust_handler_t()
      : m_dust(0)
      , m_has_dust(false)
    {
    }

    void operator()(uint64_t dust) const
    {
      m_dust = dust;
      m_has_dust = true;
    }

    mutable uint64_t m_dust;
    mutable bool m_has_dust;
  };

  class decompose_amount_into_digits_test : public ::testing::Test
  {
  protected:
    chunk_handler_t m_chunk_handler;
    dust_handler_t m_dust_handler;
  };
}

TEST_F(decompose_amount_into_digits_test, is_correct_0)
{
  std::vector<uint64_t> expected_chunks;
  CryptoNote::decompose_amount_into_digits(0, 0, m_chunk_handler, m_dust_handler);
  ASSERT_EQ(m_chunk_handler.m_chunks, expected_chunks);
  ASSERT_EQ(m_dust_handler.m_has_dust, false);
}

TEST_F(decompose_amount_into_digits_test, is_correct_1)
{
  std::vector<uint64_t> expected_chunks;
  CryptoNote::decompose_amount_into_digits(0, 10, m_chunk_handler, m_dust_handler);
  ASSERT_EQ(m_chunk_handler.m_chunks, expected_chunks);
  ASSERT_EQ(m_dust_handler.m_has_dust, false);
}

TEST_F(decompose_amount_into_digits_test, is_correct_2)
{
  uint64_t expected_chunks_arr[] = {10};
  VEC_FROM_ARR(expected_chunks);
  CryptoNote::decompose_amount_into_digits(10, 0, m_chunk_handler, m_dust_handler);
  ASSERT_EQ(m_chunk_handler.m_chunks, expected_chunks);
  ASSERT_EQ(m_dust_handler.m_has_dust, false);
}

TEST_F(decompose_amount_into_digits_test, is_correct_3)
{
  std::vector<uint64_t> expected_chunks;
  uint64_t expected_dust = 10;
  CryptoNote::decompose_amount_into_digits(10, 10, m_chunk_handler, m_dust_handler);
  ASSERT_EQ(m_chunk_handler.m_chunks, expected_chunks);
  ASSERT_EQ(m_dust_handler.m_dust, expected_dust);
}

TEST_F(decompose_amount_into_digits_test, is_correct_4)
{
  uint64_t expected_dust = 8100;
  std::vector<uint64_t> expected_chunks;
  CryptoNote::decompose_amount_into_digits(8100, 1000000, m_chunk_handler, m_dust_handler);
  ASSERT_EQ(m_chunk_handler.m_chunks, expected_chunks);
  ASSERT_EQ(m_dust_handler.m_dust, expected_dust);
}

TEST_F(decompose_amount_into_digits_test, is_correct_5)
{
  uint64_t expected_chunks_arr[] = {100, 900000, 8000000};
  VEC_FROM_ARR(expected_chunks);
  CryptoNote::decompose_amount_into_digits(8900100, 10, m_chunk_handler, m_dust_handler);
  ASSERT_EQ(m_chunk_handler.m_chunks, expected_chunks);
  ASSERT_EQ(m_dust_handler.m_has_dust, false);
}

TEST_F(decompose_amount_into_digits_test, is_correct_6)
{
  uint64_t expected_chunks_arr[] = {900000, 8000000};
  VEC_FROM_ARR(expected_chunks);
  uint64_t expected_dust = 100;
  CryptoNote::decompose_amount_into_digits(8900100, 1000, m_chunk_handler, m_dust_handler);
  ASSERT_EQ(m_chunk_handler.m_chunks, expected_chunks);
  ASSERT_EQ(m_dust_handler.m_dust, expected_dust);
}
