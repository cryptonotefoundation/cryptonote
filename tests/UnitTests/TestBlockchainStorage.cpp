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

#include "crypto/crypto.h"

#include "CryptoNoteCore/BlockchainStorage.h"
#include "CryptoNoteCore/MemoryBlockchainStorage.h"
#include "CryptoNoteCore/SwappedBlockchainStorage.h"

#include <boost/filesystem/operations.hpp>
#include <boost/range/combine.hpp>

using namespace CryptoNote;

class BlockchainStorageTests : public ::testing::Test {
public:
  BlockchainStorageTests() {
  }

  void SetUp() override {
    m_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("test_data_dir_%%%%%%%%%%%%");
    boost::system::error_code ignoredErrorCode;
    boost::filesystem::create_directory(m_dir, ignoredErrorCode);
  }

  void TearDown() override {
    boost::system::error_code ignoredErrorCode;
    boost::filesystem::remove_all(m_dir, ignoredErrorCode);
  }

  std::vector<RawBlock> generateRandomBlocks(size_t blocksNumber, size_t txsPerBlock) {
    const size_t BLOCK_SIZE = 10;
    const size_t TX_SIZE = 5;
    std::vector<RawBlock> blocks;
    for (size_t i = 0; i < blocksNumber; ++i) {
      RawBlock block;
      for (size_t j = 0; j < BLOCK_SIZE; ++j) {
        block.block.push_back(rand());
      }

      for (size_t j = 0; j < txsPerBlock; ++j) {
        BinaryArray rawTx;
        for (size_t k = 0; k < TX_SIZE; ++k) {
          rawTx.push_back(rand());
        }

        block.transactions.push_back(rawTx);
      }

      blocks.push_back(block);
    }

    return blocks;
  }

protected:
  boost::filesystem::path m_dir;
};

TEST_F(BlockchainStorageTests, MemoryBlockchainStoragePush) {
  MemoryBlockchainStorage storage(1);
  RawBlock block = generateRandomBlocks(1, 0).front();
  storage.pushBlock(std::move(block));
  ASSERT_EQ(1, storage.getBlockCount());
}

namespace CryptoNote {
bool operator == (const CryptoNote::RawBlock& l, const CryptoNote::RawBlock& r) {
  using boost::get;
  if (l.block.size() != r.block.size() || l.transactions.size() != r.transactions.size()) {
    return false;
  }

  if (!std::equal(std::begin(l.block), std::end(l.block), std::begin(r.block))) {
    return false;
  }

  for (const auto& pair : boost::combine(l.transactions, r.transactions)) {
    if (!std::equal(get<0>(pair).begin(), get<0>(pair).end(), get<1>(pair).begin())) {
      return false;
    }
  }

  return true;
}
}

TEST_F(BlockchainStorageTests, MemoryBlockchainStorageGetBlock) {
  MemoryBlockchainStorage storage(2);
  std::vector<RawBlock> blocks = generateRandomBlocks(2, 0);
  std::vector<RawBlock> blocksCopy = blocks;
  storage.pushBlock(std::move(blocksCopy.front()));
  storage.pushBlock(std::move(blocksCopy.back()));
  ASSERT_EQ(2, storage.getBlockCount());
  ASSERT_EQ(blocks.front(), storage.getBlockByIndex(0));
  ASSERT_EQ(blocks.back(), storage.getBlockByIndex(1));
}

TEST_F(BlockchainStorageTests, MemoryBlockchainStorageSplit) {
  const uint32_t BLOCKS_COUNT = 10;
  const uint32_t SPLIT_INDEX = 4;
  MemoryBlockchainStorage storage(BLOCKS_COUNT);
  std::vector<RawBlock> blocks = generateRandomBlocks(BLOCKS_COUNT, 0);
  std::vector<RawBlock> blocksCopy = blocks;
  for (RawBlock& block : blocksCopy) {
    storage.pushBlock(std::move(block));
  }

  ASSERT_EQ(BLOCKS_COUNT, storage.getBlockCount());

  std::unique_ptr<BlockchainStorage::IBlockchainStorageInternal> newStorage = storage.splitStorage(SPLIT_INDEX);
  ASSERT_EQ(SPLIT_INDEX, storage.getBlockCount());
  ASSERT_EQ(BLOCKS_COUNT - SPLIT_INDEX, newStorage->getBlockCount());

  for (uint32_t i = 0; i < SPLIT_INDEX; ++i) {
    ASSERT_EQ(blocks[i], storage.getBlockByIndex(i));
  }

  for (uint32_t i = 0; i < BLOCKS_COUNT - SPLIT_INDEX; ++i) {
    ASSERT_EQ(blocks[SPLIT_INDEX + i], newStorage->getBlockByIndex(i));
  }
}

TEST_F(BlockchainStorageTests, SwappedBlockchainStoragePush) {
  SwappedBlockchainStorage storage(m_dir.string() + "/items.dat", m_dir.string() + "/index.dat");
  RawBlock block = generateRandomBlocks(1, 0).front();
  storage.pushBlock(std::move(block));
  ASSERT_EQ(1, storage.getBlockCount());
}

TEST_F(BlockchainStorageTests, SwappedBlockchainStorageGetBlock) {
  SwappedBlockchainStorage storage(m_dir.string() + "/items.dat", m_dir.string() + "/index.dat");
  std::vector<RawBlock> blocks = generateRandomBlocks(2, 0);
  std::vector<RawBlock> blocksCopy = blocks;
  storage.pushBlock(std::move(blocksCopy.front()));
  storage.pushBlock(std::move(blocksCopy.back()));
  ASSERT_EQ(2, storage.getBlockCount());
  ASSERT_EQ(blocks.front(), storage.getBlockByIndex(0));
  ASSERT_EQ(blocks.back(), storage.getBlockByIndex(1));
}

TEST_F(BlockchainStorageTests, SwappedBlockchainStorageSplit) {
  const uint32_t BLOCKS_COUNT = 10;
  const uint32_t SPLIT_INDEX = 4;
  SwappedBlockchainStorage storage(m_dir.string() + "/items.dat", m_dir.string() + "/index.dat");
  std::vector<RawBlock> blocks = generateRandomBlocks(BLOCKS_COUNT, 0);
  std::vector<RawBlock> blocksCopy = blocks;
  for (RawBlock& block : blocksCopy) {
    storage.pushBlock(std::move(block));
  }

  ASSERT_EQ(BLOCKS_COUNT, storage.getBlockCount());

  std::unique_ptr<BlockchainStorage::IBlockchainStorageInternal> newStorage = storage.splitStorage(SPLIT_INDEX);
  ASSERT_EQ(SPLIT_INDEX, storage.getBlockCount());
  ASSERT_EQ(BLOCKS_COUNT - SPLIT_INDEX, newStorage->getBlockCount());

  for (uint32_t i = 0; i < SPLIT_INDEX; ++i) {
    ASSERT_EQ(blocks[i], storage.getBlockByIndex(i));
  }

  for (uint32_t i = 0; i < BLOCKS_COUNT - SPLIT_INDEX; ++i) {
    ASSERT_EQ(blocks[SPLIT_INDEX + i], newStorage->getBlockByIndex(i));
  }
}

TEST_F(BlockchainStorageTests, BlockchainStoragePush) {
  BlockchainStorage storage(1);
  RawBlock block = generateRandomBlocks(1, 0).front();
  storage.pushBlock(std::move(block));
  ASSERT_EQ(1, storage.getBlockCount());
}

TEST_F(BlockchainStorageTests, BlockchainStorageGetBlock) {
  BlockchainStorage storage(2);
  std::vector<RawBlock> blocks = generateRandomBlocks(2, 0);
  std::vector<RawBlock> blocksCopy = blocks;
  storage.pushBlock(std::move(blocksCopy.front()));
  storage.pushBlock(std::move(blocksCopy.back()));
  ASSERT_EQ(2, storage.getBlockCount());
  ASSERT_EQ(blocks.front(), storage.getBlockByIndex(0));
  ASSERT_EQ(blocks.back(), storage.getBlockByIndex(1));
}

TEST_F(BlockchainStorageTests, BlockchainStorageSplit) {
  const uint32_t BLOCKS_COUNT = 10;
  const uint32_t SPLIT_INDEX = 4;
  BlockchainStorage storage(BLOCKS_COUNT);
  std::vector<RawBlock> blocks = generateRandomBlocks(BLOCKS_COUNT, 0);
  std::vector<RawBlock> blocksCopy = blocks;
  for (RawBlock& block : blocksCopy) {
    storage.pushBlock(std::move(block));
  }

  ASSERT_EQ(BLOCKS_COUNT, storage.getBlockCount());

  std::unique_ptr<BlockchainStorage> newStorage = storage.splitStorage(SPLIT_INDEX);
  ASSERT_EQ(BLOCKS_COUNT - SPLIT_INDEX, newStorage->getBlockCount());
  ASSERT_EQ(SPLIT_INDEX, storage.getBlockCount());

  for (uint32_t i = 0; i < BLOCKS_COUNT - SPLIT_INDEX; ++i) {
    ASSERT_EQ(blocks[SPLIT_INDEX + i], newStorage->getBlockByIndex(i));
  }

  for (uint32_t i = 0; i < SPLIT_INDEX; ++i) {
    ASSERT_EQ(blocks[i], storage.getBlockByIndex(i));
  }
}
