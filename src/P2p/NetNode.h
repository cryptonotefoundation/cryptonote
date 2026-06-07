// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero project
// Copyright (c) 2014-2018, The Forknote developers
// Copyright (c) 2018-2022, Conceal Network & Conceal Devs
// Copyright (c) 2016-2026, The Karbo developers
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

#include "System/Context.h"
#include "System/ContextGroup.h"
#include "System/Dispatcher.h"
#include "System/Event.h"
#include "System/Timer.h"
#include "System/TcpConnection.h"
#include "System/TcpListener.h"
#include "System/TcpConnector.h"
#include "System/ContextGroupTimeout.h"
#include "System/EventLock.h"
#include "System/InterruptedException.h"
#include "System/Ipv4Address.h"
#include "System/Ipv4Resolver.h"

#include "CryptoNoteCore/OnceInInterval.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "Common/CommandLine.h"
#include "Logging/LoggerRef.h"

#include "ConnectionContext.h"
#include "P2pConnectionContext.h"
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


  class NodeServer :  public IP2pEndpoint
  {
  public:

    NodeServer(System::Dispatcher& dispatcher, CryptoNote::CryptoNoteProtocolHandler& payload_handler, Logging::ILogger& log);

    bool run();
    bool init(const NetNodeConfig& config);
    bool deinit();
    bool sendStopSignal();
    uint32_t get_this_peer_port() const { return m_listeningPort; }
    CryptoNote::CryptoNoteProtocolHandler& get_payload_object();

    void serialize(ISerializer& s);

    // debug functions
    bool log_peerlist() const;
    bool log_connections() const;
    bool log_banlist() const;
    virtual uint64_t get_connections_count() override;
    size_t get_outgoing_connections_count() const;

    CryptoNote::PeerlistManager& getPeerlistManager() { return m_peerlist; }
    bool ban_host(const uint32_t address_ip, time_t seconds = P2P_IP_BLOCKTIME) override;
    bool unban_host(const uint32_t address_ip) override;
    std::map<uint32_t, time_t> get_blocked_hosts() override { return m_blocked_hosts; };

  private:

    enum PeerType { anchor = 0, white, gray };

    int handleCommand(const LevinProtocol::Command& cmd, BinaryArray& buff_out, P2pConnectionContext& context, bool& handled);

    //----------------- commands handlers ----------------------------------------------
    int handle_handshake(int command, const COMMAND_HANDSHAKE::request& arg, COMMAND_HANDSHAKE::response& rsp, P2pConnectionContext& context);
    int handle_timed_sync(int command, const COMMAND_TIMED_SYNC::request& arg, COMMAND_TIMED_SYNC::response& rsp, P2pConnectionContext& context);
    int handle_ping(int command, const COMMAND_PING::request& arg, COMMAND_PING::response& rsp, const P2pConnectionContext& context) const;

    bool init_config();
    bool make_default_config();
    bool store_config();

    bool handshake(CryptoNote::LevinProtocol& proto, P2pConnectionContext& context, bool just_take_peerlist = false);
    bool timedSync();
    bool handleTimedSyncResponse(const BinaryArray& in, P2pConnectionContext& context);
    void forEachConnection(const std::function<void(P2pConnectionContext&)> action);

    void on_connection_new(P2pConnectionContext& context);
    void on_connection_close(P2pConnectionContext& context);

    //----------------- i_p2p_endpoint -------------------------------------------------------------
    void relay_notify_to_all(int command, const BinaryArray &data_buff, const net_connection_id *excludeConnection) override;
    bool invoke_notify_to_peer(int command, const BinaryArray &req_buff, const CryptoNoteConnectionContext &context) override;
    void drop_connection(CryptoNoteConnectionContext &context, bool add_fail) override;
    void for_each_connection(const std::function<void(CryptoNote::CryptoNoteConnectionContext &, PeerIdType)> &f) override;
    void externalRelayNotifyToAll(int command, const BinaryArray &data_buff, const net_connection_id *excludeConnection) override;
    void externalRelayNotifyToList(int command, const BinaryArray &data_buff, const std::list<boost::uuids::uuid> &relayList) override;

    //-----------------------------------------------------------------------------------------------
    bool add_host_fail(const uint32_t address_ip);
    bool block_host(const uint32_t address_ip, time_t seconds = P2P_IP_BLOCKTIME);
    bool unblock_host(const uint32_t address_ip);
    bool is_remote_host_allowed(const uint32_t address_ip);
    bool is_addr_recently_failed(const uint32_t address_ip);
    bool handleConfig(const NetNodeConfig& config);
    bool append_net_address(std::vector<NetworkAddress>& nodes, const std::string& addr);
    bool handle_remote_peerlist(const std::vector<PeerlistEntry>& peerlist, time_t local_time, const CryptoNoteConnectionContext& context);
    bool get_local_node_data(basic_node_data& node_data) const;

    bool fix_time_delta(std::vector<PeerlistEntry>& local_peerlist, time_t local_time, int64_t& delta) const;

    bool connections_maker();
    bool make_new_connection_from_peerlist(bool use_white_list);
    bool make_new_connection_from_anchor_peerlist(const std::vector<AnchorPeerlistEntry>& anchor_peerlist);
    bool try_to_connect_and_handshake_with_new_peer(const NetworkAddress& na, bool just_take_peerlist = false, uint64_t last_seen_stamp = 0, PeerType peer_type = white, uint64_t first_seen_stamp = 0);
    bool is_peer_used(const PeerlistEntry& peer) const;
    bool is_peer_used(const AnchorPeerlistEntry& peer) const;
    bool is_addr_connected(const NetworkAddress& peer) const;
    bool is_priority_node(const NetworkAddress& na) const;
    bool try_ping(const basic_node_data& node_data, const P2pConnectionContext& context);
    bool make_expected_connections_count(PeerType peer_type, size_t expected_connections);

    bool connect_to_peerlist(const std::vector<NetworkAddress>& peers);

    bool parse_peers_and_add_to_container(const boost::program_options::variables_map& vm, 
      const command_line::arg_descriptor<std::vector<std::string> > & arg, std::vector<NetworkAddress>& container) const;

    bool gray_peerlist_housekeeping();

    //debug functions
    std::string print_connections_container() const;

    using ConnectionContainer = std::unordered_map<boost::uuids::uuid, P2pConnectionContext, boost::hash<boost::uuids::uuid>>;
    using ConnectionIterator = ConnectionContainer::iterator;
    ConnectionContainer m_connections;

    void acceptLoop();
    void connectionHandler(const boost::uuids::uuid& connectionId, P2pConnectionContext& connection);
    void writeHandler(P2pConnectionContext& ctx) const;
    void onIdle();
    void connectionWorker();
    void timedSyncLoop();
    void timeoutLoop();

    template<typename T>
    void safeInterrupt(T& obj) const;

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
    bool m_allow_local_ip = false;
    bool m_hide_my_port = false;
    std::string m_p2p_state_filename;

    System::Dispatcher& m_dispatcher;
    System::ContextGroup m_workingContextGroup;
    System::Event m_stopEvent;
    System::Timer m_idleTimer;
    System::Timer m_connTimer;
    System::Timer m_timeoutTimer;
    System::TcpListener m_listener;
    Logging::LoggerRef logger;
    std::atomic<bool> m_stop{false};

    CryptoNoteProtocolHandler& m_payload_handler;
    PeerlistManager m_peerlist;

    OnceInInterval m_peer_handshake_idle_maker_interval;
    OnceInInterval m_connections_maker_interval;
    OnceInInterval m_peerlist_store_interval;
    OnceInInterval m_gray_peerlist_housekeeping_interval;
    System::Timer m_timedSyncTimer;

    std::string m_bind_ip;
    std::string m_port;
    std::vector<NetworkAddress> m_priority_peers;
    std::vector<NetworkAddress> m_exclusive_peers;
    std::vector<NetworkAddress> m_seed_nodes;
    std::vector<PeerlistEntry> m_command_line_peers;
    boost::uuids::uuid m_network_id = CRYPTONOTE_NETWORK;
    std::map<uint32_t, time_t> m_blocked_hosts;
    std::map<uint32_t, uint64_t> m_host_fails_score;
  };
}
