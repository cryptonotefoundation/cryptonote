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

#include "ConfigurationManager.h"

#include <fstream>
#include <boost/program_options.hpp>

#include "Common/CommandLine.h"
#include "Common/Util.h"
#include "version.h"

namespace PaymentService {

namespace po = boost::program_options;

ConfigurationManager::ConfigurationManager() {
  startInprocess = false;
}

bool ConfigurationManager::init(int argc, char** argv) {
  po::options_description cmdGeneralOptions("Common Options");

  cmdGeneralOptions.add_options()
      ("config,c", po::value<std::string>(), "configuration file");

  po::options_description confGeneralOptions;
  confGeneralOptions.add(cmdGeneralOptions).add_options()
      ("testnet", po::bool_switch(), "")
      ("local", po::bool_switch(), "");

  cmdGeneralOptions.add_options()
      ("help,h", "produce this help message and exit")
      ("local", po::bool_switch(), "start with local node (remote is default)")
      ("testnet", po::bool_switch(), "testnet mode")
      ("version", "Output version information");

  command_line::add_arg(cmdGeneralOptions, command_line::arg_data_dir, Tools::getDefaultDataDirectory());
  command_line::add_arg(confGeneralOptions, command_line::arg_data_dir, Tools::getDefaultDataDirectory());

  Configuration::initOptions(cmdGeneralOptions);
  Configuration::initOptions(confGeneralOptions);

  po::options_description netNodeOptions("Local Node Options");
  CryptoNote::NetNodeConfig::initOptions(netNodeOptions);
  
  po::options_description remoteNodeOptions("Remote Node Options");
  RpcNodeConfiguration::initOptions(remoteNodeOptions);

  po::options_description cmdOptionsDesc;
  cmdOptionsDesc.add(cmdGeneralOptions).add(remoteNodeOptions).add(netNodeOptions);

  po::options_description confOptionsDesc;
  confOptionsDesc.add(confGeneralOptions).add(remoteNodeOptions).add(netNodeOptions);

  po::variables_map cmdOptions;
  po::store(po::parse_command_line(argc, argv, cmdOptionsDesc), cmdOptions);
  po::notify(cmdOptions);

  if (cmdOptions.count("help")) {
    std::cout << cmdOptionsDesc << std::endl;
    return false;
  }

  if (cmdOptions.count("version") > 0) {
    std::cout << "walletd v" << PROJECT_VERSION_LONG;
    return false;
  }

  if (cmdOptions.count("config")) {
    std::ifstream confStream(cmdOptions["config"].as<std::string>(), std::ifstream::in);
    if (!confStream.good()) {
      throw ConfigurationError("Cannot open configuration file");
    }

    po::variables_map confOptions;
    po::store(po::parse_config_file(confStream, confOptionsDesc), confOptions);
    po::notify(confOptions);

    gateConfiguration.init(confOptions);
    netNodeConfig.init(confOptions);
    remoteNodeConfig.init(confOptions);

    netNodeConfig.setTestnet(confOptions["testnet"].as<bool>());
    startInprocess = confOptions["local"].as<bool>();
  }

  //command line options should override options from config file
  gateConfiguration.init(cmdOptions);
  netNodeConfig.init(cmdOptions);
  remoteNodeConfig.init(cmdOptions);
  dataDir = command_line::get_arg(cmdOptions, command_line::arg_data_dir);

  if (cmdOptions["testnet"].as<bool>()) {
    netNodeConfig.setTestnet(true);
  }

  if (cmdOptions["local"].as<bool>()) {
    startInprocess = true;
  }

  return true;
}

} //namespace PaymentService
