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

#include "CryptoNoteCore/Checkpoints.h"
#include <Logging/LoggerGroup.h>

using namespace CryptoNote;

TEST(checkpoints_isAlternativeBlockAllowed, handles_empty_checkpoins)
{
  Logging::LoggerGroup logger;
  Checkpoints cp(logger);

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(0, 0));

  ASSERT_TRUE(cp.isAlternativeBlockAllowed(1, 1));
  ASSERT_TRUE(cp.isAlternativeBlockAllowed(1, 9));
  ASSERT_TRUE(cp.isAlternativeBlockAllowed(9, 1));
}

TEST(checkpoints_isAlternativeBlockAllowed, handles_one_checkpoint)
{
  Logging::LoggerGroup logger;
  Checkpoints cp(logger);
  cp.addCheckpoint(5, "0000000000000000000000000000000000000000000000000000000000000000");

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(0, 0));

  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 1));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 4));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 9));

  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 1));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 4));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 9));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(5, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(5, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(5, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(5, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(5, 9));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(6, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(6, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(6, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(6, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(6, 9));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(9, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(9, 9));
}

TEST(checkpoints_isAlternativeBlockAllowed, handles_two_and_more_checkpoints)
{
  Logging::LoggerGroup logger;
  Checkpoints cp(logger);
  cp.addCheckpoint(5, "0000000000000000000000000000000000000000000000000000000000000000");
  cp.addCheckpoint(9, "0000000000000000000000000000000000000000000000000000000000000000");

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(0, 0));

  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 1));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 4));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 8));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 9));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 10));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(1, 11));

  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 1));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 4));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 8));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 9));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 10));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(4, 11));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(5, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(5, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(5, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(5, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(5, 8));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(5, 9));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(5, 10));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(5, 11));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(6, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(6, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(6, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(6, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(6, 8));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(6, 9));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(6, 10));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(6, 11));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(8, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(8, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(8, 5));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(8, 6));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(8, 8));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(8, 9));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(8, 10));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(8, 11));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 5));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 6));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 8));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(9, 9));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(9, 10));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(9, 11));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(10, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(10, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(10, 5));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(10, 6));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(10, 8));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(10, 9));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(10, 10));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(10, 11));

  ASSERT_FALSE(cp.isAlternativeBlockAllowed(11, 1));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(11, 4));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(11, 5));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(11, 6));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(11, 8));
  ASSERT_FALSE(cp.isAlternativeBlockAllowed(11, 9));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(11, 10));
  ASSERT_TRUE (cp.isAlternativeBlockAllowed(11, 11));
}
