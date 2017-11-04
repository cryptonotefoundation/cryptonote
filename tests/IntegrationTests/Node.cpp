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

#include <gtest/gtest.h>

#include <fstream>

#include <Common/StringTools.h>
#include <System/Dispatcher.h>
#include <System/Timer.h>

#include <Serialization/JsonOutputStreamSerializer.h>
#include <Serialization/JsonInputStreamSerializer.h>
#include <Serialization/SerializationOverloads.h>

#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "Wallet/WalletGreen.h"

#include "../IntegrationTestLib/TestNetwork.h"
#include "../IntegrationTestLib/NodeObserver.h"
#include "../IntegrationTestLib/NodeCallback.h"

#include "BaseTests.h"

using namespace Tests;
using namespace CryptoNote;

namespace CryptoNote {
  void serialize(BlockShortEntry& v, ISerializer& s) {
    s(v.blockHash, "hash");
    
    if (s.type() == ISerializer::INPUT) {
      std::string blockBinary;
      if (s.binary(blockBinary, "block")) {
        fromBinaryArray(v.block, Common::asBinaryArray(blockBinary));
        v.hasBlock = true;
      }
    } else {
      if (v.hasBlock) {
        std::string blockBinary(Common::asString(toBinaryArray(v.block)));
        s.binary(blockBinary, "block");
      }
    }

    s(v.txsShortInfo, "transactions");
  }

  void serialize(TransactionShortInfo& v, ISerializer& s) {
    s(v.txId, "hash");
    s(v.txPrefix, "prefix");
  }

  bool operator == (const BlockShortEntry& a, const BlockShortEntry& b) {
    return
      a.blockHash == b.blockHash &&
      a.hasBlock == b.hasBlock &&
      // a.block == b.block &&
      a.txsShortInfo == b.txsShortInfo;
  }

  bool operator == (const TransactionShortInfo& a, const TransactionShortInfo& b) {
    return a.txId == b.txId;
  }
}

struct BlockchainInfo {
  std::list<BlockShortEntry> blocks;
  std::unordered_map<Crypto::Hash, std::vector<uint32_t>> globalOutputs;

  bool operator == (const BlockchainInfo& other) const {
    return blocks == other.blocks && globalOutputs == other.globalOutputs;
  }

  void serialize(ISerializer& s) {
    s(blocks, "blocks");
    s(globalOutputs, "outputs");
  }
};

void storeBlockchainInfo(const std::string& filename, BlockchainInfo& bc) {
  JsonOutputStreamSerializer s;
  serialize(bc, s);

  std::ofstream jsonBlocks(filename, std::ios::trunc);
  jsonBlocks << s.getValue();
}

void loadBlockchainInfo(const std::string& filename, BlockchainInfo& bc) {
  std::ifstream jsonBlocks(filename);
  JsonInputStreamSerializer s(jsonBlocks);
  serialize(bc, s);
}


class NodeTest: public BaseTest {
protected:
  void startNetworkWithBlockchain(const std::string& sourcePath);
  void readBlockchainInfo(INode& node, BlockchainInfo& bc);
  void dumpBlockchainInfo(INode& node);

  const std::string TEST_WALLET_FILE = "wallet.bin";
};

void NodeTest::startNetworkWithBlockchain(const std::string& sourcePath) {
  auto networkCfg = TestNetworkBuilder(2, Topology::Ring).build();

  for (auto& node : networkCfg) {
    node.blockchainLocation = sourcePath;
  }

  network.addNodes(networkCfg);
  network.waitNodesReady();
}

void NodeTest::readBlockchainInfo(INode& node, BlockchainInfo& bc) {
  std::vector<Crypto::Hash> history = { currency.genesisBlockHash() };
  uint64_t timestamp = 0;
  uint32_t startHeight = 0;
  size_t itemsAdded = 0;
  NodeCallback cb;

  bc.blocks = {
    BlockShortEntry{ currency.genesisBlockHash(), true, currency.genesisBlock() }
  };

  do {
    itemsAdded = 0;
    std::vector<BlockShortEntry> blocks;
    node.queryBlocks(std::vector<Crypto::Hash>(history.rbegin(), history.rend()), timestamp, blocks, startHeight, cb.callback());

    ASSERT_TRUE(cb.get() == std::error_code());

    uint64_t currentHeight = startHeight;

    for (auto& entry : blocks) {

      if (currentHeight < history.size()) {
        // detach no expected
        ASSERT_EQ(entry.blockHash, history[currentHeight]);
      } else {
        auto txHash = getObjectHash(entry.block.baseTransaction);

        std::vector<uint32_t> globalIndices;
        node.getTransactionOutsGlobalIndices(txHash, globalIndices, cb.callback());

        ASSERT_TRUE(!cb.get());

        bc.globalOutputs.insert(std::make_pair(txHash, std::move(globalIndices)));

        bc.blocks.push_back(entry);
        history.push_back(entry.blockHash);
        ++itemsAdded;
      }

      ++currentHeight;
    }
  } while (itemsAdded > 0);
}

void NodeTest::dumpBlockchainInfo(INode& node) {
  BlockchainInfo bc;
  ASSERT_NO_FATAL_FAILURE(readBlockchainInfo(node, bc));
  storeBlockchainInfo("blocks.js", bc);
}


TEST_F(NodeTest, generateBlockchain) {
  if (boost::filesystem::exists(TEST_WALLET_FILE)) {
    boost::filesystem::remove(TEST_WALLET_FILE);
  }

  auto networkCfg = TestNetworkBuilder(2, Topology::Ring).build();
  networkCfg[0].cleanupDataDir = false;
  network.addNodes(networkCfg);
  network.waitNodesReady();

  auto& daemon = network.getNode(0);

  {
    std::unique_ptr<INode> mainNode;
    ASSERT_TRUE(daemon.makeINode(mainNode));

    std::string password = "pass";
    CryptoNote::WalletGreen wallet(dispatcher, currency, *mainNode, logger);

    wallet.initialize(TEST_WALLET_FILE, password);

    std::string minerAddress = wallet.createAddress();
    daemon.startMining(1, minerAddress);

    System::Timer timer(dispatcher);

    while (daemon.getLocalHeight() < 300) {
      std::cout << "Waiting for block..." << std::endl;
      timer.sleep(std::chrono::seconds(10));
    }

    daemon.stopMining();

    wallet.save();
    wallet.shutdown();

    dumpBlockchainInfo(*mainNode);
 }
}

TEST_F(NodeTest, dumpBlockchain) {
  startNetworkWithBlockchain("testnet_300");
  //auto& daemon = network.getNode(0);
  auto mainNode = network.getNode(0).makeINode();
  dumpBlockchainInfo(*mainNode);
}

TEST_F(NodeTest, addMoreBlocks) {
  auto networkCfg = TestNetworkBuilder(2, Topology::Ring).build();
  networkCfg[0].cleanupDataDir = false;
  networkCfg[0].blockchainLocation = "testnet_300";
  networkCfg[1].blockchainLocation = "testnet_300";
  network.addNodes(networkCfg);
  network.waitNodesReady();

  auto& daemon = network.getNode(0);

  {
    std::unique_ptr<INode> mainNode;
    ASSERT_TRUE(daemon.makeINode(mainNode));

    auto startHeight = daemon.getLocalHeight();

    std::string password = "pass";
    CryptoNote::WalletGreen wallet(dispatcher, currency, *mainNode, logger);
    wallet.load(TEST_WALLET_FILE, password);

    std::string minerAddress = wallet.getAddress(0);
    daemon.startMining(1, minerAddress);

    System::Timer timer(dispatcher);

    while (daemon.getLocalHeight() <= startHeight + 3) {
      std::cout << "Waiting for block..." << std::endl;
      timer.sleep(std::chrono::seconds(1));
    }

    daemon.stopMining();

    wallet.save();
    wallet.shutdown();

    dumpBlockchainInfo(*mainNode);
  }
}


TEST_F(NodeTest, queryBlocks) {
  BlockchainInfo knownBc, nodeBc;

  loadBlockchainInfo("blocks.js", knownBc);

  startNetworkWithBlockchain("testnet_300");
  auto& daemon = network.getNode(0);
  std::unique_ptr<INode> mainNode;

  // check full sync

  ASSERT_TRUE(daemon.makeINode(mainNode));
  ASSERT_NO_FATAL_FAILURE(readBlockchainInfo(*mainNode, nodeBc));
  ASSERT_EQ(knownBc, nodeBc);

  // check query with timestamp

  size_t pivotBlockIndex = knownBc.blocks.size() / 3 * 2;

  auto iter = knownBc.blocks.begin();
  std::advance(iter, pivotBlockIndex);

  ASSERT_TRUE(iter->hasBlock);
  auto timestamp = iter->block.timestamp - 1;
  uint32_t startHeight = 0;
  std::vector<BlockShortEntry> blocks;

  std::cout << "Requesting timestamp: " << timestamp << std::endl;
  
  NodeCallback cb;
  mainNode->queryBlocks({ currency.genesisBlockHash() }, timestamp, blocks, startHeight, cb.callback());
  ASSERT_TRUE(!cb.get());

  EXPECT_EQ(0, startHeight);
  EXPECT_EQ(knownBc.blocks.begin()->blockHash, blocks.begin()->blockHash);
  EXPECT_EQ(knownBc.blocks.size(), blocks.size());

  auto startBlockIter = std::find_if(blocks.begin(), blocks.end(), [](const BlockShortEntry& e) { return e.hasBlock; });
  ASSERT_TRUE(startBlockIter != blocks.end());

  const BlockTemplate& startBlock = startBlockIter->block;

  std::cout << "Starting block timestamp: " << startBlock.timestamp << std::endl;
  auto startFullIndex = std::distance(blocks.begin(), startBlockIter);
  EXPECT_EQ(pivotBlockIndex, startFullIndex);

  auto it = blocks.begin();
  for (const auto& item : knownBc.blocks) {
    ASSERT_EQ(item.blockHash, it->blockHash);
    ++it;
  }
}


TEST_F(NodeTest, observerHeightNotifications) {
  BlockchainInfo extraBlocks;
  loadBlockchainInfo("blocks_extra.js", extraBlocks);

  startNetworkWithBlockchain("testnet_300");

  auto& daemon = network.getNode(0);

  {
    std::unique_ptr<INode> mainNode;
    daemon.makeINode(mainNode);

    NodeObserver observer(*mainNode);

    std::chrono::seconds timeout(10);
    uint32_t knownHeight = 0;
    uint32_t localHeight = 0;
    size_t peerCount = 0;

    EXPECT_TRUE(observer.m_localHeight.waitFor(timeout, localHeight));
    EXPECT_TRUE(observer.m_knownHeight.waitFor(timeout, knownHeight));
    EXPECT_TRUE(observer.m_peerCount.waitFor(timeout, peerCount));

    EXPECT_GT(localHeight, 0U);
    EXPECT_GT(knownHeight, 0U);
    EXPECT_GT(peerCount, 0U);

    std::cout << "Local height = " << localHeight << std::endl;
    std::cout << "Known height = " << knownHeight << std::endl;
    std::cout << "Peer count = " << peerCount << std::endl;

    EXPECT_EQ(localHeight, mainNode->getLastLocalBlockHeight());
    EXPECT_EQ(knownHeight, mainNode->getLastKnownBlockHeight());

    // submit 1 block and check observer

    uint32_t newKnownHeight = 0;
    uint32_t newLocalHeight = 0;

    auto blockData = toBinaryArray(extraBlocks.blocks.begin()->block);
    ASSERT_TRUE(daemon.submitBlock(Common::toHex(blockData.data(), blockData.size())));

    ASSERT_TRUE(observer.m_localHeight.waitFor(timeout, newLocalHeight));
    ASSERT_TRUE(observer.m_knownHeight.waitFor(timeout, newKnownHeight));

    size_t blocksSubmitted = 1;

    EXPECT_EQ(localHeight + blocksSubmitted, newLocalHeight);
    EXPECT_EQ(knownHeight + blocksSubmitted, newKnownHeight);

    EXPECT_EQ(newLocalHeight, mainNode->getLastLocalBlockHeight());
    EXPECT_EQ(newKnownHeight, mainNode->getLastKnownBlockHeight());

  }
}
