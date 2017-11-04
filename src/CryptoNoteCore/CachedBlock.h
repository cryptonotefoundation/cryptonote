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

#pragma once

#include <boost/optional.hpp>
#include <CryptoNote.h>

namespace Crypto {

class cn_context;

}

namespace CryptoNote {

class CachedBlock {
public:
  explicit CachedBlock(const BlockTemplate& block);
  const BlockTemplate& getBlock() const;
  const Crypto::Hash& getTransactionTreeHash() const;
  const Crypto::Hash& getBlockHash() const;
  const Crypto::Hash& getBlockLongHash(Crypto::cn_context& cryptoContext) const;
  const Crypto::Hash& getAuxiliaryBlockHeaderHash() const;
  const BinaryArray& getBlockHashingBinaryArray() const;
  const BinaryArray& getParentBlockBinaryArray(bool headerOnly) const;
  const BinaryArray& getParentBlockHashingBinaryArray(bool headerOnly) const;
  uint32_t getBlockIndex() const;

private:
  const BlockTemplate& block;
  mutable boost::optional<BinaryArray> blockHashingBinaryArray;
  mutable boost::optional<BinaryArray> parentBlockBinaryArray;
  mutable boost::optional<BinaryArray> parentBlockHashingBinaryArray;
  mutable boost::optional<BinaryArray> parentBlockBinaryArrayHeaderOnly;
  mutable boost::optional<BinaryArray> parentBlockHashingBinaryArrayHeaderOnly;
  mutable boost::optional<uint32_t> blockIndex;
  mutable boost::optional<Crypto::Hash> transactionTreeHash;
  mutable boost::optional<Crypto::Hash> blockHash;
  mutable boost::optional<Crypto::Hash> blockLongHash;
  mutable boost::optional<Crypto::Hash> auxiliaryBlockHeaderHash;
};

}
