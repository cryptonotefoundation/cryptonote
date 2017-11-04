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

#include <chrono>
#include "NetNodeConfig.h"

namespace CryptoNote {

class P2pNodeConfig : public NetNodeConfig {
public:
  P2pNodeConfig();

  // getters
  std::chrono::nanoseconds getTimedSyncInterval() const;
  std::chrono::nanoseconds getHandshakeTimeout() const;
  std::chrono::nanoseconds getConnectInterval() const;
  std::chrono::nanoseconds getConnectTimeout() const;
  size_t getExpectedOutgoingConnectionsCount() const;
  size_t getWhiteListConnectionsPercent() const;
  boost::uuids::uuid getNetworkId() const;
  size_t getPeerListConnectRange() const;
  size_t getPeerListGetTryCount() const;

  // setters
  void setTimedSyncInterval(std::chrono::nanoseconds interval);
  void setHandshakeTimeout(std::chrono::nanoseconds timeout);
  void setConnectInterval(std::chrono::nanoseconds interval);
  void setConnectTimeout(std::chrono::nanoseconds timeout);
  void setExpectedOutgoingConnectionsCount(size_t count);
  void setWhiteListConnectionsPercent(size_t percent);
  void setNetworkId(const boost::uuids::uuid& id);
  void setPeerListConnectRange(size_t range);
  void setPeerListGetTryCount(size_t count);

private:
  std::chrono::nanoseconds timedSyncInterval;
  std::chrono::nanoseconds handshakeTimeout;
  std::chrono::nanoseconds connectInterval;
  std::chrono::nanoseconds connectTimeout;
  boost::uuids::uuid networkId;
  size_t expectedOutgoingConnectionsCount;
  size_t whiteListConnectionsPercent;
  size_t peerListConnectRange;
  size_t peerListGetTryCount;
};

}
