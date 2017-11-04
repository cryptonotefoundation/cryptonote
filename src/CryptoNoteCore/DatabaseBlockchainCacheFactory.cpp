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

#include "DatabaseBlockchainCacheFactory.h"

#include "IDataBase.h"

#include "BlockchainCache.h"
#include "DatabaseBlockchainCache.h"

namespace CryptoNote {

DatabaseBlockchainCacheFactory::DatabaseBlockchainCacheFactory(IDataBase& database, Logging::ILogger& logger): database(database), logger(logger) {

}

DatabaseBlockchainCacheFactory::~DatabaseBlockchainCacheFactory() {

}

std::unique_ptr<IBlockchainCache> DatabaseBlockchainCacheFactory::createRootBlockchainCache(const Currency& currency) {
  return std::unique_ptr<IBlockchainCache> (new DatabaseBlockchainCache(currency, database, *this, logger));
}

std::unique_ptr<IBlockchainCache> DatabaseBlockchainCacheFactory::createBlockchainCache(const Currency& currency, IBlockchainCache* parent, uint32_t startIndex) {
  return std::unique_ptr<IBlockchainCache> (new BlockchainCache("", currency, logger, parent, startIndex));
}

} //namespace CryptoNote
