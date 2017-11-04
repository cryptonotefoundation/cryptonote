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

#include "P2pConnectionProxy.h"

#include "LevinProtocol.h"
#include "P2pContext.h"
#include "P2pNode.h"

#include <System/InterruptedException.h>

using namespace System;

namespace CryptoNote {

P2pConnectionProxy::P2pConnectionProxy(P2pContextOwner&& ctx, IP2pNodeInternal& node)
  : m_contextOwner(std::move(ctx)), m_context(m_contextOwner.get()), m_node(node) {}

P2pConnectionProxy::~P2pConnectionProxy() {
  m_context.stop();
}

bool P2pConnectionProxy::processIncomingHandshake() {
  LevinProtocol::Command cmd;
  if (!m_context.readCommand(cmd)) {
    throw std::runtime_error("Connection unexpectedly closed");
  }

  if (cmd.command == COMMAND_HANDSHAKE::ID) {
    handleHandshakeRequest(cmd);
    return true;
  } 
  
  if (cmd.command == COMMAND_PING::ID) {
    COMMAND_PING::response resp{ PING_OK_RESPONSE_STATUS_TEXT, m_node.getPeerId() };
    m_context.writeMessage(makeReply(COMMAND_PING::ID, LevinProtocol::encode(resp), LEVIN_PROTOCOL_RETCODE_SUCCESS));
    return false;
  } 

  throw std::runtime_error("Unexpected command: " + std::to_string(cmd.command));
}

void P2pConnectionProxy::read(P2pMessage& message) {
  if (!m_readQueue.empty()) {
    message = std::move(m_readQueue.front());
    m_readQueue.pop();
    return;
  }

  for (;;) {
    LevinProtocol::Command cmd;
    if (!m_context.readCommand(cmd)) {
      throw InterruptedException();
    }

    message.type = cmd.command;

    if (cmd.command == COMMAND_HANDSHAKE::ID) {
      handleHandshakeResponse(cmd, message);
      break;
    } else if (cmd.command == COMMAND_TIMED_SYNC::ID) {
      handleTimedSync(cmd);
    } else {
      message.data = std::move(cmd.buf);
      break;
    }
  }
}

void P2pConnectionProxy::write(const P2pMessage &message) {
  if (message.type == COMMAND_HANDSHAKE::ID) {
    writeHandshake(message);
  } else {
    m_context.writeMessage(P2pContext::Message(P2pMessage(message), P2pContext::Message::NOTIFY));
  }
}

void P2pConnectionProxy::ban() {
  // not implemented
}

void P2pConnectionProxy::stop() {
  m_context.stop();
}

void P2pConnectionProxy::writeHandshake(const P2pMessage &message) {
  CORE_SYNC_DATA coreSync;
  LevinProtocol::decode(message.data, coreSync);

  if (m_context.isIncoming()) {
    // response
    COMMAND_HANDSHAKE::response res;
    res.node_data = m_node.getNodeData();
    res.payload_data = coreSync;
    res.local_peerlist = m_node.getLocalPeerList();
    m_context.writeMessage(makeReply(COMMAND_HANDSHAKE::ID, LevinProtocol::encode(res), LEVIN_PROTOCOL_RETCODE_SUCCESS));
    m_node.tryPing(m_context);
  } else {
    // request
    COMMAND_HANDSHAKE::request req;
    req.node_data = m_node.getNodeData();
    req.payload_data = coreSync;
    m_context.writeMessage(makeRequest(COMMAND_HANDSHAKE::ID, LevinProtocol::encode(req)));
  }
}

void P2pConnectionProxy::handleHandshakeRequest(const LevinProtocol::Command& cmd) {
  COMMAND_HANDSHAKE::request req;
  if (!LevinProtocol::decode<COMMAND_HANDSHAKE::request>(cmd.buf, req)) {
    throw std::runtime_error("Failed to decode COMMAND_HANDSHAKE request");
  }

  m_node.handleNodeData(req.node_data, m_context);
  m_readQueue.push(P2pMessage{ 
    cmd.command, LevinProtocol::encode(req.payload_data) }); // enqueue payload info
}

void P2pConnectionProxy::handleHandshakeResponse(const LevinProtocol::Command& cmd, P2pMessage& message) {
  if (m_context.isIncoming()) {
    // handshake should be already received by P2pNode
    throw std::runtime_error("Unexpected COMMAND_HANDSHAKE from incoming connection");
  }

  COMMAND_HANDSHAKE::response res;
  if (!LevinProtocol::decode(cmd.buf, res)) {
    throw std::runtime_error("Invalid handshake message format");
  }

  m_node.handleNodeData(res.node_data, m_context);
  m_node.handleRemotePeerList(res.local_peerlist, res.node_data.local_time);

  message.data = LevinProtocol::encode(res.payload_data);
}

void P2pConnectionProxy::handleTimedSync(const LevinProtocol::Command& cmd) {
  if (cmd.isResponse) {
    COMMAND_TIMED_SYNC::response res;
    LevinProtocol::decode(cmd.buf, res);
    m_node.handleRemotePeerList(res.local_peerlist, res.local_time);
  } else {
    // we ignore information from the request
    // COMMAND_TIMED_SYNC::request req;
    // LevinProtocol::decode(cmd.buf, req);
    COMMAND_TIMED_SYNC::response res;

    res.local_time = time(nullptr);
    res.local_peerlist = m_node.getLocalPeerList();
    res.payload_data = m_node.getGenesisPayload();

    m_context.writeMessage(makeReply(COMMAND_TIMED_SYNC::ID, LevinProtocol::encode(res), LEVIN_PROTOCOL_RETCODE_SUCCESS));
  }
}

}
