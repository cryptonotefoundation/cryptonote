// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include "InProcTestNode.h"

#include <future>

#include <Common/StringTools.h>
#include <Logging/ConsoleLogger.h>

#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/CoreConfig.h"
#include "CryptoNoteCore/Miner.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include "InProcessNode/InProcessNode.h"

using namespace CryptoNote;

#undef ERROR

namespace Tests {

namespace {
bool parse_peer_from_string(NetworkAddress &pe, const std::string &node_addr) {
  return ::Common::parseIpAddressAndPort(pe.ip, pe.port, node_addr);
}
}


InProcTestNode::InProcTestNode(const TestNodeConfiguration& cfg, const CryptoNote::Currency& currency) : 
  m_cfg(cfg), m_currency(currency) {

  std::promise<std::string> initPromise;
  std::future<std::string> initFuture = initPromise.get_future();

  m_thread = std::thread(std::bind(&InProcTestNode::workerThread, this, std::ref(initPromise)));
  auto initError = initFuture.get();

  if (!initError.empty()) {
    m_thread.join();
    throw std::runtime_error(initError);
  }
}

InProcTestNode::~InProcTestNode() {
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void InProcTestNode::workerThread(std::promise<std::string>& initPromise) {

  System::Dispatcher dispatcher;

  Logging::ConsoleLogger log;

  Logging::LoggerRef logger(log, "InProcTestNode");

  try {

    core.reset(new CryptoNote::core(m_currency, NULL, log, false));
    protocol.reset(new CryptoNote::CryptoNoteProtocolHandler(m_currency, dispatcher, *core, NULL, log));
    p2pNode.reset(new CryptoNote::NodeServer(dispatcher, *protocol, log));
    protocol->set_p2p_endpoint(p2pNode.get());
    core->set_cryptonote_protocol(protocol.get());

    CryptoNote::NetNodeConfig p2pConfig;

    p2pConfig.setBindIp("127.0.0.1");
    p2pConfig.setBindPort(m_cfg.p2pPort);
    p2pConfig.setExternalPort(0);
    p2pConfig.setAllowLocalIp(false);
    p2pConfig.setHideMyPort(false);
    p2pConfig.setConfigFolder(m_cfg.dataDir);

    std::vector<NetworkAddress> exclusiveNodes;
    for (const auto& en : m_cfg.exclusiveNodes) {
      NetworkAddress na;
      parse_peer_from_string(na, en);
      exclusiveNodes.push_back(na);
    }

    p2pConfig.setExclusiveNodes(exclusiveNodes);

    if (!p2pNode->init(p2pConfig)) {
      throw std::runtime_error("Failed to init p2pNode");
    }

    CryptoNote::MinerConfig emptyMiner;
    CryptoNote::CoreConfig coreConfig;

    coreConfig.configFolder = m_cfg.dataDir;
    
    if (!core->init(coreConfig, emptyMiner, true)) {
      throw std::runtime_error("Core failed to initialize");
    }

    initPromise.set_value(std::string());

  } catch (std::exception& e) {
    logger(Logging::ERROR) << "Failed to initialize: " << e.what();
    initPromise.set_value(e.what());
    return;
  }

  try {
    p2pNode->run();
  } catch (std::exception& e) {
    logger(Logging::ERROR) << "exception in p2p::run: " << e.what();
  }

  core->deinit();
  p2pNode->deinit();
  core->set_cryptonote_protocol(NULL);
  protocol->set_p2p_endpoint(NULL);

  p2pNode.reset();
  protocol.reset();
  core.reset();
}

bool InProcTestNode::startMining(size_t threadsCount, const std::string &address) {
  assert(core.get());
  AccountPublicAddress addr;
  m_currency.parseAccountAddressString(address, addr);
  return core->get_miner().start(addr, threadsCount);
}

bool InProcTestNode::stopMining() {
  assert(core.get());
  return core->get_miner().stop();
}

bool InProcTestNode::stopDaemon() {
  if (!p2pNode.get()) {
    return false;
  }

  p2pNode->sendStopSignal();
  m_thread.join();
  return true;
}

bool InProcTestNode::getBlockTemplate(const std::string &minerAddress, CryptoNote::Block &blockTemplate, uint64_t &difficulty) {
  AccountPublicAddress addr;
  m_currency.parseAccountAddressString(minerAddress, addr);
  uint32_t height = 0;
  return core->get_block_template(blockTemplate, addr, difficulty, height, BinaryArray());
}

bool InProcTestNode::submitBlock(const std::string& block) {
  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  core->handle_incoming_block_blob(Common::fromHex(block), bvc, true, true);
  return bvc.m_added_to_main_chain;
}

bool InProcTestNode::getTailBlockId(Crypto::Hash &tailBlockId) {
  tailBlockId = core->get_tail_id();
  return true;
}

bool InProcTestNode::makeINode(std::unique_ptr<CryptoNote::INode> &node) {

  std::unique_ptr<InProcessNode> inprocNode(new CryptoNote::InProcessNode(*core, *protocol));

  std::promise<std::error_code> p;
  auto future = p.get_future();

  inprocNode->init([&p](std::error_code ec) {
    std::promise<std::error_code> localPromise(std::move(p));
    localPromise.set_value(ec);
  });

  auto ec = future.get();

  if (!ec) {
    node = std::move(inprocNode);
    return true;
  }

  return false;
}

uint64_t InProcTestNode::getLocalHeight() {
  return core->get_current_blockchain_height();
}

}
