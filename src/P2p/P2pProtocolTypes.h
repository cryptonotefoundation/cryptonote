// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2017, The Monero project
// Copyright (c) 2016-2020, The Karbo developers
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

#include <string.h>
#include <tuple>
#include <boost/uuid/uuid.hpp>
#include "android.h"
#include "Common/StringTools.h"

namespace CryptoNote
{
  typedef boost::uuids::uuid uuid;
  typedef boost::uuids::uuid net_connection_id;
  typedef uint64_t PeerIdType;

#pragma pack (push, 1)
  
  struct NetworkAddress
  {
    uint32_t ip;
    uint32_t port;
  };

  struct PeerlistEntry
  {
    NetworkAddress adr;
    PeerIdType id;
    uint64_t last_seen;
  };

  struct AnchorPeerlistEntry
  {
    NetworkAddress adr;
    PeerIdType id;
    int64_t first_seen;
  };

  struct connection_entry
  {
    NetworkAddress adr;
    PeerIdType id;
    bool is_income;
  };

#pragma pack(pop)

  inline bool operator < (const NetworkAddress& a, const NetworkAddress& b) {
    return std::tie(a.ip, a.port) < std::tie(b.ip, b.port);
  }

  inline bool operator == (const NetworkAddress& a, const NetworkAddress& b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
  }

  inline std::ostream& operator << (std::ostream& s, const NetworkAddress& na) {
    return s << Common::ipAddressToString(na.ip) << ":" << std::to_string(na.port);   
  }

  inline uint32_t hostToNetwork(uint32_t n) {
    return (n << 24) | (n & 0xff00) << 8 | (n & 0xff0000) >> 8 | (n >> 24);
  }

  inline uint32_t networkToHost(uint32_t n) {
    return hostToNetwork(n); // the same
  }

}
