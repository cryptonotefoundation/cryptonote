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

#include <functional>
#include <list>

#include <Logging/LoggerRef.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/TcpListener.h>
#include <System/Timer.h>

#include "IP2pNodeInternal.h"
#include "IStreamSerializable.h"
#include "NetNodeConfig.h"
#include "P2pInterfaces.h"
#include "P2pNodeConfig.h"
#include "P2pProtocolDefinitions.h"
#include "PeerListManager.h"

namespace CryptoNote {

class P2pContext;
class P2pConnectionProxy;

class P2pNode : 
  public IP2pNode, 
  public IStreamSerializable,
  IP2pNodeInternal {

public:

  P2pNode(
    const P2pNodeConfig& cfg,
    System::Dispatcher& dispatcher, 
    Logging::ILogger& log, 
    const Crypto::Hash& genesisHash, 
    PeerIdType peerId);

  ~P2pNode();
  
  // IP2pNode
  virtual std::unique_ptr<IP2pConnection> receiveConnection() override;
  virtual void stop() override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

  // P2pNode
  void start();
  void serialize(ISerializer& s);

private:
  typedef std::unique_ptr<P2pContext> ContextPtr;
  typedef std::list<ContextPtr> ContextList;

  Logging::LoggerRef logger;
  bool m_stopRequested;
  const P2pNodeConfig m_cfg;
  const PeerIdType m_myPeerId;
  const Crypto::Hash m_genesisHash;
  const CORE_SYNC_DATA m_genesisPayload;

  System::Dispatcher& m_dispatcher;
  System::ContextGroup workingContextGroup;
  System::TcpListener m_listener;
  System::Timer m_connectorTimer;
  PeerlistManager m_peerlist;
  ContextList m_contexts;
  System::Event m_queueEvent;
  std::deque<std::unique_ptr<IP2pConnection>> m_connectionQueue;

  // IP2pNodeInternal
  virtual const CORE_SYNC_DATA& getGenesisPayload() const override;
  virtual std::list<PeerlistEntry> getLocalPeerList() const override;
  virtual basic_node_data getNodeData() const override;
  virtual PeerIdType getPeerId() const override;

  virtual void handleNodeData(const basic_node_data& node, P2pContext& ctx) override;
  virtual bool handleRemotePeerList(const std::list<PeerlistEntry>& peerlist, time_t local_time) override;
  virtual void tryPing(P2pContext& ctx) override;

  // spawns
  void acceptLoop();
  void connectorLoop();

  // connection related
  void connectPeers();
  void connectPeerList(const std::vector<NetworkAddress>& peers);
  bool isPeerConnected(const NetworkAddress& address);
  bool isPeerUsed(const PeerlistEntry& peer);
  ContextPtr tryToConnectPeer(const NetworkAddress& address);
  bool fetchPeerList(ContextPtr connection);

  // making and processing connections
  size_t getOutgoingConnectionsCount() const;
  void makeExpectedConnectionsCount(const PeerlistManager::Peerlist& peerlist, size_t connectionsCount);
  bool makeNewConnectionFromPeerlist(const PeerlistManager::Peerlist& peerlist);
  void preprocessIncomingConnection(ContextPtr ctx);
  void enqueueConnection(std::unique_ptr<P2pConnectionProxy> proxy);
  std::unique_ptr<P2pConnectionProxy> createProxy(ContextPtr ctx);
};

}
