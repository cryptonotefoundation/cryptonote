// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include "RpcServerConfig.h"
#include "Common/CommandLine.h"
#include "CryptoNoteConfig.h"
#include "android.h"

namespace CryptoNote {

  namespace {

    const std::string DEFAULT_RPC_IP = "127.0.0.1";
    const uint16_t DEFAULT_RPC_PORT = RPC_DEFAULT_PORT;
    const uint16_t DEFAULT_RPC_SSL_PORT = RPC_DEFAULT_SSL_PORT;
    const std::string DEFAULT_RPC_CHAIN_FILE = std::string(RPC_DEFAULT_CHAIN_FILE);
    const std::string DEFAULT_RPC_KEY_FILE = std::string(RPC_DEFAULT_KEY_FILE);
    const std::string DEFAULT_RPC_DH_FILE = std::string(RPC_DEFAULT_DH_FILE);

    const command_line::arg_descriptor<std::string> arg_rpc_bind_ip     = { "rpc-bind-ip", "", DEFAULT_RPC_IP };
    const command_line::arg_descriptor<uint16_t>    arg_rpc_bind_port   = { "rpc-bind-port", "", DEFAULT_RPC_PORT };
    const command_line::arg_descriptor<bool> arg_rpc_bind_ssl_enable    = { "rpc-bind-ssl-enable", "Enable SSL for RPC service", false };
    const command_line::arg_descriptor<uint16_t> arg_rpc_bind_ssl_port  = { "rpc-bind-ssl-port", "SSL port for RPC service", DEFAULT_RPC_SSL_PORT };
    const command_line::arg_descriptor<std::string> arg_chain_file      = { "rpc-chain-file", "SSL chain file", DEFAULT_RPC_CHAIN_FILE };
    const command_line::arg_descriptor<std::string> arg_key_file        = { "rpc-key-file", "SSL key file", DEFAULT_RPC_KEY_FILE };
    const command_line::arg_descriptor<std::string> arg_dh_file         = { "rpc-dh-file", "SSL DH file", DEFAULT_RPC_DH_FILE };
    const command_line::arg_descriptor<bool>        arg_restricted_rpc  = { "restricted-rpc", "Restrict RPC to view only commands to prevent abuse", false };
    const command_line::arg_descriptor<std::string> arg_enable_cors     = { "enable-cors", "Adds header 'Access-Control-Allow-Origin' to the daemon's RPC responses. Uses the value as domain. Use * for all", "" };
    const command_line::arg_descriptor<std::string> arg_set_contact     = { "contact", "Sets node admin contact", "" };
    const command_line::arg_descriptor<std::string> arg_set_fee_address = { "fee-address", "Sets fee address for light wallets.", "" };
    const command_line::arg_descriptor<std::string> arg_set_fee_amount  = { "fee-amount", "Sets flat rate fee for light wallets.", "" };
    const command_line::arg_descriptor<std::string> arg_set_view_key    = { "view-key", "Sets private view key to check for node's fee.", "" };
  }


  RpcServerConfig::RpcServerConfig() : 
    bindIp(DEFAULT_RPC_IP),
    bindPort(DEFAULT_RPC_PORT),
    enableCors(""),
    enableSSL(false),
    restrictedRPC(false),
    contactInfo(""),
    nodeFeeAddress(""),
    nodeFeeAmountStr(""),
    nodeFeeViewKey(""),
    bindPortSSL(RPC_DEFAULT_SSL_PORT) {
  }

  bool RpcServerConfig::isEnabledSSL() const { return enableSSL; }
  uint16_t RpcServerConfig::getBindPort() const { return bindPort; }
  uint16_t RpcServerConfig::getBindPortSSL() const { return bindPortSSL; }
  std::string RpcServerConfig::getBindIP() const { return bindIp; }
  std::string RpcServerConfig::getDhFile() const { return dhFile; }
  std::string RpcServerConfig::getChainFile() const { return chainFile; }
  std::string RpcServerConfig::getKeyFile() const { return keyFile; }
  std::string RpcServerConfig::getBindAddress() const { return bindIp + ":" + std::to_string(bindPort); }
  std::string RpcServerConfig::getBindAddressSSL() const { return bindIp + ":" + std::to_string(bindPortSSL); }

  void RpcServerConfig::initOptions(boost::program_options::options_description& desc) {
    command_line::add_arg(desc, arg_rpc_bind_ip);
    command_line::add_arg(desc, arg_rpc_bind_port);
    command_line::add_arg(desc, arg_rpc_bind_ssl_enable);
    command_line::add_arg(desc, arg_rpc_bind_ssl_port);
    command_line::add_arg(desc, arg_chain_file);
    command_line::add_arg(desc, arg_key_file);
    command_line::add_arg(desc, arg_dh_file);
    command_line::add_arg(desc, arg_restricted_rpc);
    command_line::add_arg(desc, arg_set_contact);
    command_line::add_arg(desc, arg_enable_cors);
    command_line::add_arg(desc, arg_set_fee_address);
    command_line::add_arg(desc, arg_set_fee_amount);
    command_line::add_arg(desc, arg_set_view_key);
  }

  void RpcServerConfig::init(const boost::program_options::variables_map& vm)  {
    bindIp = command_line::get_arg(vm, arg_rpc_bind_ip);
    bindPort = command_line::get_arg(vm, arg_rpc_bind_port);
    enableSSL = command_line::get_arg(vm, arg_rpc_bind_ssl_enable);
    bindPortSSL = command_line::get_arg(vm, arg_rpc_bind_ssl_port);
    chainFile = command_line::get_arg(vm, arg_chain_file);
    keyFile = command_line::get_arg(vm, arg_key_file);
    dhFile = command_line::get_arg(vm, arg_dh_file);
    enableCors = command_line::get_arg(vm, arg_enable_cors);
    restrictedRPC = command_line::get_arg(vm, arg_restricted_rpc);
    contactInfo = command_line::get_arg(vm, arg_set_contact);
    nodeFeeAddress = command_line::get_arg(vm, arg_set_fee_address);
    nodeFeeAmountStr = command_line::get_arg(vm, arg_set_fee_amount);
    nodeFeeViewKey = command_line::get_arg(vm, arg_set_view_key);
  }

}
