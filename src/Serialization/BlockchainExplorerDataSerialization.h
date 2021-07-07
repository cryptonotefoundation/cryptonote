// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "BlockchainExplorerData.h"

#include "Serialization/ISerializer.h"

namespace CryptoNote {

void serialize(transactionOutputDetails2& output, ISerializer& serializer);
void serialize(TransactionOutputReferenceDetails& outputReference, ISerializer& serializer);

void serialize(BaseInputDetails& inputBase, ISerializer& serializer);
void serialize(KeyInputDetails& inputToKey, ISerializer& serializer);
void serialize(MultisignatureInputDetails& inputMultisig, ISerializer& serializer);
void serialize(transactionInputDetails2& input, ISerializer& serializer);

void serialize(TransactionExtraDetails& extra, ISerializer& serializer);
void serialize(TransactionExtraDetails2& extra, ISerializer& serializer);
void serialize(TransactionDetails& transaction, ISerializer& serializer);

void serialize(BlockDetails& block, ISerializer& serializer);

} //namespace CryptoNote
