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

#include "CryptoNoteCore/BlockchainCache.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionValidatiorState.h"
#include "Logging/FileLogger.h"
#include "TestBlockchainGenerator.h"

using namespace CryptoNote;

class BlockchainCacheTests : public ::testing::Test {
public:
  BlockchainCacheTests() :
    currency(CurrencyBuilder(logger).currency()),
    blockCache("cache", currency, logger, nullptr),
    generator(currency) {
  }

  Currency currency;
  Logging::FileLogger logger;
  BlockchainCache blockCache;
  TestBlockchainGenerator generator;
};

TEST_F(BlockchainCacheTests, getParentNull) {
  ASSERT_EQ(nullptr, blockCache.getParent());
}

TEST_F(BlockchainCacheTests, getBlockCountGenesis) {
  ASSERT_EQ(1, blockCache.getBlockCount());
}

TEST_F(BlockchainCacheTests, pushBlockMany) {
  const size_t BLOCK_COUNT = 10;
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  generator.generateEmptyBlocks(BLOCK_COUNT);
  auto bcCopy = generator.getBlockchainCopy();
  for (size_t i = 1; i < bcCopy.size(); ++i) { //Skip genesis block
    ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(CachedBlock(bcCopy.at(i)), transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  }

  ASSERT_EQ(generator.getBlockchain().size(), blockCache.getBlockCount());
}

TEST_F(BlockchainCacheTests, getTopBlockIndex) {
  ASSERT_EQ(0, blockCache.getTopBlockIndex());
}

TEST_F(BlockchainCacheTests, getTopBlockIndexChain) {
  const CachedBlock block(generator.getBlockchain().front());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;

  const CachedBlock nextBlock(generator.getBlockchain().back());
  BlockchainCache otherCache("cache", currency, logger, &blockCache, nextBlock.getBlockIndex());
  ASSERT_NO_FATAL_FAILURE(otherCache.pushBlock(nextBlock, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  ASSERT_EQ(1, otherCache.getTopBlockIndex());
}

TEST_F(BlockchainCacheTests, getTopBlockHash) {
  const CachedBlock block(generator.getBlockchain().back());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  ASSERT_EQ(block.getBlockHash(), blockCache.getTopBlockHash());
}

TEST_F(BlockchainCacheTests, hasBlock) {
  const CachedBlock block(generator.getBlockchain().back());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  ASSERT_TRUE(blockCache.hasBlock(block.getBlockHash()));
}

TEST_F(BlockchainCacheTests, getBlockIndexEmptyThrows) {
  const CachedBlock block(generator.getBlockchain().back());
  ASSERT_ANY_THROW(blockCache.getBlockIndex(block.getBlockHash()));
}

TEST_F(BlockchainCacheTests, getBlockIndex) {
  const CachedBlock block(generator.getBlockchain().back());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  ASSERT_EQ(block.getBlockIndex(), blockCache.getBlockIndex(block.getBlockHash()));
}

TEST_F(BlockchainCacheTests, getBlockHash) {
  const CachedBlock block(generator.getBlockchain().back());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  ASSERT_EQ(block.getBlockHash(), blockCache.getBlockHash(block.getBlockIndex()));
}

TEST_F(BlockchainCacheTests, getBlockHashes) {
  const size_t START_INDEX = 0;
  const size_t BLOCK_COUNT = 10;
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  generator.generateEmptyBlocks(BLOCK_COUNT);
  std::vector<Crypto::Hash> expected;
  auto bcCopy = generator.getBlockchainCopy();
  std::transform(std::begin(bcCopy), std::end(bcCopy), std::back_inserter(expected), [](const BlockTemplate& block){
    return CachedBlock(block).getBlockHash();
  });
  for (size_t i = 1; i < bcCopy.size(); ++i) { //Skip genesis block
    const CachedBlock block(bcCopy.at(i));
    ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  }

  auto actual = blockCache.getBlockHashes(START_INDEX, generator.getBlockchain().size());
  ASSERT_EQ(expected, actual);
}

TEST_F(BlockchainCacheTests, getBlockHashesStartIndex) {
  const size_t BLOCK_COUNT = 10;
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  generator.generateEmptyBlocks(BLOCK_COUNT);
  std::vector<Crypto::Hash> expected;
  auto bcCopy = generator.getBlockchainCopy();
  std::transform(std::begin(bcCopy), std::end(bcCopy), std::back_inserter(expected), [](const BlockTemplate& block){
    return CachedBlock(block).getBlockHash();
  });
  for (size_t i = 1; i < bcCopy.size(); ++i) { //Skip genesis block
    const CachedBlock block(bcCopy.at(i));
    ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  }

  for (uint32_t i = 0; !expected.empty(); ++i) {
    auto actual = blockCache.getBlockHashes(i, generator.getBlockchain().size());
    ASSERT_EQ(expected, actual);
    expected.erase(expected.begin());
  }
}

TEST_F(BlockchainCacheTests, getBlockHashesMaxCount) {
  const size_t START_INDEX = 0;
  const size_t BLOCK_COUNT = 10;
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  generator.generateEmptyBlocks(BLOCK_COUNT);
  std::vector<Crypto::Hash> expected;
  auto bcCopy = generator.getBlockchainCopy();
  std::transform(std::begin(bcCopy), std::end(bcCopy), std::back_inserter(expected), [](const BlockTemplate& block){
    return CachedBlock(block).getBlockHash();
  });
  for (size_t i = 1; i < bcCopy.size(); ++i) { //Skip genesis block
    const CachedBlock block(bcCopy.at(i));
    ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  }

  for (size_t i = generator.getBlockchain().size(); !expected.empty(); --i) {
    auto actual = blockCache.getBlockHashes(START_INDEX, i);
    ASSERT_EQ(expected, actual);
    expected.pop_back();
  }
  auto actual = blockCache.getBlockHashes(START_INDEX, 0);
  ASSERT_EQ(expected, actual);
}

TEST_F(BlockchainCacheTests, getBlockHashesChained) {
  const size_t START_INDEX = 0;
  const size_t BLOCK_COUNT = 10;
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  generator.generateEmptyBlocks(BLOCK_COUNT);
  std::vector<Crypto::Hash> expected;
  auto bcCopy = generator.getBlockchainCopy();
  std::transform(std::begin(bcCopy), std::end(bcCopy), std::back_inserter(expected), [](const BlockTemplate& block){
    return CachedBlock(block).getBlockHash();
  });
  for (size_t i = 1; i < bcCopy.size(); ++i) { //Skip genesis block
    const CachedBlock block(bcCopy.at(i));
    ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  }

  uint32_t start = static_cast<uint32_t>(generator.getBlockchain().size());
  BlockchainCache otherCache("cache", currency, logger, &blockCache, start);
  generator.generateEmptyBlocks(BLOCK_COUNT);
  for (size_t i = start; i < generator.getBlockchain().size(); ++i) {
    const CachedBlock block(generator.getBlockchain()[i]);
    expected.push_back(block.getBlockHash());
    ASSERT_NO_FATAL_FAILURE(otherCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  }

  auto actual = otherCache.getBlockHashes(START_INDEX, generator.getBlockchain().size());
  ASSERT_EQ(expected, actual);
}

TEST_F(BlockchainCacheTests, split) {
  const uint32_t SPLIT_HEIGHT = 3;
  const size_t BLOCK_COUNT = 10;
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  generator.generateEmptyBlocks(BLOCK_COUNT);
  auto bcCopy = generator.getBlockchainCopy();
  for (size_t i = 1; i < bcCopy.size(); ++i) { //Skip genesis block
    const CachedBlock block(bcCopy.at(i));
    ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));
  }

  std::unique_ptr<IBlockchainCache> otherCache;
  ASSERT_NO_FATAL_FAILURE(otherCache = blockCache.split(SPLIT_HEIGHT));
  ASSERT_EQ(generator.getBlockchain().size() - SPLIT_HEIGHT, otherCache->getBlockCount());
  ASSERT_EQ(SPLIT_HEIGHT, blockCache.getBlockCount());
}

TEST_F(BlockchainCacheTests, checkIfSpentFalse) {
  Crypto::KeyImage keyImage = Crypto::rand<Crypto::KeyImage>();
  ASSERT_FALSE(blockCache.checkIfSpent(keyImage));
}

TEST_F(BlockchainCacheTests, checkIfSpentTrue) {
  const CachedBlock block(generator.getBlockchain().back());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  Crypto::KeyImage keyImage = Crypto::rand<Crypto::KeyImage>();
  validatorState.spentKeyImages.insert(keyImage);
  ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));

  ASSERT_TRUE(blockCache.checkIfSpent(keyImage));
}

TEST_F(BlockchainCacheTests, checkIfSpentChain) {
  const CachedBlock block(generator.getBlockchain().back());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  Crypto::KeyImage keyImage = Crypto::rand<Crypto::KeyImage>();
  validatorState.spentKeyImages.insert(keyImage);
  ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));

  BlockchainCache otherCache("cache", currency, logger, &blockCache);
  ASSERT_TRUE(otherCache.checkIfSpent(keyImage));
}

TEST_F(BlockchainCacheTests, checkIfSpentBlockIndexFalse) {
  Crypto::KeyImage keyImage = Crypto::rand<Crypto::KeyImage>();
  ASSERT_FALSE(blockCache.checkIfSpent(keyImage, 0));
}

TEST_F(BlockchainCacheTests, checkIfSpentBlockIndexTrue) {
  const CachedBlock block(generator.getBlockchain().back());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  Crypto::KeyImage keyImage = Crypto::rand<Crypto::KeyImage>();
  validatorState.spentKeyImages.insert(keyImage);
  ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));

  ASSERT_TRUE(blockCache.checkIfSpent(keyImage, 1));
}

TEST_F(BlockchainCacheTests, checkIfSpentChain1) {
  const CachedBlock block(generator.getBlockchain().back());
  const uint64_t REWARD = rand();
  const uint64_t SIZE = rand();
  const Difficulty DIFFICULTY = rand();
  std::vector<CachedTransaction> transactions;
  TransactionValidatorState validatorState;
  Crypto::KeyImage keyImage = Crypto::rand<Crypto::KeyImage>();
  validatorState.spentKeyImages.insert(keyImage);
  ASSERT_NO_FATAL_FAILURE(blockCache.pushBlock(block, transactions, validatorState, SIZE, REWARD, DIFFICULTY, RawBlock()));

  BlockchainCache otherCache("cache", currency, logger, &blockCache);
  ASSERT_TRUE(otherCache.checkIfSpent(keyImage, 1));
}
