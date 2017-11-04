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

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include <System/ContextGroup.h>
#include <System/ContextGroupTimeout.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/Ipv4Resolver.h>
#include <System/TcpConnection.h>
#include <System/TcpConnector.h>
#include <System/Timer.h>

#include "Common/CommandLine.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"
#include "P2p/P2pProtocolDefinitions.h"
#include "P2p/LevinProtocol.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"
#include "Serialization/SerializationTools.h"
#include "version.h"

namespace po = boost::program_options;
using namespace CryptoNote;

#ifndef ENDL
#define ENDL std::endl
#endif

namespace {
  const command_line::arg_descriptor<std::string, true> arg_ip           = {"ip", "set ip"};
  const command_line::arg_descriptor<uint16_t>      arg_port = { "port", "set port" };
  const command_line::arg_descriptor<uint16_t>      arg_rpc_port           = {"rpc_port", "set rpc port"};
  const command_line::arg_descriptor<uint32_t, true> arg_timeout         = {"timeout", "set timeout"};
  const command_line::arg_descriptor<std::string> arg_priv_key           = {"private_key", "private key to subscribe debug command", "", true};
  const command_line::arg_descriptor<uint64_t>    arg_peer_id            = {"peer_id", "peer_id if known(if not - will be requested)", 0};
  const command_line::arg_descriptor<bool>        arg_generate_keys      = {"generate_keys_pair", "generate private and public keys pair"};
  const command_line::arg_descriptor<bool>        arg_request_stat_info  = {"request_stat_info", "request statistics information"};
  const command_line::arg_descriptor<bool>        arg_request_net_state  = {"request_net_state", "request network state information (peer list, connections count)"};
  const command_line::arg_descriptor<bool>        arg_get_daemon_info    = {"rpc_get_daemon_info", "request daemon state info vie rpc (--rpc_port option should be set ).", "", true};
}

struct response_schema {
  std::string status;
  std::string COMMAND_REQUEST_STAT_INFO_status;
  std::string COMMAND_REQUEST_NETWORK_STATE_status;
  boost::optional<COMMAND_REQUEST_STAT_INFO::response> si_rsp;
  boost::optional<COMMAND_REQUEST_NETWORK_STATE::response> ns_rsp;
};

void withTimeout(System::Dispatcher& dispatcher, unsigned timeout, std::function<void()> f) {
  std::string result;
  System::ContextGroup cg(dispatcher);
  System::ContextGroupTimeout cgTimeout(dispatcher, cg, std::chrono::milliseconds(timeout));
  
  cg.spawn([&] {
    try {
      f();
    } catch (System::InterruptedException&) {
      result = "Operation timeout";
    } catch (std::exception& e) {
      result = e.what();
    }
  });

  cg.wait();

  if (!result.empty()) {
    throw std::runtime_error(result);
  }
}


std::ostream& get_response_schema_as_json(std::ostream& ss, response_schema &rs) {
  
  ss << "{" << ENDL
     << "  \"status\": \"" << rs.status << "\"," << ENDL
     << "  \"COMMAND_REQUEST_NETWORK_STATE_status\": \"" << rs.COMMAND_REQUEST_NETWORK_STATE_status << "\"," << ENDL
     << "  \"COMMAND_REQUEST_STAT_INFO_status\": \"" << rs.COMMAND_REQUEST_STAT_INFO_status << "\"";

  if (rs.si_rsp.is_initialized()) {
    ss << "," << ENDL << "  \"si_rsp\": " << storeToJson(rs.si_rsp.get());
  }

  if (rs.ns_rsp.is_initialized()) {
    const auto& networkState = rs.ns_rsp.get();

    ss << "," << ENDL << "  \"ns_rsp\": {" << ENDL
       << "    \"local_time\": " << networkState.local_time << "," << ENDL
       << "    \"my_id\": \"" << networkState.my_id << "\"," << ENDL
       << "    \"connections_list\": [" << ENDL;

    size_t i = 0;
    for (const connection_entry &ce : networkState.connections_list) {
      ss << "      {\"peer_id\": \"" << ce.id << "\", \"ip\": \"" << Common::ipAddressToString(ce.adr.ip) << "\", \"port\": " << ce.adr.port << ", \"is_income\": " << ce.is_income << "}";
      if (networkState.connections_list.size() - 1 != i)
        ss << ",";
      ss << ENDL;
      i++;
    }
    ss << "    ]," << ENDL;
    ss << "    \"local_peerlist_white\": [" << ENDL;
    i = 0;
    for (const PeerlistEntry &pe : networkState.local_peerlist_white) {
      ss << "      {\"peer_id\": \"" << pe.id << "\", \"ip\": \"" << Common::ipAddressToString(pe.adr.ip) << "\", \"port\": " << pe.adr.port << ", \"last_seen\": " << networkState.local_time - pe.last_seen << "}";
      if (networkState.local_peerlist_white.size() - 1 != i)
        ss << ",";
      ss << ENDL;
      i++;
    }
    ss << "    ]," << ENDL;

    ss << "    \"local_peerlist_gray\": [" << ENDL;
    i = 0;
    for (const PeerlistEntry &pe : networkState.local_peerlist_gray) {
      ss << "      {\"peer_id\": \"" << pe.id << "\", \"ip\": \"" << Common::ipAddressToString(pe.adr.ip) << "\", \"port\": " << pe.adr.port << ", \"last_seen\": " << networkState.local_time - pe.last_seen << "}";
      if (networkState.local_peerlist_gray.size() - 1 != i)
        ss << ",";
      ss << ENDL;
      i++;
    }
    ss << "    ]" << ENDL << "  }" << ENDL;
  }

  ss << "}";

  return ss;
}

//---------------------------------------------------------------------------------------------------------------
bool print_COMMAND_REQUEST_STAT_INFO(const COMMAND_REQUEST_STAT_INFO::response &si) {
  std::cout << " ------ COMMAND_REQUEST_STAT_INFO ------ " << ENDL;
  std::cout << "Version:             " << si.version << ENDL;
  std::cout << "OS Version:          " << si.os_version << ENDL;
  std::cout << "Connections:          " << si.connections_count << ENDL;
  std::cout << "INC Connections:     " << si.incoming_connections_count << ENDL;


  std::cout << "Tx pool size:        " << si.payload_info.transactionPoolSize << ENDL;
  std::cout << "BC height:           " << si.payload_info.blockchainHeight << ENDL;
  std::cout << "Mining speed:          " << si.payload_info.miningSpeed << ENDL;
  std::cout << "Alternative blocks:  " << si.payload_info.alternativeBlockCount << ENDL;
  std::cout << "Top block id:        " << si.payload_info.topBlockHashString << ENDL;
  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool print_COMMAND_REQUEST_NETWORK_STATE(const COMMAND_REQUEST_NETWORK_STATE::response &ns) {
  std::cout << " ------ COMMAND_REQUEST_NETWORK_STATE ------ " << ENDL;
  std::cout << "Peer id: " << ns.my_id << ENDL;
  std::cout << "Active connections:" << ENDL;

  for (const connection_entry &ce : ns.connections_list) {
    std::cout << ce.id << "\t" << ce.adr << (ce.is_income ? "(INC)" : "(OUT)") << ENDL;
  }

  std::cout << "Peer list white:" << ns.my_id << ENDL;
  for (const PeerlistEntry &pe : ns.local_peerlist_white) {
    std::cout << pe.id << "\t" << pe.adr << "\t" << Common::timeIntervalToString(ns.local_time - pe.last_seen) << ENDL;
  }

  std::cout << "Peer list gray:" << ns.my_id << ENDL;
  for (const PeerlistEntry &pe : ns.local_peerlist_gray) {
    std::cout << pe.id << "\t" << pe.adr << "\t" << Common::timeIntervalToString(ns.local_time - pe.last_seen) << ENDL;
  }

  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool handle_get_daemon_info(po::variables_map& vm) {
  if(!command_line::has_arg(vm, arg_rpc_port)) {
    std::cout << "ERROR: rpc port not set" << ENDL;
    return false;
  }

  try {
    System::Dispatcher dispatcher;
    HttpClient httpClient(dispatcher, command_line::get_arg(vm, arg_ip), command_line::get_arg(vm, arg_rpc_port));

    CryptoNote::COMMAND_RPC_GET_INFO::request req;
    CryptoNote::COMMAND_RPC_GET_INFO::response res;

    invokeJsonCommand(httpClient, "/getinfo", req, res); // TODO: timeout

    std::cout << "OK" << ENDL
      << "height: " << res.height << ENDL
      << "difficulty: " << res.difficulty << ENDL
      << "tx_count: " << res.tx_count << ENDL
      << "tx_pool_size: " << res.tx_pool_size << ENDL
      << "alt_blocks_count: " << res.alt_blocks_count << ENDL
      << "outgoing_connections_count: " << res.outgoing_connections_count << ENDL
      << "incoming_connections_count: " << res.incoming_connections_count << ENDL
      << "white_peerlist_size: " << res.white_peerlist_size << ENDL
      << "grey_peerlist_size: " << res.grey_peerlist_size << ENDL;

  } catch (const std::exception& e) {
    std::cout << "ERROR: " << e.what() << std::endl;
    return false;
  }

  return true;
}
//---------------------------------------------------------------------------------------------------------------
bool handle_request_stat(po::variables_map& vm, PeerIdType peer_id) {
  if(!command_line::has_arg(vm, arg_priv_key)) {
    std::cout << "{" << ENDL << "  \"status\": \"ERROR: " << "secret key not set \"" << ENDL << "}";
    return false;
  }

  Crypto::SecretKey prvk;
  if (!Common::podFromHex(command_line::get_arg(vm, arg_priv_key), prvk)) {
    std::cout << "{" << ENDL << "  \"status\": \"ERROR: " << "wrong secret key set \"" << ENDL << "}";
    return false;
  }

  response_schema rs;
  unsigned timeout = command_line::get_arg(vm, arg_timeout);

  try {
    System::Dispatcher dispatcher;
    System::TcpConnector connector(dispatcher);
    System::Ipv4Resolver resolver(dispatcher);

    std::cout << "Connecting to " << command_line::get_arg(vm, arg_ip) << ":" << command_line::get_arg(vm, arg_port) << ENDL;

    auto addr = resolver.resolve(command_line::get_arg(vm, arg_ip));

    System::TcpConnection connection;

    withTimeout(dispatcher, timeout, [&] {
      connection = connector.connect(addr, command_line::get_arg(vm, arg_port));
    });

    rs.status = "OK";

    LevinProtocol levin(connection);

    if (!peer_id) {
      COMMAND_REQUEST_PEER_ID::request req;
      COMMAND_REQUEST_PEER_ID::response rsp;
      withTimeout(dispatcher, timeout, [&] {
        levin.invoke(COMMAND_REQUEST_PEER_ID::ID, req, rsp);
      });
      peer_id = rsp.my_id;
    }

    proof_of_trust pot;
    pot.peer_id = peer_id;
    pot.time = time(NULL);
    Crypto::PublicKey pubk;
    Common::podFromHex(P2P_STAT_TRUSTED_PUB_KEY, pubk);
    Crypto::Hash h = get_proof_of_trust_hash(pot);
    Crypto::generate_signature(h, pubk, prvk, pot.sign);

    if (command_line::get_arg(vm, arg_request_stat_info)) {
      COMMAND_REQUEST_STAT_INFO::request req;
      COMMAND_REQUEST_STAT_INFO::response res;

      req.tr = pot;

      try {
        withTimeout(dispatcher, timeout, [&] {
          levin.invoke(COMMAND_REQUEST_STAT_INFO::ID, req, res);
        });
        rs.si_rsp = std::move(res);
        rs.COMMAND_REQUEST_STAT_INFO_status = "OK";
      } catch (const std::exception &e) {
        std::stringstream ss;
        ss << "ERROR: Failed to invoke remote command COMMAND_REQUEST_STAT_INFO to " 
           << command_line::get_arg(vm, arg_ip) << ":" << command_line::get_arg(vm, arg_port) 
           << " - " << e.what();
        rs.COMMAND_REQUEST_STAT_INFO_status = ss.str();
      }
    }

    if (command_line::get_arg(vm, arg_request_net_state))  {
      ++pot.time;
      h = get_proof_of_trust_hash(pot);
      Crypto::generate_signature(h, pubk, prvk, pot.sign);
      COMMAND_REQUEST_NETWORK_STATE::request req{ pot };
      COMMAND_REQUEST_NETWORK_STATE::response res;

      try {
        withTimeout(dispatcher, timeout, [&] {
          levin.invoke(COMMAND_REQUEST_NETWORK_STATE::ID, req, res);
        });
        rs.ns_rsp = std::move(res);
        rs.COMMAND_REQUEST_NETWORK_STATE_status = "OK";
      } catch (const std::exception &e) {
        std::stringstream ss;
        ss << "ERROR: Failed to invoke remote command COMMAND_REQUEST_NETWORK_STATE to "
           << command_line::get_arg(vm, arg_ip) << ":" << command_line::get_arg(vm, arg_port)
           << " - " << e.what();
        rs.COMMAND_REQUEST_NETWORK_STATE_status = ss.str();
      }
    }
  } catch (const std::exception& e) {
    std::cout << "ERROR: " << e.what() << std::endl;
    return false;
  }

  get_response_schema_as_json(std::cout, rs) << std::endl;
  return true;
}

//---------------------------------------------------------------------------------------------------------------
bool generate_and_print_keys() {
  Crypto::PublicKey pk;
  Crypto::SecretKey sk;
  generate_keys(pk, sk);
  std::cout << "PUBLIC KEY: " << Common::podToHex(pk) << ENDL
            << "PRIVATE KEY: " << Common::podToHex(sk);
  return true;
}

int main(int argc, char *argv[]) {
  // Declare the supported options.
  po::options_description desc_general("General options");
  command_line::add_arg(desc_general, command_line::arg_help);

  po::options_description desc_params("Connectivity options");
  command_line::add_arg(desc_params, arg_ip);
  command_line::add_arg(desc_params, arg_port);
  command_line::add_arg(desc_params, arg_rpc_port);
  command_line::add_arg(desc_params, arg_timeout);
  command_line::add_arg(desc_params, arg_request_stat_info);
  command_line::add_arg(desc_params, arg_request_net_state);
  command_line::add_arg(desc_params, arg_generate_keys);
  command_line::add_arg(desc_params, arg_peer_id);
  command_line::add_arg(desc_params, arg_priv_key);
  command_line::add_arg(desc_params, arg_get_daemon_info);

  po::options_description desc_all;
  desc_all.add(desc_general).add(desc_params);

  po::variables_map vm;
  bool r = command_line::handle_error_helper(desc_all, [&]() {
    po::store(command_line::parse_command_line(argc, argv, desc_general, true), vm);
    if (command_line::get_arg(vm, command_line::arg_help))
    {
      std::cout << desc_all << ENDL;
      return false;
    }

    po::store(command_line::parse_command_line(argc, argv, desc_params, false), vm);
    po::notify(vm);

    return true;
  });

  if (!r)
    return 1;

  if (command_line::has_arg(vm, arg_request_stat_info) || command_line::has_arg(vm, arg_request_net_state)) {
    return handle_request_stat(vm, command_line::get_arg(vm, arg_peer_id)) ? 0 : 1;
  }
  
  if (command_line::has_arg(vm, arg_get_daemon_info)) {
    return handle_get_daemon_info(vm) ? 0 : 1;
  } 
  
  if (command_line::has_arg(vm, arg_generate_keys)) {
    return generate_and_print_keys() ? 0 : 1;
  }

  std::cerr << "Not enough arguments." << ENDL;
  std::cerr << desc_all << ENDL;
  return 1;
}
