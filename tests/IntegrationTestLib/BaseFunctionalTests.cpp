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

#include "BaseFunctionalTests.h"

#include <vector>
#include <thread>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

#include <boost/filesystem.hpp>

#include <System/ContextGroup.h>
#include <System/Event.h>
#include <System/Timer.h>
#include <System/InterruptedException.h>

#include "P2p/NetNodeConfig.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "WalletLegacy/WalletLegacy.h"

#include "Logger.h"

#include "InProcTestNode.h"
#include "RPCTestNode.h"

#if defined __linux__
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#endif

#ifdef _WIN32
const std::string DAEMON_FILENAME = "bytecoind.exe";
#else
const std::string DAEMON_FILENAME = "bytecoind";
#endif

using namespace Tests::Common;
using namespace Tests;

void BaseFunctionalTests::launchTestnet(size_t count, Topology t) {
  if (count < 1) {
    LOG_WARNING("Testnet has no nodes");
  }

  m_testnetSize = count;
  m_topology = t;

  nodeDaemons.resize(m_testnetSize);

  for (size_t i = 0; i < m_testnetSize; ++i) {
    startNode(i);
  }

  waitDaemonsReady();

  nodeDaemons[0]->makeINode(mainNode);
  makeWallet(workingWallet, mainNode);
}

void BaseFunctionalTests::launchInprocTestnet(size_t count, Topology t) {
  m_testnetSize = count;
  m_topology = t;

  for (size_t i = 0; i < m_testnetSize; ++i) {
    auto cfg = createNodeConfiguration(i);
    nodeDaemons.emplace_back(new InProcTestNode(cfg, m_currency, m_dispatcher));
  }

  waitDaemonsReady();

  nodeDaemons[0]->makeINode(mainNode);
  makeWallet(workingWallet, mainNode);
}

void BaseFunctionalTests::launchTestnetWithInprocNode(size_t count, Topology t) {
  if (count < 1) {
    LOG_WARNING("Testnet has no nodes");
  }

  m_testnetSize = count;
  m_topology = t;

  nodeDaemons.resize(m_testnetSize);

  for (size_t i = 0; i < m_testnetSize - 1; ++i) {
    startNode(i);
  }

  auto cfg = createNodeConfiguration(m_testnetSize - 1);
  nodeDaemons[m_testnetSize - 1].reset(new InProcTestNode(cfg, m_currency, m_dispatcher));

  waitDaemonsReady();

  nodeDaemons[0]->makeINode(mainNode);
  makeWallet(workingWallet, mainNode);
}

Tests::TestNodeConfiguration BaseFunctionalTests::createNodeConfiguration(size_t index) {
  Tests::TestNodeConfiguration cfg;

  std::string dataDirPath = m_dataDir + "/node" + std::to_string(index);
  boost::filesystem::create_directory(dataDirPath);

  cfg.dataDir = dataDirPath;

  uint16_t rpcPort = static_cast<uint16_t>(RPC_FIRST_PORT + index);
  uint16_t p2pPort = static_cast<uint16_t>(P2P_FIRST_PORT + index);

  cfg.p2pPort = p2pPort;
  cfg.rpcPort = rpcPort;

  switch (m_topology) {
  case Line:
    if (index != 0) {
      cfg.exclusiveNodes.push_back("127.0.0.1:" + std::to_string(p2pPort - 1));
    }
    break;

  case Ring: {
    uint16_t p2pExternalPort = static_cast<uint16_t>(P2P_FIRST_PORT + (index + 1) % m_testnetSize);
    cfg.exclusiveNodes.push_back("127.0.0.1:" + std::to_string(p2pExternalPort + 1));
    break;
  }

  case Star:
    if (index == 0) {
      for (size_t node = 1; node < m_testnetSize; ++node) {
        cfg.exclusiveNodes.push_back("127.0.0.1:" + std::to_string(P2P_FIRST_PORT + node));
      }
    }

    break;
  }

  return cfg;
}

void BaseFunctionalTests::startNode(size_t index) {
  std::string dataDirPath = m_dataDir + "/node" + std::to_string(index);
  boost::filesystem::create_directory(dataDirPath);

  std::ofstream config(dataDirPath + "/daemon.conf", std::ios_base::trunc | std::ios_base::out);

  uint16_t rpcPort = static_cast<uint16_t>(RPC_FIRST_PORT + index);
  uint16_t p2pPort = static_cast<uint16_t>(P2P_FIRST_PORT + index);

  config
    << "rpc-bind-port=" << rpcPort << std::endl
    << "p2p-bind-port=" << p2pPort << std::endl
    << "log-level=4" << std::endl
    << "log-file=test_bytecoind_" << index << ".log" << std::endl;

  switch (m_topology) {
  case Line:
    if (index != 0) {
      config << "add-exclusive-node=127.0.0.1:" << p2pPort - 1 << std::endl;
    }

    break;

  case Ring: {
    uint16_t p2pExternalPort = static_cast<uint16_t>(P2P_FIRST_PORT + (index + 1) % m_testnetSize);
    config << "add-exclusive-node=127.0.0.1:" << (p2pExternalPort + 1) << std::endl;
    break;
  }
  case Star:
    if (index == 0) {
      for (size_t node = 1; node < m_testnetSize; ++node) {
        config << "add-exclusive-node=127.0.0.1:" << (P2P_FIRST_PORT + node) << std::endl;
      }
    }

    break;
  }

  config.close();

  boost::filesystem::path daemonPath = index < m_config.daemons.size() ?
    boost::filesystem::path(m_config.daemons[index]) : (boost::filesystem::path(m_daemonDir) / DAEMON_FILENAME);
  boost::system::error_code ignoredEc;
  if (!boost::filesystem::exists(daemonPath, ignoredEc)) {
    throw std::runtime_error("daemon binary wasn't found");
  }

#if defined WIN32
  std::string commandLine = "start /MIN \"bytecoind" + std::to_string(index) + "\" \"" + daemonPath.string() +
    "\" --testnet --data-dir=\"" + dataDirPath + "\" --config-file=daemon.conf";
  LOG_DEBUG(commandLine);
  system(commandLine.c_str());
#elif defined __linux__
  auto pid = fork();
  if (pid == 0) {
    std::string pathToDaemon = daemonPath.string();
    close(1);
    close(2);
    std::string dataDir = "--data-dir=" + dataDirPath + "";
    LOG_TRACE(pathToDaemon);
    if (execl(pathToDaemon.c_str(), "bytecoind", "--testnet", dataDir.c_str(), "--config-file=daemon.conf", NULL) == -1) {
      LOG_ERROR(TO_STRING(errno));
    }
    abort();
//    throw std::runtime_error("failed to start daemon");
  } else if (pid > 0) {
    pids.resize(m_testnetSize, 0);
    assert(pids[index] == 0);
    pids[index] = pid;
  }
#else
  assert(false);
#endif

  assert(nodeDaemons.size() > index);
  nodeDaemons[index] = std::unique_ptr<TestNode>(new RPCTestNode(rpcPort, m_dispatcher));
}

void BaseFunctionalTests::stopNode(size_t index) {
  assert(nodeDaemons[index].get() != nullptr);
  bool ok = nodeDaemons[index]->stopDaemon();
  assert(ok);
  std::this_thread::sleep_for(std::chrono::milliseconds(5000));

  nodeDaemons[index].release();

#ifdef __linux__
  int status;
  assert(pids[index] != 0);
  while (-1 == waitpid(pids[index], &status, 0));
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::cerr << "Process " << " (pid " << pids[index] << ") failed" << std::endl;
    exit(1);
  }
  pids[index] = 0;
#endif
}

bool BaseFunctionalTests::waitDaemonsReady() {
  for (size_t i = 0; i < nodeDaemons.size(); ++i) {
    bool ok = waitDaemonReady(i);
    if (!ok) {
      return false;
    }
  }

  return true;
}

bool BaseFunctionalTests::waitDaemonReady(size_t nodeIndex) {
  assert(nodeIndex < nodeDaemons.size() && nodeDaemons[nodeIndex].get() != nullptr);

  for (size_t i = 0; ; ++i) {
    if (nodeDaemons[nodeIndex]->getLocalHeight() > 0) {
      break;
    } else if (i < 2 * 60) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } else {
      return false;
    }
  }

  return true;
}

BaseFunctionalTests::~BaseFunctionalTests() {
  if (mainNode) {
    mainNode->shutdown();
  }

  stopTestnet();

  for (size_t i = 0; i < m_testnetSize; ++i) {
    boost::system::error_code ignoredErrorCode;
    auto nodeDataDir = boost::filesystem::path(m_dataDir) / boost::filesystem::path("node" + std::to_string(i));
    boost::filesystem::remove_all(nodeDataDir, ignoredErrorCode);
  }
}

namespace {
  class WaitForCoinBaseObserver : public CryptoNote::IWalletLegacyObserver {
    Semaphore& m_gotReward;
    CryptoNote::IWalletLegacy& m_wallet;
  public:
    WaitForCoinBaseObserver(Semaphore& gotReward, CryptoNote::IWalletLegacy& wallet) : m_gotReward(gotReward), m_wallet(wallet) { }
    virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId) override {
      CryptoNote::WalletLegacyTransaction trInfo;
      m_wallet.getTransaction(transactionId, trInfo);
      if (trInfo.isCoinbase) m_gotReward.notify();
    }
  };
}

bool BaseFunctionalTests::mineBlocks(TestNode& node, const CryptoNote::AccountPublicAddress& address, size_t blockCount) {
  for (size_t i = 0; i < blockCount; ++i) {
    BlockTemplate blockTemplate;
    uint64_t difficulty;

    if (!node.getBlockTemplate(m_currency.accountAddressAsString(address), blockTemplate, difficulty)) {
      return false;
    }

    if (difficulty != 1) {
      return false;
    }

    if (!prepareAndSubmitBlock(node, std::move(blockTemplate))) {
      return false;
    }
  }

  return true;
}

bool BaseFunctionalTests::prepareAndSubmitBlock(TestNode& node, CryptoNote::BlockTemplate&& blockTemplate) {
  blockTemplate.timestamp = m_nextTimestamp;
  m_nextTimestamp += 2 * m_currency.difficultyTarget();

  if (blockTemplate.majorVersion >= BLOCK_MAJOR_VERSION_2) {
    blockTemplate.parentBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
    blockTemplate.parentBlock.minorVersion = BLOCK_MINOR_VERSION_0;
    blockTemplate.parentBlock.transactionCount = 1;

    
    CryptoNote::TransactionExtraMergeMiningTag mmTag;
    mmTag.depth = 0;
    //FIXME
    /* if (!CryptoNote::get_aux_block_header_hash(blockTemplate, mmTag.merkleRoot)) { */
    /*   return false; */
    /* } */

    blockTemplate.parentBlock.baseTransaction.extra.clear();
    if (!CryptoNote::appendMergeMiningTagToExtra(blockTemplate.parentBlock.baseTransaction.extra, mmTag)) {
      return false;
    }
  }

  BinaryArray blockBlob = CryptoNote::toBinaryArray(blockTemplate);
  return node.submitBlock(::Common::toHex(blockBlob.data(), blockBlob.size()));
}

bool BaseFunctionalTests::mineBlock(std::unique_ptr<CryptoNote::IWalletLegacy> &wallet) {
  if (nodeDaemons.empty() || !wallet)
    return false;
  if (!nodeDaemons.front()->stopMining())
    return false;
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  Semaphore gotReward;
  WaitForCoinBaseObserver cbo(gotReward, *wallet.get());
  wallet->addObserver(&cbo);
  if (!nodeDaemons.front()->startMining(1, wallet->getAddress()))
    return false;
  gotReward.wait();
  if (!nodeDaemons.front()->stopMining())
    return false;
  wallet->removeObserver(&cbo);
  return true;
}

bool BaseFunctionalTests::mineBlock() {
  return mineBlock(workingWallet);
}

bool BaseFunctionalTests::startMining(size_t threads) {
  if (nodeDaemons.empty() || !workingWallet) return false;
  if(!stopMining()) return false;
  return nodeDaemons.front()->startMining(threads, workingWallet->getAddress());
}

bool BaseFunctionalTests::stopMining() {
  if (nodeDaemons.empty()) return false;
  return nodeDaemons.front()->stopMining();
}

bool BaseFunctionalTests::makeWallet(std::unique_ptr<CryptoNote::IWalletLegacy> & wallet, std::unique_ptr<CryptoNote::INode>& node, const std::string& password) {
  if (!node) return false;
  wallet = std::unique_ptr<CryptoNote::IWalletLegacy>(new CryptoNote::WalletLegacy(m_currency, *node));
  wallet->initAndGenerate(password);
  return true;
}

void BaseFunctionalTests::stopTestnet() {
  if (nodeDaemons.empty()) {
    return;
  }

  // WORKAROUND: Make sure all contexts, that use daemons, are finished before these daemons will be destroyed
  // TODO: There is should be used context groups
  m_dispatcher.yield();

  for (auto& daemon : nodeDaemons) {
    if (daemon) {
      daemon->stopDaemon();
    }
  }
  
  // std::this_thread::sleep_for(std::chrono::milliseconds(5000));

  nodeDaemons.clear();

#ifdef __linux__
  for (auto& pid : pids) {
    if (pid != 0) {
      int status;
      while (-1 == waitpid(pid, &status, 0));
      if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cerr << "Process " << " (pid " << pid << ") failed" << std::endl;
        exit(1);
      }
    }
  }

  pids.clear();
#endif
}

namespace {
  struct PeerCountWaiter : CryptoNote::INodeObserver {
    System::Dispatcher& m_dispatcher;
    System::Event m_event;
    System::Timer m_timer;
    bool m_timedout = false;
    bool m_waiting = false;
    size_t m_expectedPeerCount;

    PeerCountWaiter(System::Dispatcher& dispatcher) : m_dispatcher(dispatcher), m_event(m_dispatcher), m_timer(m_dispatcher) {
    }

    void wait(size_t expectedPeerCount) {
      m_waiting = true;
      m_expectedPeerCount = expectedPeerCount;
      System::ContextGroup cg(m_dispatcher);

      cg.spawn([&] {
        try {
          System::Timer(m_dispatcher).sleep(std::chrono::minutes(2));
          m_timedout = true;
          m_event.set();
        } catch (System::InterruptedException&) {
        }
      });
        
      cg.spawn([&] {
        m_event.wait(); 
        cg.interrupt();
      });

      cg.wait();
      m_waiting = false;
    }

    virtual void peerCountUpdated(size_t count) override {
      m_dispatcher.remoteSpawn([this, count]() {
        if (m_waiting && count == m_expectedPeerCount) {
          m_event.set();
        }
      });
    }
  };
}

bool BaseFunctionalTests::waitForPeerCount(CryptoNote::INode& node, size_t expectedPeerCount) {
  PeerCountWaiter peerCountWaiter(m_dispatcher);
  node.addObserver(&peerCountWaiter);
  if (node.getPeerCount() != expectedPeerCount) {
    peerCountWaiter.wait(expectedPeerCount);
  }
  node.removeObserver(&peerCountWaiter);
  // TODO workaround: make sure ObserverManager doesn't have local pointers to peerCountWaiter, so it can be destroyed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // Run all spawned handlers from PeerCountWaiter::peerCountUpdated
  m_dispatcher.yield();

  return !peerCountWaiter.m_timedout;
}

namespace {
  struct PoolUpdateWaiter : public INodeObserver {
    System::Dispatcher& m_dispatcher;
    System::Event& m_event;

    PoolUpdateWaiter(System::Dispatcher& dispatcher, System::Event& event) : m_dispatcher(dispatcher), m_event(event) {
    }

    virtual void poolChanged() override {
      m_dispatcher.remoteSpawn([this]() { m_event.set(); });
    }
  };
}

bool BaseFunctionalTests::waitForPoolSize(size_t nodeIndex, CryptoNote::INode& node, size_t expectedPoolSize,
  std::vector<std::unique_ptr<CryptoNote::ITransactionReader>>& txPool) {
  System::Event event(m_dispatcher);
  PoolUpdateWaiter poolUpdateWaiter(m_dispatcher, event);
  node.addObserver(&poolUpdateWaiter);

  bool ok;
  for (size_t i = 0; ; ++i) {
    ok = getNodeTransactionPool(nodeIndex, node, txPool);
    if (!ok) {
      break;
    }
    if (txPool.size() == expectedPoolSize) {
      break;
    }

    // TODO NodeRpcProxy doesn't send poolChanged() notification!!!
    //event.wait();
    //event.clear();
    // WORKAROUND
    if (i < 3 * P2P_DEFAULT_HANDSHAKE_INTERVAL) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } else {
      ok = false;
      break;
    }
  }

  node.removeObserver(&poolUpdateWaiter);
  // TODO workaround: make sure ObserverManager doesn't have local pointers to poolUpdateWaiter, so it can be destroyed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // Run all spawned handlers from PoolUpdateWaiter::poolChanged
  m_dispatcher.yield();

  return ok;
}

bool BaseFunctionalTests::getNodeTransactionPool(size_t nodeIndex, CryptoNote::INode& node,
  std::vector<std::unique_ptr<CryptoNote::ITransactionReader>>& txPool) {

  assert(nodeIndex < nodeDaemons.size() && nodeDaemons[nodeIndex].get() != nullptr);
  auto& daemon = *nodeDaemons[nodeIndex];

  Crypto::Hash tailBlockId;
  bool updateTailBlockId = true;
  while (true) {
    if (updateTailBlockId) {
      if (!daemon.getTailBlockId(tailBlockId)) {
        return false;
      }
      updateTailBlockId = false;
    }

    System::Event poolReceivedEvent(m_dispatcher);
    std::error_code ec;
    bool isTailBlockActual;
    std::vector<std::unique_ptr<ITransactionReader>> addedTxs;
    std::vector<Crypto::Hash> deletedTxsIds;
    node.getPoolSymmetricDifference(std::vector<Crypto::Hash>(), tailBlockId, isTailBlockActual, addedTxs, deletedTxsIds,
      [this, &poolReceivedEvent, &ec](std::error_code result) {
        ec = result;
        m_dispatcher.remoteSpawn([&poolReceivedEvent]() { poolReceivedEvent.set(); });
      }
    );
    poolReceivedEvent.wait();

    if (ec) {
      return false;
    } else if (!isTailBlockActual) {
      updateTailBlockId = true;
    } else {
      txPool = std::move(addedTxs);
      break;
    }
  }

  return true;
}
