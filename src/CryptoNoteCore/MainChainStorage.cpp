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

#include "MainChainStorage.h"

#include <boost/filesystem.hpp>

#include "CryptoNoteTools.h"

namespace CryptoNote {

const size_t STORAGE_CACHE_SIZE = 100;

MainChainStorage::MainChainStorage(const std::string& blocksFilename, const std::string& indexesFilename) {
  if (!storage.open(blocksFilename, indexesFilename, STORAGE_CACHE_SIZE)) {
    throw std::runtime_error("Failed to load main chain storage: " + blocksFilename);
  }
}

MainChainStorage::~MainChainStorage() {
  storage.close();
}

void MainChainStorage::pushBlock(const RawBlock& rawBlock) {
  storage.push_back(rawBlock);
}

void MainChainStorage::popBlock() {
  storage.pop_back();
}

RawBlock MainChainStorage::getBlockByIndex(uint32_t index) const {
  if (index >= storage.size()) {
    throw std::out_of_range("Block index " + std::to_string(index) + " is out of range. Blocks count: " + std::to_string(storage.size()));
  }

  return storage[index];
}

uint32_t MainChainStorage::getBlockCount() const {
  return static_cast<uint32_t>(storage.size());
}

void MainChainStorage::clear() {
  storage.clear();
}

std::unique_ptr<IMainChainStorage> createSwappedMainChainStorage(const std::string& dataDir, const Currency& currency) {
  boost::filesystem::path blocksFilename = boost::filesystem::path(dataDir) / currency.blocksFileName();
  boost::filesystem::path indexesFilename = boost::filesystem::path(dataDir) / currency.blockIndexesFileName();

  std::unique_ptr<IMainChainStorage> storage(new MainChainStorage(blocksFilename.string(), indexesFilename.string()));
  if (storage->getBlockCount() == 0) {
    RawBlock genesis;
    genesis.block = toBinaryArray(currency.genesisBlock());
    storage->pushBlock(genesis);
  }

  return storage;
}

}
