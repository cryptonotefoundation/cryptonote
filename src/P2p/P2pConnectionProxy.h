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

#include <queue>

#include "IP2pNodeInternal.h"
#include "LevinProtocol.h"
#include "P2pContextOwner.h"
#include "P2pInterfaces.h"

namespace CryptoNote {

class P2pContext;
class P2pNode;

class P2pConnectionProxy : public IP2pConnection {
public:

  P2pConnectionProxy(P2pContextOwner&& ctx, IP2pNodeInternal& node);
  ~P2pConnectionProxy();

  bool processIncomingHandshake();

  // IP2pConnection
  virtual void read(P2pMessage& message) override;
  virtual void write(const P2pMessage &message) override;
  virtual void ban() override;
  virtual void stop() override;

private:

  void writeHandshake(const P2pMessage &message);
  void handleHandshakeRequest(const LevinProtocol::Command& cmd);
  void handleHandshakeResponse(const LevinProtocol::Command& cmd, P2pMessage& message);
  void handleTimedSync(const LevinProtocol::Command& cmd);

  std::queue<P2pMessage> m_readQueue;
  P2pContextOwner m_contextOwner;
  P2pContext& m_context;
  IP2pNodeInternal& m_node;
};

}
