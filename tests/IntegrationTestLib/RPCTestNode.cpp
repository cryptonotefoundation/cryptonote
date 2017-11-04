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

#include "RPCTestNode.h"

#include <future>
#include <vector>
#include <thread>

#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "NodeRpcProxy/NodeRpcProxy.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"
#include "Rpc/JsonRpc.h"

#include "Logger.h"
#include "NodeCallback.h"

using namespace CryptoNote;
using namespace System;

namespace Tests {

RPCTestNode::RPCTestNode(uint16_t port, System::Dispatcher& d) : 
  m_rpcPort(port), m_dispatcher(d), m_httpClient(d, "127.0.0.1", port) {
}

bool RPCTestNode::startMining(size_t threadsCount, const std::string& address) { 
  LOG_DEBUG("startMining()");

  try {
    COMMAND_RPC_START_MINING::request req;
    COMMAND_RPC_START_MINING::response resp;
    req.miner_address = address;
    req.threads_count = threadsCount;

    invokeJsonCommand(m_httpClient, "/start_mining", req, resp);
    if (resp.status != CORE_RPC_STATUS_OK) {
      throw std::runtime_error(resp.status);
    }
  } catch (std::exception& e) {
    std::cout << "startMining() RPC call fail: " << e.what();
    return false;
  }

  return true;
}

bool RPCTestNode::getBlockTemplate(const std::string& minerAddress, CryptoNote::BlockTemplate& blockTemplate, uint64_t& difficulty) {
  LOG_DEBUG("getBlockTemplate()");

  try {
    COMMAND_RPC_GETBLOCKTEMPLATE::request req;
    COMMAND_RPC_GETBLOCKTEMPLATE::response rsp;
    req.wallet_address = minerAddress;
    req.reserve_size = 0;

    JsonRpc::invokeJsonRpcCommand(m_httpClient, "getblocktemplate", req, rsp);
    if (rsp.status != CORE_RPC_STATUS_OK) {
      throw std::runtime_error(rsp.status);
    }

    difficulty = rsp.difficulty;

    BinaryArray blockBlob = (::Common::fromHex(rsp.blocktemplate_blob));
    return fromBinaryArray(blockTemplate, blockBlob);
  } catch (std::exception& e) {
    LOG_ERROR("JSON-RPC call startMining() failed: " + std::string(e.what()));
    return false;
  }

  return true;
}

bool RPCTestNode::submitBlock(const std::string& block) {
  LOG_DEBUG("submitBlock()");

  try {
    COMMAND_RPC_SUBMITBLOCK::request req;
    COMMAND_RPC_SUBMITBLOCK::response res;
    req.push_back(block);
    JsonRpc::invokeJsonRpcCommand(m_httpClient, "submitblock", req, res);
    if (res.status != CORE_RPC_STATUS_OK) {
      throw std::runtime_error(res.status);
    }
  } catch (std::exception& e) {
    LOG_ERROR("RPC call of submit_block returned error: " + std::string(e.what()));
    return false;
  }

  return true;
}

bool RPCTestNode::stopMining() { 
  LOG_DEBUG("stopMining()");

  try {
    COMMAND_RPC_STOP_MINING::request req;
    COMMAND_RPC_STOP_MINING::response resp;
    invokeJsonCommand(m_httpClient, "/stop_mining", req, resp);
    if (resp.status != CORE_RPC_STATUS_OK) {
      throw std::runtime_error(resp.status);
    }
  } catch (std::exception& e) {
    std::cout << "stopMining() RPC call fail: " << e.what();
    return false;
  }

  return true;
}

bool RPCTestNode::getTailBlockId(Crypto::Hash& tailBlockId) {
  LOG_DEBUG("getTailBlockId()");

  try {
    COMMAND_RPC_GET_LAST_BLOCK_HEADER::request req;
    COMMAND_RPC_GET_LAST_BLOCK_HEADER::response rsp;
    JsonRpc::invokeJsonRpcCommand(m_httpClient, "getlastblockheader", req, rsp);
    if (rsp.status != CORE_RPC_STATUS_OK) {
      throw std::runtime_error(rsp.status);
    }

    return ::Common::podFromHex(rsp.block_header.hash, tailBlockId);
  } catch (std::exception& e) {
    LOG_ERROR("JSON-RPC call getTailBlockId() failed: " + std::string(e.what()));
    return false;
  }

  return true;
}

bool RPCTestNode::makeINode(std::unique_ptr<CryptoNote::INode>& node) {
  std::unique_ptr<CryptoNote::INode> newNode(new CryptoNote::NodeRpcProxy("127.0.0.1", m_rpcPort));
  NodeCallback cb;
  newNode->init(cb.callback());
  auto ec = cb.get();

  if (ec) {
    LOG_ERROR("init error: " + ec.message() + ':' + TO_STRING(ec.value()));
    return false;
  }

  LOG_DEBUG("NodeRPCProxy on port " + TO_STRING(m_rpcPort) + " initialized");
  node = std::move(newNode);
  return true;
}

bool RPCTestNode::stopDaemon() {
  try {
    LOG_DEBUG("stopDaemon()");
    COMMAND_RPC_STOP_DAEMON::request req;
    COMMAND_RPC_STOP_DAEMON::response resp;
    invokeJsonCommand(m_httpClient, "/stop_daemon", req, resp);
    if (resp.status != CORE_RPC_STATUS_OK) {
      throw std::runtime_error(resp.status);
    }
  } catch (std::exception& e) {
    std::cout << "stopDaemon() RPC call fail: " << e.what();
    return false;
  }

  return true;
}

uint64_t RPCTestNode::getLocalHeight() {
  try {
    CryptoNote::COMMAND_RPC_GET_INFO::request req;
    CryptoNote::COMMAND_RPC_GET_INFO::response rsp;
    invokeJsonCommand(m_httpClient, "/getinfo", req, rsp);
    if (rsp.status == CORE_RPC_STATUS_OK) {
      return rsp.height;
    }
  } catch (std::exception&) {
  }

  return 0;
}

}
