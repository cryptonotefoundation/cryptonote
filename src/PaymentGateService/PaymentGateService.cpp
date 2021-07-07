// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2017, XDN Project developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2016-2020 The Karbo developers
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

#include "PaymentGateService.h"

#include <future>
#include <boost/filesystem.hpp>

#include "Common/SignalHandler.h"
#include "InProcessNode/InProcessNode.h"
#include "Logging/LoggerRef.h"
#include "PaymentGate/PaymentServiceJsonRpcServer.h"

#include "Checkpoints/CheckpointsData.h"
#include "CryptoNoteCore/CoreConfig.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include "Rpc/RpcServer.h"
#include <System/Context.h>
#include "Wallet/WalletGreen.h"

#ifdef ERROR
#undef ERROR
#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

using namespace PaymentService;

bool validateSertPath(const std::string& rootPath,
                      const std::string& config_chain_file,
                      const std::string& config_key_file,
                      const std::string& config_dh_file,
                      std::string& chain_file,
                      std::string& key_file,
                      std::string& dh_file) {
  bool res = false;
  boost::system::error_code ec;
  boost::filesystem::path data_dir_path(rootPath);
  boost::filesystem::path chain_file_path(config_chain_file);
  boost::filesystem::path key_file_path(config_key_file);
  boost::filesystem::path dh_file_path(config_dh_file);
  if (!chain_file_path.has_parent_path()) chain_file_path = data_dir_path / chain_file_path;
  if (!key_file_path.has_parent_path()) key_file_path = data_dir_path / key_file_path;
  if (!dh_file_path.has_parent_path()) dh_file_path = data_dir_path / dh_file_path;
  if (boost::filesystem::exists(chain_file_path, ec) &&
      boost::filesystem::exists(key_file_path, ec) &&
      boost::filesystem::exists(dh_file_path, ec)) {
        chain_file = boost::filesystem::canonical(chain_file_path).string();
        key_file = boost::filesystem::canonical(key_file_path).string();
        dh_file = boost::filesystem::canonical(dh_file_path).string();
        res = true;
  }
  return res;
}

void changeDirectory(const std::string& path) {
  if (chdir(path.c_str())) {
    throw std::runtime_error("Couldn't change directory to \'" + path + "\': " + strerror(errno));
  }
}

void stopSignalHandler(PaymentGateService* pg) {
  pg->stop();
}

PaymentGateService::PaymentGateService() :
  dispatcher(nullptr),
  stopEvent(nullptr),
  config(),
  service(nullptr),
  logger(),
  currencyBuilder(logger),
  fileLogger(Logging::TRACE),
  consoleLogger(Logging::INFO) {
  consoleLogger.setPattern("%D %T %L ");
  fileLogger.setPattern("%D %T %L ");
}

bool PaymentGateService::init(int argc, char** argv) {
  if (!config.init(argc, argv)) {
    return false;
  }

  logger.setMaxLevel(static_cast<Logging::Level>(config.gateConfiguration.logLevel));
  logger.setPattern("%D %T %L ");
  logger.addLogger(consoleLogger);

  Logging::LoggerRef log(logger, "main");

  if (config.gateConfiguration.testnet) {
    log(Logging::INFO) << "Starting in testnet mode";
    currencyBuilder.testnet(true);
  }

  if (!config.gateConfiguration.serverRoot.empty()) {
    changeDirectory(config.gateConfiguration.serverRoot);
    log(Logging::INFO) << "Current working directory now is " << config.gateConfiguration.serverRoot;
  }

  fileStream.open(config.gateConfiguration.logFile, std::ofstream::app);

  if (!fileStream) {
    throw std::runtime_error("Couldn't open log file");
  }

  fileLogger.attachToStream(fileStream);
  logger.addLogger(fileLogger);

  return true;
}

WalletConfiguration PaymentGateService::getWalletConfig() const {
  return WalletConfiguration{
    config.gateConfiguration.containerFile,
    config.gateConfiguration.containerPassword,
    config.gateConfiguration.secretViewKey,
    config.gateConfiguration.secretSpendKey,
    config.gateConfiguration.mnemonicSeed,
    config.gateConfiguration.generateDeterministic
  };
}

const CryptoNote::Currency PaymentGateService::getCurrency() {
  return currencyBuilder.currency();
}

void PaymentGateService::run() {

  System::Dispatcher localDispatcher;
  System::Event localStopEvent(localDispatcher);

  this->dispatcher = &localDispatcher;
  this->stopEvent = &localStopEvent;

  Tools::SignalHandler::install(std::bind(&stopSignalHandler, this));

  Logging::LoggerRef log(logger, "run");

  if (config.startInprocess) {
    runInProcess(log);
  } else {
    runRpcProxy(log);
  }

  this->dispatcher = nullptr;
  this->stopEvent = nullptr;
}

void PaymentGateService::stop() {
  Logging::LoggerRef log(logger, "stop");

  log(Logging::INFO, Logging::BRIGHT_WHITE) << "Stop signal caught";

  if (dispatcher != nullptr) {
    dispatcher->remoteSpawn([&]() {
      if (stopEvent != nullptr) {
        stopEvent->set();
      }
    });
  }
}

void PaymentGateService::runInProcess(Logging::LoggerRef& log) {
  if (!config.coreConfig.configFolderDefaulted) {
    if (!Tools::directoryExists(config.coreConfig.configFolder)) {
      throw std::runtime_error("Directory does not exist: " + config.coreConfig.configFolder);
    }
  } else {
    if (!Tools::create_directories_if_necessary(config.coreConfig.configFolder)) {
      throw std::runtime_error("Can't create directory: " + config.coreConfig.configFolder);
    }
  }

  log(Logging::INFO) << "Starting Payment Gate with local node";

  CryptoNote::Currency currency = currencyBuilder.currency();
  CryptoNote::Core core(currency, NULL, logger, *dispatcher, false);

  CryptoNote::CryptoNoteProtocolHandler protocol(currency, *dispatcher, core, NULL, logger);
  CryptoNote::NodeServer p2pNode(*dispatcher, protocol, logger);
  CryptoNote::RpcServer rpcServer(*dispatcher, logger, core, p2pNode, protocol);
  CryptoNote::Checkpoints checkpoints(logger);
  for (const auto& cp : CryptoNote::CHECKPOINTS) {
    checkpoints.add_checkpoint(cp.height, cp.blockId);
  }
  checkpoints.load_checkpoints_from_dns();
  if (!config.gateConfiguration.testnet) {
    core.set_checkpoints(std::move(checkpoints));
  }

  protocol.set_p2p_endpoint(&p2pNode);
  core.set_cryptonote_protocol(&protocol);

  log(Logging::INFO) << "initializing p2pNode";
  if (!p2pNode.init(config.netNodeConfig)) {
    throw std::runtime_error("Failed to init p2pNode");
  }

  log(Logging::INFO) << "initializing core";
  CryptoNote::MinerConfig emptyMiner;
  core.init(config.coreConfig, emptyMiner, true);

  std::promise<std::error_code> initPromise;
  auto initFuture = initPromise.get_future();

  std::unique_ptr<CryptoNote::INode> node(new CryptoNote::InProcessNode(core, protocol));

  node->init([&initPromise, &log](std::error_code ec) {
    if (ec) {
      log(Logging::WARNING, Logging::YELLOW) << "Failed to init node: " << ec.message();
    } else {
      log(Logging::INFO) << "node is inited successfully";
    }

    initPromise.set_value(ec);
  });

  auto ec = initFuture.get();
  if (ec) {
    throw std::system_error(ec);
  }

  bool rpc_run_ssl = false;
  std::string rpc_chain_file = "";
  std::string rpc_key_file = "";
  std::string rpc_dh_file = "";

  if (config.remoteNodeConfig.m_enable_ssl) {
    if (validateSertPath(config.coreConfig.configFolder,
        config.remoteNodeConfig.m_chain_file,
        config.remoteNodeConfig.m_key_file,
        config.remoteNodeConfig.m_dh_file,
        rpc_chain_file,
        rpc_key_file,
        rpc_dh_file)) {
      rpcServer.setCerts(rpc_chain_file, rpc_key_file, rpc_dh_file);
      rpc_run_ssl = true;
    } else {
      log((Logging::Level) Logging::ERROR, Logging::BRIGHT_RED) << "Start RPC SSL server was canceled because certificate file(s) could not be found" << std::endl;
    }
  }

  log(Logging::INFO) << "Starting core rpc server on "
	  << config.remoteNodeConfig.m_daemon_host << ":" << config.remoteNodeConfig.m_daemon_port;
  rpcServer.start(config.remoteNodeConfig.m_daemon_host,
                  config.remoteNodeConfig.m_daemon_port,
                  config.remoteNodeConfig.m_daemon_port_ssl,
                  rpc_run_ssl);
  log(Logging::INFO) << "Core rpc server started ok";

  log(Logging::INFO) << "Spawning p2p server";

  System::Event p2pStarted(*dispatcher);

  System::Context<> context(*dispatcher, [&]() {
    p2pStarted.set();
    p2pNode.run();
  });

  p2pStarted.wait();

  if (config.gateConfiguration.generateNewContainer) {
    generateNewWallet(currency, getWalletConfig(), logger, *dispatcher, *node);
  }
  else if (config.gateConfiguration.changePassword) {
    changePassword(currency, getWalletConfig(), logger, *dispatcher, *node, config.gateConfiguration.newContainerPassword);
  }
  else {
    runWalletService(currency, *node);
  }

  log(Logging::INFO) << "Stopping core rpc server...";
  rpcServer.stop();

  p2pNode.sendStopSignal();
  context.get();
  node->shutdown();
  core.deinit();
  p2pNode.deinit();
}

void PaymentGateService::runRpcProxy(Logging::LoggerRef& log) {
  log(Logging::INFO) << "Starting Payment Gate with remote node";
  CryptoNote::Currency currency = currencyBuilder.currency();

  std::unique_ptr<CryptoNote::INode> node(
    PaymentService::NodeFactory::createNode(
      config.remoteNodeConfig.m_daemon_host,
      config.remoteNodeConfig.m_daemon_port,
      "/", // TODO: need to add implementation after merge
      false));

  if (config.gateConfiguration.generateNewContainer) {
    generateNewWallet(currency, getWalletConfig(), logger, *dispatcher, *node);
  }
  else if (config.gateConfiguration.changePassword) {
    changePassword(currency, getWalletConfig(), logger, *dispatcher, *node, config.gateConfiguration.newContainerPassword);
  }
  else {
    runWalletService(currency, *node);
  }
}

void PaymentGateService::runWalletService(const CryptoNote::Currency& currency, CryptoNote::INode& node) {
  PaymentService::WalletConfiguration walletConfiguration{
    config.gateConfiguration.containerFile,
    config.gateConfiguration.containerPassword
  };

  std::unique_ptr<CryptoNote::WalletGreen> wallet(new CryptoNote::WalletGreen(*dispatcher, currency, node, logger));

  service = new PaymentService::WalletService(currency, *dispatcher, node, *wallet, *wallet, walletConfiguration, logger);
  std::unique_ptr<PaymentService::WalletService> serviceGuard(service);
  try {
    service->init();
  } catch (std::exception& e) {
    Logging::LoggerRef(logger, "run")(Logging::ERROR, Logging::BRIGHT_RED) << "Failed to init walletService reason: " << e.what();
    return;
  }

  if (config.gateConfiguration.printAddresses) {
    // print addresses and exit
    std::vector<std::string> addresses;
    service->getAddresses(addresses);
    for (const auto& address: addresses) {
      std::cout << "Address: " << address << std::endl;
    }
  } else {

    PaymentService::PaymentServiceJsonRpcServer rpcServer(*dispatcher, *stopEvent, *service, logger);

    bool rpc_run_ssl = false;
    std::string rpc_chain_file = "";
    std::string rpc_key_file = "";
    std::string rpc_dh_file = "";

    if (config.gateConfiguration.m_enable_ssl) {
        if (validateSertPath(config.coreConfig.configFolder,
            config.gateConfiguration.m_chain_file,
            config.gateConfiguration.m_key_file,
            config.gateConfiguration.m_dh_file,
            rpc_chain_file,
            rpc_key_file,
            rpc_dh_file)){
            rpcServer.setCerts(rpc_chain_file, rpc_key_file, rpc_dh_file);
            rpc_run_ssl = true;
        } else {
           Logging::LoggerRef(logger, "PaymentGateService")(Logging::ERROR, Logging::BRIGHT_RED) << "Start JSON-RPC SSL server was canceled because certificate file(s) could not be found" << std::endl;
        }
    }

    rpcServer.start(config.gateConfiguration.m_bind_address,
                    config.gateConfiguration.m_bind_port,
                    config.gateConfiguration.m_bind_port_ssl,
                    rpc_run_ssl,
                    config.gateConfiguration.m_rpcUser,
                    config.gateConfiguration.m_rpcPassword);

    Logging::LoggerRef(logger, "PaymentGateService")(Logging::INFO, Logging::BRIGHT_WHITE) << "JSON-RPC server stopped, stopping wallet service...";

    try {
      service->saveWallet();
    } catch (std::exception& ex) {
      Logging::LoggerRef(logger, "saveWallet")(Logging::WARNING, Logging::YELLOW) << "Couldn't save container: " << ex.what();
    }
  }
}
