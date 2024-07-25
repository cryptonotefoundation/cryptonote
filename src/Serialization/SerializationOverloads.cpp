// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Serialization/SerializationOverloads.h"
#include <stdexcept>

#include <limits>

namespace CryptoNote {

void serializeBlockHeight(ISerializer& s, uint32_t& blockHeight, Common::StringView name) {
  if (s.type() == ISerializer::INPUT) {
    uint64_t height;
    s(height, name);

    if (height == std::numeric_limits<uint64_t>::max()) {
      blockHeight = std::numeric_limits<uint32_t>::max();
    } else if (height > std::numeric_limits<uint32_t>::max() && height < std::numeric_limits<uint64_t>::max()) {
      throw std::runtime_error("Deserialization error: wrong value");
    } else {
      blockHeight = static_cast<uint32_t>(height);
    }
  } else {
    s(blockHeight, name);
  }
}

void serializeGlobalOutputIndex(ISerializer& s, uint32_t& globalOutputIndex, Common::StringView name) {
  serializeBlockHeight(s, globalOutputIndex, name);
}

} //namespace CryptoNote
