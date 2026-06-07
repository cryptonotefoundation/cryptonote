// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <cstdint>

#include "CryptoNoteCore/Difficulty.h"

namespace CryptoNote {

struct BlockStatsEntry {
  uint32_t height;
  uint64_t alreadyGeneratedCoins;
  uint64_t transactionsCount;
  uint64_t blockSize;
  difficulty_type difficulty;
  uint64_t reward;
  uint64_t timestamp;
};

} // namespace CryptoNote
