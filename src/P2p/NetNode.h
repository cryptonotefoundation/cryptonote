// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero project
// Copyright (c) 2014-2018, The Forknote developers
// Copyright (c) 2016-2020, The Karbo developers
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

#pragma once

#include <functional>
#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>

#include <System/Context.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/Timer.h>
#include <System/TcpConnection.h>
#include <System/TcpListener.h>

#include "CryptoNoteCore/OnceInInterval.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "Common/CommandLine.h"
#include "Logging/LoggerRef.h"

#include "ConnectionContext.h"
#include "LevinProtocol.h"
#include "NetNodeCommon.h"
#include "NetNodeConfig.h"
#include "P2pProtocolDefinitions.h"
#include "P2pNetworks.h"
#include "PeerListManager.h"

namespace System {
class TcpConnection;
}

namespace CryptoNote
{
  class LevinProtocol;
  class ISerializer;

  struct P2pMessage {
    enum Type {
      COMMAND,
      REPLY,
      NOTIFY
    };

    P2pMessage(Type type, uint32_t command, const BinaryArray& buffer, int32_t returnCode = 0) :
      type(type), command(command), buffer(buffer), returnCode(returnCode) {
    }

    P2pMessage(P2pMessage&& msg) :
      type(msg.type), command(msg.command), buffer(std::move(msg.buffer)), returnCode(msg.returnCode) {
    }

    size_t size() {
      return buffer.size();
    }

    Type type;
    uint32_t command;
    const BinaryArray buffer;
    int32_t returnCode;
  };

  struct P2pConnectionContext : public CryptoNoteConnectionContext {
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    System::Context<void>* context;
    PeerIdType peerId;
    System::TcpConnection connection;
    std::set<NetworkAddress> sent_addresses;

    P2pConnectionContext(System::Dispatcher& dispatcher, Logging::ILogger& log, System::TcpConnection&& conn) :
      context(nullptr),
      peerId(0),
      connection(std::move(conn)),
      logger(log, "node_server"),
      queueEvent(dispatcher),
      stopped(false) {
    }

    P2pConnectionContext(P2pConnectionContext&& ctx) : 
      CryptoNoteConnectionContext(std::move(ctx)),
      context(ctx.context),
      peerId(ctx.peerId),
      connection(std::move(ctx.connection)),
      logger(ctx.logger.getLogger(), "node_server"),
      queueEvent(std::move(ctx.queueEvent)),
      stopped(std::move(ctx.stopped)) {
    }

    bool pushMessage(P2pMessage&& msg);
    std::vector<P2pMessage> popBuffer();
    void interrupt();

    uint64_t writeDuration(TimePoint now) const;

  private:
    Logging::LoggerRef logger;
    TimePoint writeOperationStartTime;
    System::Event queueEvent;
    std::vector<P2pMessage> writeQueue;
    size_t writeQueueSize = 0;
    bool stopped;
  };

  class NodeServer :  public IP2pEndpoint
  {
  public:

    NodeServer(System::Dispatcher& dispatcher, CryptoNote::CryptoNoteProtocolHandler& payload_handler, Logging::ILogger& log);

    bool run();
    bool init(const NetNodeConfig& config);
    bool deinit();
    bool sendStopSignal();
    uint32_t get_this_peer_port(){return m_listeningPort;}
    CryptoNote::CryptoNoteProtocolHandler& get_payload_object();

    void serialize(ISerializer& s);

    // debug functions
    bool log_peerlist();
    bool log_connections();
    bool log_banlist();
    virtual uint64_t get_connections_count() override;
    size_t get_outgoing_connections_count();

    CryptoNote::PeerlistManager& getPeerlistManager() { return m_peerlist; }
    bool ban_host(const uint32_t address_ip, time_t seconds = P2P_IP_BLOCKTIME) override;
    bool unban_host(const uint32_t address_ip) override;
    std::map<uint32_t, time_t> get_blocked_hosts() override { return m_blocked_hosts; };

  private:

    enum PeerType { anchor = 0, white, gray };

    int handleCommand(const LevinProtocol::Command& cmd, BinaryArray& buff_out, P2pConnectionContext& context, bool& handled);

    //----------------- commands handlers ----------------------------------------------
    int handle_handshake(int command, COMMAND_HANDSHAKE::request& arg, COMMAND_HANDSHAKE::response& rsp, P2pConnectionContext& context);
    int handle_timed_sync(int command, COMMAND_TIMED_SYNC::request& arg, COMMAND_TIMED_SYNC::response& rsp, P2pConnectionContext& context);
    int handle_ping(int command, COMMAND_PING::request& arg, COMMAND_PING::response& rsp, P2pConnectionContext& context);
#ifdef ALLOW_DEBUG_COMMANDS
    int handle_get_stat_info(int command, COMMAND_REQUEST_STAT_INFO::request& arg, COMMAND_REQUEST_STAT_INFO::response& rsp, P2pConnectionContext& context);
    int handle_get_network_state(int command, COMMAND_REQUEST_NETWORK_STATE::request& arg, COMMAND_REQUEST_NETWORK_STATE::response& rsp, P2pConnectionContext& context);
    int handle_get_peer_id(int command, COMMAND_REQUEST_PEER_ID::request& arg, COMMAND_REQUEST_PEER_ID::response& rsp, P2pConnectionContext& context);
#endif

    bool init_config();
    bool make_default_config();
    bool store_config();
#ifdef ALLOW_DEBUG_COMMANDS
    bool check_trust(const proof_of_trust& tr);
#endif

    bool handshake(CryptoNote::LevinProtocol& proto, P2pConnectionContext& context, bool just_take_peerlist = false);
    bool timedSync();
    bool handleTimedSyncResponse(const BinaryArray& in, P2pConnectionContext& context);
    void forEachConnection(std::function<void(P2pConnectionContext&)> action);

    void on_connection_new(P2pConnectionContext& context);
    void on_connection_close(P2pConnectionContext& context);

    //----------------- i_p2p_endpoint -------------------------------------------------------------
    virtual void relay_notify_to_all(int command, const BinaryArray& data_buff, const net_connection_id* excludeConnection) override;
    virtual bool invoke_notify_to_peer(int command, const BinaryArray& req_buff, const CryptoNoteConnectionContext& context) override;
    virtual void drop_connection(CryptoNoteConnectionContext& context, bool add_fail) override;
    virtual void for_each_connection(std::function<void(CryptoNote::CryptoNoteConnectionContext&, PeerIdType)> f) override;
    virtual void externalRelayNotifyToAll(int command, const BinaryArray& data_buff, const net_connection_id* excludeConnection) override;
    virtual void externalRelayNotifyToList(int command, const BinaryArray &data_buff, const std::list<boost::uuids::uuid> relayList) override;

    //-----------------------------------------------------------------------------------------------
    bool add_host_fail(const uint32_t address_ip);
    bool block_host(const uint32_t address_ip, time_t seconds = P2P_IP_BLOCKTIME);
    bool unblock_host(const uint32_t address_ip);
    bool is_remote_host_allowed(const uint32_t address_ip);
    bool is_addr_recently_failed(const uint32_t address_ip);
    bool handleConfig(const NetNodeConfig& config);
    bool append_net_address(std::vector<NetworkAddress>& nodes, const std::string& addr);
    bool idle_worker();
    bool handle_remote_peerlist(const std::vector<PeerlistEntry>& peerlist, time_t local_time, const CryptoNoteConnectionContext& context);
    bool get_local_node_data(basic_node_data& node_data);

    bool fix_time_delta(std::vector<PeerlistEntry>& local_peerlist, time_t local_time, int64_t& delta);

    bool connections_maker();
    bool make_new_connection_from_peerlist(bool use_white_list);
    bool make_new_connection_from_anchor_peerlist(const std::vector<AnchorPeerlistEntry>& anchor_peerlist);
    bool try_to_connect_and_handshake_with_new_peer(const NetworkAddress& na, bool just_take_peerlist = false, uint64_t last_seen_stamp = 0, PeerType peer_type = white, uint64_t first_seen_stamp = 0);
    bool is_peer_used(const PeerlistEntry& peer);
    bool is_peer_used(const AnchorPeerlistEntry& peer);
    bool is_addr_connected(const NetworkAddress& peer);  
    bool try_ping(basic_node_data& node_data, P2pConnectionContext& context);
    bool make_expected_connections_count(PeerType peer_type, size_t expected_connections);
    bool is_priority_node(const NetworkAddress& na);

    bool connect_to_peerlist(const std::vector<NetworkAddress>& peers);

    bool parse_peers_and_add_to_container(const boost::program_options::variables_map& vm, 
      const command_line::arg_descriptor<std::vector<std::string> > & arg, std::vector<NetworkAddress>& container);

    bool gray_peerlist_housekeeping();

    //debug functions
    std::string print_connections_container();

    typedef std::unordered_map<boost::uuids::uuid, P2pConnectionContext, boost::hash<boost::uuids::uuid>> ConnectionContainer;
    typedef ConnectionContainer::iterator ConnectionIterator;
    ConnectionContainer m_connections;

    void acceptLoop();
    void connectionHandler(const boost::uuids::uuid& connectionId, P2pConnectionContext& connection);
    void writeHandler(P2pConnectionContext& ctx);
    void onIdle();
    void timedSyncLoop();
    void timeoutLoop();

    struct config
    {
      network_config m_net_config;
      uint64_t m_peer_id;

      void serialize(ISerializer& s) {
        KV_MEMBER(m_net_config)
        KV_MEMBER(m_peer_id)
      }
    };

    config m_config;
    std::string m_config_folder;

    bool m_have_address;
    bool m_first_connection_maker_call;
    uint32_t m_listeningPort;
    uint32_t m_external_port;
    uint32_t m_ip_address;
    bool m_allow_local_ip;
    bool m_hide_my_port;
    std::string m_p2p_state_filename;

    System::Dispatcher& m_dispatcher;
    System::ContextGroup m_workingContextGroup;
    System::Event m_stopEvent;
    System::Timer m_idleTimer;
    System::Timer m_timeoutTimer;
    System::TcpListener m_listener;
    Logging::LoggerRef logger;
    std::atomic<bool> m_stop;

    CryptoNoteProtocolHandler& m_payload_handler;
    PeerlistManager m_peerlist;

    OnceInInterval m_peer_handshake_idle_maker_interval;
    OnceInInterval m_connections_maker_interval;
    OnceInInterval m_peerlist_store_interval;
    OnceInInterval m_gray_peerlist_housekeeping_interval;
    System::Timer m_timedSyncTimer;

    std::string m_bind_ip;
    std::string m_port;
#ifdef ALLOW_DEBUG_COMMANDS
    uint64_t m_last_stat_request_time;
#endif
    std::vector<NetworkAddress> m_priority_peers;
    std::vector<NetworkAddress> m_exclusive_peers;
    std::vector<NetworkAddress> m_seed_nodes;
    std::vector<PeerlistEntry> m_command_line_peers;
    uint64_t m_peer_livetime;
    boost::uuids::uuid m_network_id;
    std::map<uint32_t, time_t> m_blocked_hosts;
    std::map<uint32_t, uint64_t> m_host_fails_score;

    mutable std::mutex mutex;
  };
}
