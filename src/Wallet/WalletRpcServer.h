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

#pragma  once

#include <future>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include "WalletRpcServerCommandsDefinitions.h"
#include "WalletLegacy/WalletLegacy.h"
#include "Common/CommandLine.h"
#include "Rpc/HttpServer.h"

#include <Logging/LoggerRef.h>

namespace Tools
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class wallet_rpc_server : CryptoNote::HttpServer
  {
  public:

    wallet_rpc_server(
      System::Dispatcher& dispatcher, 
      Logging::ILogger& log,
      CryptoNote::IWalletLegacy &w, 
      CryptoNote::INode &n, 
      CryptoNote::Currency& currency,
      const std::string& walletFilename);


    static void init_options(boost::program_options::options_description& desc);
    bool init(const boost::program_options::variables_map& vm);
    
    bool run();
    void send_stop_signal();

    static const command_line::arg_descriptor<uint16_t> arg_rpc_bind_port;
    static const command_line::arg_descriptor<std::string> arg_rpc_bind_ip;

  private:

    virtual void processRequest(const CryptoNote::HttpRequest& request, CryptoNote::HttpResponse& response) override;

    //json_rpc
    bool on_getbalance(const wallet_rpc::COMMAND_RPC_GET_BALANCE::request& req, wallet_rpc::COMMAND_RPC_GET_BALANCE::response& res);
    bool on_transfer(const wallet_rpc::COMMAND_RPC_TRANSFER::request& req, wallet_rpc::COMMAND_RPC_TRANSFER::response& res);
    bool on_store(const wallet_rpc::COMMAND_RPC_STORE::request& req, wallet_rpc::COMMAND_RPC_STORE::response& res);
    bool on_get_payments(const wallet_rpc::COMMAND_RPC_GET_PAYMENTS::request& req, wallet_rpc::COMMAND_RPC_GET_PAYMENTS::response& res);
    bool on_get_transfers(const wallet_rpc::COMMAND_RPC_GET_TRANSFERS::request& req, wallet_rpc::COMMAND_RPC_GET_TRANSFERS::response& res);
    bool on_get_height(const wallet_rpc::COMMAND_RPC_GET_HEIGHT::request& req, wallet_rpc::COMMAND_RPC_GET_HEIGHT::response& res);
    bool on_reset(const wallet_rpc::COMMAND_RPC_RESET::request& req, wallet_rpc::COMMAND_RPC_RESET::response& res);

    bool handle_command_line(const boost::program_options::variables_map& vm);

    Logging::LoggerRef logger;
    CryptoNote::IWalletLegacy& m_wallet;
    CryptoNote::INode& m_node;
    uint16_t m_port;
    std::string m_bind_ip;
    CryptoNote::Currency& m_currency;
    const std::string m_walletFilename;

    System::Dispatcher& m_dispatcher;
    System::Event m_stopComplete;
  };
}
