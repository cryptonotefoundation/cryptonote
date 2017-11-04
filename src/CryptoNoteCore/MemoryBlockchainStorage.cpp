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

#include "MemoryBlockchainStorage.h"
#include <cassert>
#include "Serialization/SerializationOverloads.h"

using namespace CryptoNote;

MemoryBlockchainStorage::MemoryBlockchainStorage(uint32_t reserveSize) {
  blocks.reserve(reserveSize);
}

MemoryBlockchainStorage::~MemoryBlockchainStorage() {
}

void MemoryBlockchainStorage::pushBlock(RawBlock&& rawBlock) {
  blocks.push_back(rawBlock);
}

RawBlock MemoryBlockchainStorage::getBlockByIndex(uint32_t index) const {
  assert(index < getBlockCount());
  return blocks[index];
}

uint32_t MemoryBlockchainStorage::getBlockCount() const {
  return static_cast<uint32_t>(blocks.size());
}

//Returns MemoryBlockchainStorage with elements from [splitIndex, blocks.size() - 1].
//Original MemoryBlockchainStorage will contain elements from [0, splitIndex - 1].
std::unique_ptr<BlockchainStorage::IBlockchainStorageInternal> MemoryBlockchainStorage::splitStorage(uint32_t splitIndex) {
  assert(splitIndex > 0);
  assert(splitIndex < blocks.size());
  std::unique_ptr<MemoryBlockchainStorage> newStorage(new MemoryBlockchainStorage(splitIndex));
  std::move(blocks.begin() + splitIndex, blocks.end(), std::back_inserter(newStorage->blocks));
  blocks.resize(splitIndex);
  blocks.shrink_to_fit();
  return std::move(newStorage);
}
