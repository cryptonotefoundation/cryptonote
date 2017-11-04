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

#include "Checkpoints.h"
#include "Common/StringTools.h"

using namespace Logging;

namespace CryptoNote {
//---------------------------------------------------------------------------
Checkpoints::Checkpoints(Logging::ILogger &log) : logger(log, "checkpoints") {}
//---------------------------------------------------------------------------
bool Checkpoints::addCheckpoint(uint32_t index, const std::string &hash_str) {
  Crypto::Hash h = NULL_HASH;

  if (!Common::podFromHex(hash_str, h)) {
    logger(ERROR, BRIGHT_RED) << "WRONG HASH IN CHECKPOINTS!!!";
    return false;
  }

  if (!(0 == points.count(index))) {
    logger(ERROR, BRIGHT_RED) << "WRONG HASH IN CHECKPOINTS!!!";
    return false;
  }

  points[index] = h;
  return true;
}
//---------------------------------------------------------------------------
bool Checkpoints::isInCheckpointZone(uint32_t index) const {
  return !points.empty() && (index <= (--points.end())->first);
}
//---------------------------------------------------------------------------
bool Checkpoints::checkBlock(uint32_t index, const Crypto::Hash &h,
                            bool& isCheckpoint) const {
  auto it = points.find(index);
  isCheckpoint = it != points.end();
  if (!isCheckpoint)
    return true;

  if (it->second == h) {
    logger(Logging::INFO, Logging::GREEN) 
      << "CHECKPOINT PASSED FOR INDEX " << index << " " << h;
    return true;
  } else {
    logger(Logging::WARNING, BRIGHT_YELLOW) << "CHECKPOINT FAILED FOR HEIGHT " << index
                                            << ". EXPECTED HASH: " << it->second
                                            << ", FETCHED HASH: " << h;
    return false;
  }
}
//---------------------------------------------------------------------------
bool Checkpoints::checkBlock(uint32_t index, const Crypto::Hash &h) const {
  bool ignored;
  return checkBlock(index, h, ignored);
}
//---------------------------------------------------------------------------
bool Checkpoints::isAlternativeBlockAllowed(uint32_t  blockchainSize,
                                            uint32_t  blockIndex) const {
  if (blockchainSize == 0) {
    return false;
  }

  auto it = points.upper_bound(blockchainSize);
  // Is blockchainSize before the first checkpoint?
  if (it == points.begin()) {
    return true;
  }

  --it;
  uint32_t checkpointIndex = it->first;
  return checkpointIndex < blockIndex;
}

std::vector<uint32_t> Checkpoints::getCheckpointHeights() const {
  std::vector<uint32_t> checkpointHeights;
  checkpointHeights.reserve(points.size());
  for (const auto& it : points) {
    checkpointHeights.push_back(it.first);
  }

  return checkpointHeights;
}

}
