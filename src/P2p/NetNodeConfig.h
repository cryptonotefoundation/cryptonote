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

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include <boost/program_options.hpp>
#include "P2pProtocolTypes.h"
#include "Common/CommandLine.h"
#include "CryptoNoteConfig.h"

namespace CryptoNote {

  const command_line::arg_descriptor<std::string> arg_p2p_bind_ip                          = { "p2p-bind-ip", "Interface for p2p network protocol", "0.0.0.0" };
  const command_line::arg_descriptor<uint16_t>    arg_p2p_bind_port                        = { "p2p-bind-port", "Port for p2p network protocol", P2P_DEFAULT_PORT };
  const command_line::arg_descriptor<uint16_t>    arg_p2p_external_port                    = { "p2p-external-port", "External port for p2p network protocol (if port forwarding used with NAT)", 0 };
  const command_line::arg_descriptor<bool>        arg_p2p_allow_local_ip                   = { "allow-local-ip", "Allow local ip add to peer list, mostly in debug purposes" };
  const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_peer           = { "add-peer", "Manually add peer to local peerlist" };
  const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_priority_node  = { "add-priority-node", "Specify list of peers to connect to and attempt to keep the connection open" };
  const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_exclusive_node = { "add-exclusive-node", "Specify list of peers to connect to only."
                                                                                               " If this option is given the options add-priority-node and seed-node are ignored" };
  const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_seed_node          = { "seed-node", "Connect to a node to retrieve peer addresses, and disconnect" };
  const command_line::arg_descriptor<std::string> arg_ban_list                             = { "ban-list", "Specify ban list file, one IP address per line", "", true };
  const command_line::arg_descriptor<bool> arg_p2p_hide_my_port                            = { "hide-my-port", "Do not announce yourself as peerlist candidate", false, true };

class NetNodeConfig {
public:
  NetNodeConfig();
  static void initOptions(boost::program_options::options_description& desc);
  bool init(const boost::program_options::variables_map& vm);

  std::string getP2pStateFilename() const;
  bool getTestnet() const;
  std::string getBindIp() const;
  uint16_t getBindPort() const;
  uint16_t getExternalPort() const;
  bool getAllowLocalIp() const;
  std::vector<PeerlistEntry> getPeers() const;
  std::vector<NetworkAddress> getPriorityNodes() const;
  std::vector<NetworkAddress> getExclusiveNodes() const;
  std::vector<NetworkAddress> getSeedNodes() const;
  std::vector<uint32_t> getBanList() const;
  bool getHideMyPort() const;
  std::string getConfigFolder() const;

  void setP2pStateFilename(const std::string& filename);
  void setTestnet(bool isTestnet);
  void setBindIp(const std::string& ip);
  void setBindPort(uint16_t port);
  void setExternalPort(uint16_t port);
  void setAllowLocalIp(bool allow);
  void setPeers(const std::vector<PeerlistEntry>& peerList);
  void setPriorityNodes(const std::vector<NetworkAddress>& addresses);
  void setExclusiveNodes(const std::vector<NetworkAddress>& addresses);
  void setSeedNodes(const std::vector<NetworkAddress>& addresses);
  void setHideMyPort(bool hide);
  void setConfigFolder(const std::string& folder);

private:
  std::string bindIp;
  uint16_t bindPort;
  uint16_t externalPort;
  bool allowLocalIp;
  std::vector<PeerlistEntry> peers;
  std::vector<NetworkAddress> priorityNodes;
  std::vector<NetworkAddress> exclusiveNodes;
  std::vector<NetworkAddress> seedNodes;
  std::vector<uint32_t> banList;
  bool hideMyPort;
  std::string configFolder;
  std::string p2pStateFilename;
  bool testnet;
};

} //namespace nodetool
