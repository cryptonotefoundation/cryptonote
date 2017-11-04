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

#include "TestNetwork.h"

#include <fstream>
#include <boost/filesystem.hpp>

#include "InProcTestNode.h"
#include "RPCTestNode.h"

#ifdef _WIN32
const std::string bytecoinDaemon = "bytecoind.exe";
#else
const std::string bytecoinDaemon = "bytecoind";
#endif

namespace {

using namespace Tests;

void writeConfiguration(const std::string& confFile, const TestNodeConfiguration& cfg) {
  std::ofstream config(confFile, std::ios_base::trunc | std::ios_base::out);

  config
    << "rpc-bind-port=" << cfg.rpcPort << std::endl
    << "p2p-bind-port=" << cfg.p2pPort << std::endl
    << "log-level=4" << std::endl
    << "log-file=" << cfg.logFile << std::endl;

  for (const auto& ex : cfg.exclusiveNodes) {
    config << "add-exclusive-node=" << ex << std::endl;
  }
}

bool waitDaemonReady(TestNode& node) {
  for (size_t i = 0;; ++i) {
    if (node.getLocalHeight() > 0) {
      break;
    } else if (i < 2 * 60) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } else {
      return false;
    }
  }
  return true;
}

void copyBlockchainFiles(bool testnet, const std::string& from, const std::string& to) {
  boost::filesystem::path fromPath(from);
  boost::filesystem::path toPath(to);

  auto files = {
    std::make_pair("blockindexes.dat", true),
    std::make_pair("blocks.dat", true),
    std::make_pair("blockscache.dat", false),
    std::make_pair("blockchainindices.dat", false)
  };

  for (const auto& item : files) {
    try {
      boost::filesystem::path filePath = std::string(testnet ? "testnet_" : "") + item.first;
      boost::filesystem::copy(fromPath / filePath, toPath / filePath);
    } catch (...) {
      if (item.second) { 
        // if file is required, the rethrow error
        throw;
      }
    }
  }
}

}


namespace Tests {


TestNetworkBuilder::TestNetworkBuilder(size_t nodeCount, Topology topology, uint16_t rpcBasePort, uint16_t p2pBasePort) :
  nodeCount(nodeCount), 
  topology(topology), 
  rpcBasePort(rpcBasePort), 
  p2pBasePort(p2pBasePort),
  baseDataDir("."),
  testnet(true)
{}

std::vector<TestNodeConfiguration> TestNetworkBuilder::build() {
  std::vector<TestNodeConfiguration> cfg;

  for (size_t i = 0; i < nodeCount; ++i) {
    cfg.push_back(buildNodeConfiguration(i));
  }

  return cfg;
}

TestNetworkBuilder& TestNetworkBuilder::setDataDirectory(const std::string& dataDir) {
  this->baseDataDir = dataDir;
  return *this;
}

TestNetworkBuilder& TestNetworkBuilder::setBlockchain(const std::string& blockchainDir) {
  this->blockchainLocation = blockchainDir;
  return *this;
}

TestNetworkBuilder& TestNetworkBuilder::setTestnet(bool isTestnet) {
  this->testnet = isTestnet;
  return *this;
}

TestNodeConfiguration TestNetworkBuilder::buildNodeConfiguration(size_t index) {
  TestNodeConfiguration cfg;

  if (!baseDataDir.empty()) {
    cfg.dataDir = baseDataDir + "/node" + std::to_string(index);
  }

  if (!blockchainLocation.empty()) {
    cfg.blockchainLocation = blockchainLocation;
  }

  cfg.daemonPath = bytecoinDaemon; // default
  cfg.testnet = testnet;
  cfg.logFile = "test_bytecoind" + std::to_string(index) + ".log";

  uint16_t rpcPort = static_cast<uint16_t>(rpcBasePort + index);
  uint16_t p2pPort = static_cast<uint16_t>(p2pBasePort + index);

  cfg.p2pPort = p2pPort;
  cfg.rpcPort = rpcPort;

  switch (topology) {
  case Topology::Line:
    if (index != 0) {
      cfg.exclusiveNodes.push_back("127.0.0.1:" + std::to_string(p2pPort - 1));
    }
    break;

  case Topology::Ring: {
    uint16_t p2pExternalPort = static_cast<uint16_t>(p2pBasePort + (index + 1) % nodeCount);
    cfg.exclusiveNodes.push_back("127.0.0.1:" + std::to_string(p2pExternalPort));
    break;
  }

  case Topology::Star:
    if (index == 0) {
      for (size_t node = 1; node < nodeCount; ++node) {
        cfg.exclusiveNodes.push_back("127.0.0.1:" + std::to_string(p2pBasePort + node));
      }
    }
    break;
  }

  return cfg;

}

TestNetwork::TestNetwork(System::Dispatcher& dispatcher, const CryptoNote::Currency& currency) : 
  m_dispatcher(dispatcher),
  m_currency(currency) {
}

void TestNetwork::addNodes(const std::vector<TestNodeConfiguration>& nodes) {
  for (const auto& n : nodes) {
    addNode(n);
  }
}

void TestNetwork::addNode(const TestNodeConfiguration& cfg) {
  std::unique_ptr<TestNode> node;

  boost::system::error_code ec;
  boost::filesystem::remove_all(cfg.dataDir, ec);
  boost::filesystem::create_directory(cfg.dataDir);

  if (!cfg.blockchainLocation.empty()) {
    copyBlockchainFiles(cfg.testnet, cfg.blockchainLocation, cfg.dataDir);
  }

  switch (cfg.nodeType) {
  case NodeType::InProcess:
    node.reset(new InProcTestNode(cfg, m_currency, m_dispatcher));
    break;
  case NodeType::RPC:
    node = startDaemon(cfg);
    break;
  }

  nodes.push_back(std::make_pair(std::move(node), cfg));
}

void TestNetwork::waitNodesReady() {
  for (auto& node : nodes) {
    if (!waitDaemonReady(*node.first)) {
      throw std::runtime_error("Daemon startup failure (timeout)");
    }
  }
}

TestNode& TestNetwork::getNode(size_t index) {
  if (index >= nodes.size()) {
    throw std::runtime_error("Invalid node index");
  }

  return *nodes[index].first;
}

void TestNetwork::shutdown() {
  for (auto& node : nodes) {
    node.first->stopDaemon();
  }

  for (auto& daemon : m_daemons) {
    daemon.wait();
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));

  for (auto& node : nodes) {
    if (node.second.cleanupDataDir) {
      boost::filesystem::remove_all(node.second.dataDir);
    }
  }

}


std::unique_ptr<TestNode> TestNetwork::startDaemon(const TestNodeConfiguration& cfg) {
  if (!boost::filesystem::exists(cfg.daemonPath)) {
    throw std::runtime_error("daemon binary wasn't found");
  }

  writeConfiguration(cfg.dataDir + "/daemon.conf", cfg);

  Process process;
  std::vector<std::string> daemonArgs = { "--data-dir=" + cfg.dataDir, "--config-file=daemon.conf" };

  if (cfg.testnet) {
    daemonArgs.emplace_back("--testnet");
  }

  process.startChild(cfg.daemonPath, daemonArgs);

  std::unique_ptr<TestNode> node(new RPCTestNode(cfg.rpcPort, m_dispatcher));
  m_daemons.push_back(process);

  return node;
}





}
