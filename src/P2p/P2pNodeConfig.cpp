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

#include "P2pNodeConfig.h"
#include "P2pNetworks.h"

#include <CryptoNoteConfig.h>

namespace CryptoNote {

namespace {

const std::chrono::nanoseconds P2P_DEFAULT_CONNECT_INTERVAL = std::chrono::seconds(2);
const size_t P2P_DEFAULT_CONNECT_RANGE = 20;
const size_t P2P_DEFAULT_PEERLIST_GET_TRY_COUNT = 10;

}

P2pNodeConfig::P2pNodeConfig() :
  timedSyncInterval(std::chrono::seconds(P2P_DEFAULT_HANDSHAKE_INTERVAL)),
  handshakeTimeout(std::chrono::milliseconds(P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT)),
  connectInterval(P2P_DEFAULT_CONNECT_INTERVAL),
  connectTimeout(std::chrono::milliseconds(P2P_DEFAULT_CONNECTION_TIMEOUT)),
  networkId(BYTECOIN_NETWORK),
  expectedOutgoingConnectionsCount(P2P_DEFAULT_CONNECTIONS_COUNT),
  whiteListConnectionsPercent(P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT),
  peerListConnectRange(P2P_DEFAULT_CONNECT_RANGE),
  peerListGetTryCount(P2P_DEFAULT_PEERLIST_GET_TRY_COUNT) {
}

// getters

std::chrono::nanoseconds P2pNodeConfig::getTimedSyncInterval() const {
  return timedSyncInterval;
}

std::chrono::nanoseconds P2pNodeConfig::getHandshakeTimeout() const {
  return handshakeTimeout;
}

std::chrono::nanoseconds P2pNodeConfig::getConnectInterval() const {
  return connectInterval;
}

std::chrono::nanoseconds P2pNodeConfig::getConnectTimeout() const {
  return connectTimeout;
}

size_t P2pNodeConfig::getExpectedOutgoingConnectionsCount() const {
  return expectedOutgoingConnectionsCount;
}

size_t P2pNodeConfig::getWhiteListConnectionsPercent() const {
  return whiteListConnectionsPercent;
}

boost::uuids::uuid P2pNodeConfig::getNetworkId() const {
  if (getTestnet()) {
    boost::uuids::uuid copy = networkId;
    copy.data[0] += 1;
    return copy;
  }
  return networkId;
}

size_t P2pNodeConfig::getPeerListConnectRange() const {
  return peerListConnectRange;
}

size_t P2pNodeConfig::getPeerListGetTryCount() const {
  return peerListGetTryCount;
}

// setters

void P2pNodeConfig::setTimedSyncInterval(std::chrono::nanoseconds interval) {
  timedSyncInterval = interval;
}

void P2pNodeConfig::setHandshakeTimeout(std::chrono::nanoseconds timeout) {
  handshakeTimeout = timeout;
}

void P2pNodeConfig::setConnectInterval(std::chrono::nanoseconds interval) {
  connectInterval = interval;
}

void P2pNodeConfig::setConnectTimeout(std::chrono::nanoseconds timeout) {
  connectTimeout = timeout;
}

void P2pNodeConfig::setExpectedOutgoingConnectionsCount(size_t count) {
  expectedOutgoingConnectionsCount = count;
}

void P2pNodeConfig::setWhiteListConnectionsPercent(size_t percent) {
  if (percent > 100) {
    throw std::invalid_argument("whiteListConnectionsPercent cannot be greater than 100");
  }

  whiteListConnectionsPercent = percent;
}

void P2pNodeConfig::setNetworkId(const boost::uuids::uuid& id) {
  networkId = id;
}

void P2pNodeConfig::setPeerListConnectRange(size_t range) {
  peerListConnectRange = range;
}

void P2pNodeConfig::setPeerListGetTryCount(size_t count) {
  peerListGetTryCount = count;
}

}
