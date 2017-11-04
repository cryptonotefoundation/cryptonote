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

#include "SynchronizationState.h"

#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"

using namespace Common;

namespace CryptoNote {

SynchronizationState::ShortHistory SynchronizationState::getShortHistory(uint32_t localHeight) const {
  ShortHistory history;
  uint32_t i = 0;
  uint32_t current_multiplier = 1;
  uint32_t sz = std::min(static_cast<uint32_t>(m_blockchain.size()), localHeight + 1);

  if (!sz)
    return history;

  uint32_t current_back_offset = 1;
  bool genesis_included = false;

  while (current_back_offset < sz) {
    history.push_back(m_blockchain[sz - current_back_offset]);
    if (sz - current_back_offset == 0)
      genesis_included = true;
    if (i < 10) {
      ++current_back_offset;
    } else {
      current_back_offset += current_multiplier *= 2;
    }
    ++i;
  }

  if (!genesis_included) {
    history.push_back(m_blockchain[0]);
  }

  return history;
}

SynchronizationState::CheckResult SynchronizationState::checkInterval(const BlockchainInterval& interval) const {
  assert(interval.startHeight <= m_blockchain.size());

  CheckResult result = { false, 0, false, 0 };

  uint32_t intervalEnd = interval.startHeight + static_cast<uint32_t>(interval.blocks.size());
  uint32_t iterationEnd = std::min(static_cast<uint32_t>(m_blockchain.size()), intervalEnd);

  for (uint32_t i = interval.startHeight; i < iterationEnd; ++i) {
    if (m_blockchain[i] != interval.blocks[i - interval.startHeight]) {
      result.detachRequired = true;
      result.detachHeight = i;
      break;
    }
  }

  if (result.detachRequired) {
    result.hasNewBlocks = true;
    result.newBlockHeight = result.detachHeight;
    return result;
  }

  if (intervalEnd > m_blockchain.size()) {
    result.hasNewBlocks = true;
    result.newBlockHeight = static_cast<uint32_t>(m_blockchain.size());
  }

  return result;
}

void SynchronizationState::detach(uint32_t height) {
  assert(height < m_blockchain.size());
  m_blockchain.resize(height);
}

void SynchronizationState::addBlocks(const Crypto::Hash* blockHashes, uint32_t height, uint32_t count) {
  assert(blockHashes);
  auto size = m_blockchain.size();
  assert( size == height);
  m_blockchain.insert(m_blockchain.end(), blockHashes, blockHashes + count);
}

uint32_t SynchronizationState::getHeight() const {
  return static_cast<uint32_t>(m_blockchain.size());
}

const std::vector<Crypto::Hash>& SynchronizationState::getKnownBlockHashes() const {
  return m_blockchain;
}

void SynchronizationState::save(std::ostream& os) {
  StdOutputStream stream(os);
  CryptoNote::BinaryOutputStreamSerializer s(stream);
  serialize(s, "state");
}

void SynchronizationState::load(std::istream& in) {
  StdInputStream stream(in);
  CryptoNote::BinaryInputStreamSerializer s(stream);
  serialize(s, "state");
}

CryptoNote::ISerializer& SynchronizationState::serialize(CryptoNote::ISerializer& s, const std::string& name) {
  s.beginObject(name);
  s(m_blockchain, "blockchain");
  s.endObject();
  return s;
}

}
