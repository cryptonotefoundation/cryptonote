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

#include "NetworkConfiguration.h"
#include "Process.h"
#include "TestNode.h"

namespace System {
class Dispatcher;
}

namespace CryptoNote {
class Currency;
}

namespace Tests {

enum class Topology {
  Ring,
  Line,
  Star
};


class TestNetworkBuilder {
public:

  TestNetworkBuilder(size_t nodeCount, Topology topology = Topology::Line, uint16_t rpcBasePort = 9200, uint16_t p2pBasePort = 9000);

  TestNetworkBuilder& setDataDirectory(const std::string& dataDir);
  TestNetworkBuilder& setBlockchain(const std::string& blockchainDir);
  TestNetworkBuilder& setTestnet(bool isTestnet);

  std::vector<TestNodeConfiguration> build();

private:

  TestNodeConfiguration buildNodeConfiguration(size_t index);

  uint16_t rpcBasePort;
  uint16_t p2pBasePort;
  Topology topology;
  size_t nodeCount;
  std::string baseDataDir;
  std::string blockchainLocation;
  bool testnet;
};


class TestNetwork {

public:

  TestNetwork(System::Dispatcher& dispatcher, const CryptoNote::Currency& currency);

  void addNodes(const std::vector<TestNodeConfiguration>& nodes);
  void addNode(const TestNodeConfiguration& cfg);
  void waitNodesReady();
  void shutdown();

  TestNode& getNode(size_t index);

private:

  std::unique_ptr<TestNode> startDaemon(const TestNodeConfiguration& cfg);

  std::vector<std::pair<std::unique_ptr<TestNode>, TestNodeConfiguration>> nodes;
  System::Dispatcher& m_dispatcher;
  const CryptoNote::Currency& m_currency;
  std::vector<Process> m_daemons;
};

}
