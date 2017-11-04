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

#include "CryptoNote.h"
#include "P2pProtocolTypes.h"

namespace CryptoNote {

  struct CryptoNoteConnectionContext;

  struct IP2pEndpoint {
    virtual void relay_notify_to_all(int command, const BinaryArray& data_buff, const net_connection_id* excludeConnection) = 0;
    virtual bool invoke_notify_to_peer(int command, const BinaryArray& req_buff, const CryptoNote::CryptoNoteConnectionContext& context) = 0;
    virtual uint64_t get_connections_count()=0;
    virtual void for_each_connection(std::function<void(CryptoNote::CryptoNoteConnectionContext&, PeerIdType)> f) = 0;
    // can be called from external threads
    virtual void externalRelayNotifyToAll(int command, const BinaryArray& data_buff) = 0;
  };

  struct p2p_endpoint_stub: public IP2pEndpoint {
    virtual void relay_notify_to_all(int command, const BinaryArray& data_buff, const net_connection_id* excludeConnection) override {}
    virtual bool invoke_notify_to_peer(int command, const BinaryArray& req_buff, const CryptoNote::CryptoNoteConnectionContext& context) override { return true; }
    virtual void for_each_connection(std::function<void(CryptoNote::CryptoNoteConnectionContext&, PeerIdType)> f) override {}
    virtual uint64_t get_connections_count() override { return 0; }   
    virtual void externalRelayNotifyToAll(int command, const BinaryArray& data_buff) override {}
  };
}
