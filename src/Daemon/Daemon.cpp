// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016, The Forknote developers
// Copyright (c) 2018, The TurtleCoin developers
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

#include "version.h"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "DaemonCommandsHandler.h"

#include "Common/FormatTools.h"
#include "Common/SignalHandler.h"
#include "Common/StringTools.h"
#include "Common/PathTools.h"
#include <Common/ColouredMsg.h>
#include "crypto/hash.h"
#include "Checkpoints/CheckpointsData.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/CoreConfig.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/MinerConfig.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"
#include "P2p/NetNode.h"
#include "P2p/NetNodeConfig.h"
#include "Rpc/RpcServer.h"
#include "Rpc/RpcServerConfig.h"
#include "version.h"

#include <Logging/LoggerManager.h>

#if defined(WIN32)
#include <crtdbg.h>
#endif

using Common::JsonValue;
using namespace CryptoNote;
using namespace Logging;

namespace po = boost::program_options;

namespace
{
  const command_line::arg_descriptor<std::string> arg_config_file               = { "config-file", "Specify configuration file", std::string(CryptoNote::CRYPTONOTE_NAME) + ".conf" };
  const command_line::arg_descriptor<bool>        arg_os_version                = { "os-version", "" };
  const command_line::arg_descriptor<std::string> arg_log_file                  = { "log-file", "", "" };
  const command_line::arg_descriptor<int>         arg_log_level                 = { "log-level", "", 2 }; // info level
  const command_line::arg_descriptor<bool>        arg_no_console                = { "no-console", "Disable daemon console commands" };
  const command_line::arg_descriptor<bool>        arg_print_genesis_tx          = { "print-genesis-tx", "Prints genesis' block tx hex to insert it to config and exits" };
  const command_line::arg_descriptor<bool>        arg_testnet_on                = { "testnet", "Used to deploy test nets. Checkpoints and hardcoded seeds are ignored, "
    "network id is changed. Use it with --data-dir flag. The wallet must be launched with --testnet flag.", false };
  const command_line::arg_descriptor<std::string> arg_load_checkpoints          = { "load-checkpoints", "<filename> Load checkpoints from csv file.", "" };
  const command_line::arg_descriptor<bool>        arg_disable_checkpoints       = { "without-checkpoints", "Synchronize without checkpoints" };
  const command_line::arg_descriptor<std::string> arg_rollback                  = { "rollback", "Rollback blockchain to <height>", "", true };

  bool command_line_preprocessor(const boost::program_options::variables_map &vm, LoggerRef &logger) {
    bool exit = false;

    if (command_line::get_arg(vm, command_line::arg_version)) {
      std::cout << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL;
      exit = true;
    }
    if (command_line::get_arg(vm, arg_os_version)) {
      std::cout << "OS: " << Tools::get_os_version_string() << ENDL;
      exit = true;
    }

    if (exit) {
      return true;
    }

    return false;
  }

  void print_genesis_tx_hex(const po::variables_map& vm, LoggerManager& logManager) {
    CryptoNote::Transaction tx = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction();
    std::string tx_hex = Common::toHex(CryptoNote::toBinaryArray(tx));
    std::cout << "Add this line into your coin configuration file as is: " << std::endl;
    std::cout << "\"GENESIS_COINBASE_TX_HEX\":\"" << tx_hex << "\"," << std::endl;
    return;
  }

  JsonValue buildLoggerConfiguration(Level level, const std::string& logfile) {
    JsonValue loggerConfiguration(JsonValue::OBJECT);
    loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

    JsonValue& cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

    JsonValue& fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
    fileLogger.insert("type", "file");
    fileLogger.insert("filename", logfile);
    fileLogger.insert("level", static_cast<int64_t>(TRACE));

    JsonValue& consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
    consoleLogger.insert("type", "console");
    consoleLogger.insert("level", static_cast<int64_t>(TRACE));
    consoleLogger.insert("pattern", "%D %T %L ");

    return loggerConfiguration;
  }

} // end anonymous namespace

int main(int argc, char* argv[])
{

#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

  LoggerManager logManager;
  LoggerRef logger(logManager, "daemon");

  try {

    po::options_description desc_cmd_only("Command line options");
    po::options_description desc_cmd_sett("Command line options and settings options");

    desc_cmd_sett.add_options() 
      ("enable-blockchain-indexes,i", po::bool_switch()->default_value(false), "Enable blockchain indexes");

    command_line::add_arg(desc_cmd_only, command_line::arg_help);
    command_line::add_arg(desc_cmd_only, command_line::arg_version);
    command_line::add_arg(desc_cmd_only, arg_os_version);
    // tools::get_default_data_dir() can't be called during static initialization
    command_line::add_arg(desc_cmd_only, command_line::arg_data_dir, Tools::getDefaultDataDirectory());
    command_line::add_arg(desc_cmd_only, arg_config_file);

    command_line::add_arg(desc_cmd_sett, arg_log_file);
    command_line::add_arg(desc_cmd_sett, arg_log_level);
    command_line::add_arg(desc_cmd_sett, arg_no_console);
    command_line::add_arg(desc_cmd_sett, arg_testnet_on);
    command_line::add_arg(desc_cmd_sett, arg_print_genesis_tx);
    command_line::add_arg(desc_cmd_sett, arg_load_checkpoints);
    command_line::add_arg(desc_cmd_sett, arg_disable_checkpoints);
    command_line::add_arg(desc_cmd_sett, arg_rollback);

    RpcServerConfig::initOptions(desc_cmd_sett);
    CoreConfig::initOptions(desc_cmd_sett);
    NetNodeConfig::initOptions(desc_cmd_sett);
    MinerConfig::initOptions(desc_cmd_sett);

    po::options_description desc_options("Allowed options");
    desc_options.add(desc_cmd_only).add(desc_cmd_sett);

    po::variables_map vm;
    boost::system::error_code ec;
    std::string data_dir = "";
    bool r = command_line::handle_error_helper(desc_options, [&]()
    {
      po::store(po::parse_command_line(argc, argv, desc_options), vm);

      if (command_line::get_arg(vm, command_line::arg_help))
      {
        std::cout << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL << ENDL;
        std::cout << desc_options << std::endl;
        return false;
      }

      data_dir = command_line::get_arg(vm, command_line::arg_data_dir);
      std::string config = command_line::get_arg(vm, arg_config_file);

      boost::filesystem::path data_dir_path(data_dir);
      boost::filesystem::path config_path(config);
      if (!config_path.has_parent_path()) {
        config_path = data_dir_path / config_path;
      }

      if (boost::filesystem::exists(config_path, ec)) {
        po::store(po::parse_config_file<char>(config_path.string<std::string>().c_str(), desc_cmd_sett), vm);
      }
      po::notify(vm);
      if (command_line::get_arg(vm, arg_print_genesis_tx)) {
        print_genesis_tx_hex(vm, logManager);
        return false;
      }
      return true;
    });

    if (!r)
      return 1;
  
    auto modulePath = Common::NativePathToGeneric(argv[0]);
    auto cfgLogFile = Common::NativePathToGeneric(command_line::get_arg(vm, arg_log_file));

    if (cfgLogFile.empty()) {
      cfgLogFile = Common::ReplaceExtenstion(modulePath, ".log");
    } else {
      if (!Common::HasParentPath(cfgLogFile)) {
        cfgLogFile = Common::CombinePath(Common::GetPathDirectory(modulePath), cfgLogFile);
      }
    }

    Level cfgLogLevel = static_cast<Level>(static_cast<int>(Logging::ERROR) + command_line::get_arg(vm, arg_log_level));

    // configure logging
    logManager.configure(buildLoggerConfiguration(cfgLogLevel, cfgLogFile));

    logger(INFO) << CryptoNote::CRYPTONOTE_NAME << " v. " << PROJECT_VERSION_LONG;

    if (command_line_preprocessor(vm, logger)) {
      return 0;
    }

    std::cout << ColouredMsg("\n"
"  _|    _|    _|_|    _|_|_|    _|_|_|      _|_|    \n"
"  _|  _|    _|    _|  _|    _|  _|    _|  _|    _|  \n"
"  _|_|      _|_|_|_|  _|_|_|    _|_|_|    _|    _|  \n",Common::Console::Color::BrightCyan);
    std::cout << ColouredMsg(
"  _|  _|    _|    _|  _|    _|  _|    _|  _|    _|  \n"
"  _|    _|  _|    _|  _|    _|  _|    _|  _|    _|  \n"
"  _|    _|  _|    _|  _|    _|  _|_|_|      _|_|    \n\n", Common::Console::Color::BrightYellow);

    logger(INFO) << "Module folder: " << argv[0];


    bool testnet_mode = command_line::get_arg(vm, arg_testnet_on);
    if (testnet_mode) {
      logger(INFO) << "Starting in testnet mode!";
    }

    CoreConfig coreConfig;
    coreConfig.init(vm);
    NetNodeConfig netNodeConfig;
    netNodeConfig.init(vm);
    netNodeConfig.setTestnet(testnet_mode);
    MinerConfig minerConfig;
    minerConfig.init(vm);
    RpcServerConfig rpcConfig;
    rpcConfig.init(vm);

    std::string contact_str = rpcConfig.contactInfo;
    if (!contact_str.empty() && contact_str.size() > 128) {
      logger(ERROR, BRIGHT_RED) << "Too long contact info";
      return 1;
    }

    // check this early
    if ((rpcConfig.nodeFeeAddress.empty() && !rpcConfig.nodeFeeAmountStr.empty()) ||
       (!rpcConfig.nodeFeeAddress.empty() && rpcConfig.nodeFeeAmountStr.empty())) {
      logger(ERROR, BRIGHT_RED) << "Need to set both, fee-address and fee-amount";
      return 1;
    }

    //create objects and link them
    CryptoNote::CurrencyBuilder currencyBuilder(logManager);
    currencyBuilder.testnet(testnet_mode);
    try {
      currencyBuilder.currency();
    } catch (std::exception&) {
      std::cout << "GENESIS_COINBASE_TX_HEX constant has an incorrect value. Please launch: " << CryptoNote::CRYPTONOTE_NAME << "d --" << arg_print_genesis_tx.name;
      return 1;
    }
    CryptoNote::Currency currency = currencyBuilder.currency();
    System::Dispatcher dispatcher;
    CryptoNote::Core m_core(currency, nullptr, logManager, dispatcher, vm["enable-blockchain-indexes"].as<bool>());

    bool disable_checkpoints = command_line::get_arg(vm, arg_disable_checkpoints);
    if (!disable_checkpoints) {
      CryptoNote::Checkpoints checkpoints(logManager);
      for (const auto& cp : CryptoNote::CHECKPOINTS) {
        checkpoints.add_checkpoint(cp.height, cp.blockId);
      }

#ifndef __ANDROID__
      checkpoints.load_checkpoints_from_dns();
#endif

      bool manual_checkpoints = !command_line::get_arg(vm, arg_load_checkpoints).empty();

      if (manual_checkpoints && !testnet_mode) {
        logger(INFO) << "Loading checkpoints from file...";
        std::string checkpoints_file = command_line::get_arg(vm, arg_load_checkpoints);
        bool results = checkpoints.load_checkpoints_from_file(checkpoints_file);
        if (!results) {
          throw std::runtime_error("Failed to load checkpoints");
        }
      }

      if (!testnet_mode) {
        m_core.set_checkpoints(std::move(checkpoints));
      }
    }

    if (!coreConfig.configFolderDefaulted) {
      if (!Tools::directoryExists(coreConfig.configFolder)) {
        throw std::runtime_error("Directory does not exist: " + coreConfig.configFolder);
      }
    } else {
      if (!Tools::create_directories_if_necessary(coreConfig.configFolder)) {
        throw std::runtime_error("Can't create directory: " + coreConfig.configFolder);
      }
    }

    CryptoNote::CryptoNoteProtocolHandler cprotocol(currency, dispatcher, m_core, nullptr, logManager);
    CryptoNote::NodeServer p2psrv(dispatcher, cprotocol, logManager);
    CryptoNote::RpcServer rpcServer(dispatcher, logManager, m_core, p2psrv, cprotocol);

    cprotocol.set_p2p_endpoint(&p2psrv);
    m_core.set_cryptonote_protocol(&cprotocol);
    DaemonCommandsHandler dch(m_core, p2psrv, logManager, cprotocol, &rpcServer);

    boost::filesystem::path data_dir_path(data_dir);
    boost::filesystem::path chain_file_path(rpcConfig.getChainFile());
    boost::filesystem::path key_file_path(rpcConfig.getKeyFile());
    boost::filesystem::path dh_file_path(rpcConfig.getDhFile());
    if (!chain_file_path.has_parent_path()) {
      chain_file_path = data_dir_path / chain_file_path;
    }
    if (!key_file_path.has_parent_path()) {
      key_file_path = data_dir_path / key_file_path;
    }
    if (!dh_file_path.has_parent_path()) {
      dh_file_path = data_dir_path / dh_file_path;
    }

    // initialize objects
    logger(INFO) << "Initializing p2p server...";
    if (!p2psrv.init(netNodeConfig)) {
      logger(ERROR, BRIGHT_RED) << "Failed to initialize p2p server.";
      return 1;
    }
    logger(INFO) << "P2p server initialized OK";

    // initialize Core here
    logger(INFO) << "Initializing core...";
    if (!m_core.init(coreConfig, minerConfig, true)) {
      logger(ERROR, BRIGHT_RED) << "Failed to initialize core";
      return 1;
    }
    logger(INFO) << "Core initialized OK";

    if (command_line::has_arg(vm, arg_rollback)) {
      std::string rollback_str = command_line::get_arg(vm, arg_rollback);
      if (!rollback_str.empty()) {
        uint32_t _index = 0;
        if (!Common::fromString(rollback_str, _index)) {
          std::cout << "wrong block index parameter" << ENDL;
          return false;
        }
        m_core.rollbackBlockchain(_index);
      }
    }

    // start components
    if (!command_line::has_arg(vm, arg_no_console)) {
      dch.start_handling();
    }

    bool server_ssl_enable = false;
    if (rpcConfig.isEnabledSSL()) {
      if (boost::filesystem::exists(chain_file_path, ec) &&
        boost::filesystem::exists(key_file_path, ec) &&
        boost::filesystem::exists(dh_file_path, ec)) {
        rpcServer.setCerts(boost::filesystem::canonical(chain_file_path).string(),
          boost::filesystem::canonical(key_file_path).string(),
          boost::filesystem::canonical(dh_file_path).string());
        server_ssl_enable = true;
      }
      else {
        logger(ERROR, BRIGHT_RED) << "Start RPC SSL server was canceled because certificate file(s) could not be found" << std::endl;
      }
    }
    std::string ssl_info = "";
    if (server_ssl_enable) ssl_info += ", SSL on address " + rpcConfig.getBindAddressSSL();
    logger(INFO) << "Starting core rpc server on address " << rpcConfig.getBindAddress() << ssl_info;
    rpcServer.start(rpcConfig.getBindIP(), rpcConfig.getBindPort(), rpcConfig.getBindPortSSL(), server_ssl_enable);
    rpcServer.restrictRpc(rpcConfig.restrictedRPC);
    rpcServer.enableCors(rpcConfig.enableCors);
    if (!rpcConfig.nodeFeeAddress.empty() && !rpcConfig.nodeFeeAmountStr.empty()) {
      AccountPublicAddress acc = boost::value_initialized<AccountPublicAddress>();
      if (!currency.parseAccountAddressString(rpcConfig.nodeFeeAddress, acc)) {
        logger(ERROR, BRIGHT_RED) << "Bad fee address: " << rpcConfig.nodeFeeAddress;
        return 1;
      }
      rpcServer.setFeeAddress(rpcConfig.nodeFeeAddress, acc);

      uint64_t fee;
      if (!Common::Format::parseAmount(rpcConfig.nodeFeeAmountStr, fee)) {
        logger(ERROR, BRIGHT_RED) << "Couldn't parse fee amount";
        return 1;
      }
      if (fee > CryptoNote::parameters::COIN) {
        logger(ERROR, BRIGHT_RED) << "Maximum allowed fee is " 
          << Common::Format::formatAmount(CryptoNote::parameters::COIN);
        return 1;
      }

      rpcServer.setFeeAmount(fee);
    }
    
    if (!rpcConfig.nodeFeeViewKey.empty()) {
      rpcServer.setViewKey(rpcConfig.nodeFeeViewKey);
    }
    if (!rpcConfig.contactInfo.empty()) {
      rpcServer.setContactInfo(rpcConfig.contactInfo);
    }
    logger(INFO) << "Core rpc server started ok";

    Tools::SignalHandler::install([&dch, &p2psrv] {
      dch.stop_handling();
      p2psrv.sendStopSignal();
    });

    logger(INFO) << "Starting p2p net loop...";
    p2psrv.run();
    logger(INFO) << "p2p net loop stopped";

    dch.stop_handling();

    //stop components
    logger(INFO) << "Stopping core rpc server...";
    rpcServer.stop();

    //deinitialize components
    logger(INFO) << "Deinitializing core...";
    m_core.deinit();
    logger(INFO) << "Deinitializing p2p...";
    p2psrv.deinit();

    m_core.set_cryptonote_protocol(NULL);
    cprotocol.set_p2p_endpoint(NULL);

  } catch (const std::exception& e) {
    logger(ERROR, BRIGHT_RED) << "Exception: " << e.what();
    return 1;
  }

  logger(INFO) << "Node stopped.";
  return 0;
}

