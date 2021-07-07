// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero project
// Copyright (c) 2014-2018, The Forknote developers
// Copyright (c) 2016-2018, The Karbowanec developers
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

#include <boost/format.hpp>
#include "Common/ConsoleHandler.h"
#include "CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"
#include <Logging/LoggerRef.h>
#include <Logging/LoggerManager.h>
#include "Rpc/RpcServer.h"

namespace CryptoNote {
class Core;
class NodeServer;
class ICryptoNoteProtocolQuery;
}

class DaemonCommandsHandler
{
public:
  DaemonCommandsHandler(CryptoNote::Core& core, CryptoNote::NodeServer& srv, Logging::LoggerManager& log, const CryptoNote::ICryptoNoteProtocolQuery& protocol, CryptoNote::RpcServer* prpc_server);

  bool start_handling() {
    m_consoleHandler.start();
    return true;
  }

  void stop_handling() {
    m_consoleHandler.stop();
  }

private:

  Common::ConsoleHandler m_consoleHandler;
  CryptoNote::Core& m_core;
  CryptoNote::NodeServer& m_srv;
  Logging::LoggerRef logger;
  Logging::LoggerManager& m_logManager;
  const CryptoNote::ICryptoNoteProtocolQuery& protocolQuery;
  CryptoNote::RpcServer* m_prpc_server;
  
  std::string get_commands_str();
  std::string get_mining_speed(uint64_t hr);
  float get_sync_percentage(uint64_t height, uint64_t target_height);
  bool print_block_by_height(uint32_t height);
  bool print_block_by_hash(const std::string& arg);

  bool exit(const std::vector<std::string>& args);
  bool help(const std::vector<std::string>& args);
  bool print_pl(const std::vector<std::string>& args);
  bool show_hr(const std::vector<std::string>& args);
  bool hide_hr(const std::vector<std::string>& args);
  bool print_bc_outs(const std::vector<std::string>& args);
  bool print_cn(const std::vector<std::string>& args);
  bool print_bc(const std::vector<std::string>& args);
  bool print_bci(const std::vector<std::string>& args);
  bool print_height(const std::vector<std::string>& args);
  bool set_log(const std::vector<std::string>& args);
  bool print_block(const std::vector<std::string>& args);
  bool print_tx(const std::vector<std::string>& args);
  bool print_pool(const std::vector<std::string>& args);
  bool print_pool_sh(const std::vector<std::string>& args);
  bool print_pool_count(const std::vector<std::string>& args);
  bool start_mining(const std::vector<std::string>& args);
  bool stop_mining(const std::vector<std::string>& args);
  bool print_diff(const std::vector<std::string>& args);
  bool print_ban(const std::vector<std::string>& args);
  bool ban(const std::vector<std::string>& args);
  bool unban(const std::vector<std::string>& args);
  bool status(const std::vector<std::string>& args);
  bool save(const std::vector<std::string>& args);
};
