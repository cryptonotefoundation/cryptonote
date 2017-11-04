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

#include "KeysStorage.h"

#include "WalletLegacy/WalletLegacySerialization.h"
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"

namespace CryptoNote {

void KeysStorage::serialize(ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);

  serializer(creationTimestamp, "creation_timestamp");

  serializer(spendPublicKey, "spend_public_key");
  serializer(spendSecretKey, "spend_secret_key");

  serializer(viewPublicKey, "view_public_key");
  serializer(viewSecretKey, "view_secret_key");

  serializer.endObject();
}

}
