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

#include "MiningConfig.h"

#include <iostream>
#include <thread>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "CryptoNoteConfig.h"
#include "Logging/ILogger.h"

namespace po = boost::program_options;

namespace CryptoNote {

namespace {

const size_t DEFAULT_SCANT_PERIOD = 30;
const char* DEFAULT_DAEMON_HOST = "127.0.0.1";
const size_t CONCURRENCY_LEVEL = std::thread::hardware_concurrency();

po::options_description cmdOptions;

void parseDaemonAddress(const std::string& daemonAddress, std::string& daemonHost, uint16_t& daemonPort) {
  std::vector<std::string> splittedAddress;
  boost::algorithm::split(splittedAddress, daemonAddress, boost::algorithm::is_any_of(":"));

  if (splittedAddress.size() != 2) {
    throw std::runtime_error("Wrong daemon address format");
  }

  if (splittedAddress[0].empty() || splittedAddress[1].empty()) {
    throw std::runtime_error("Wrong daemon address format");
  }

  daemonHost = splittedAddress[0];

  try {
    daemonPort = boost::lexical_cast<uint16_t>(splittedAddress[1]);
  } catch (std::exception&) {
    throw std::runtime_error("Wrong daemon address format");
  }
}

}

MiningConfig::MiningConfig(): help(false) {
  cmdOptions.add_options()
      ("help,h", "produce this help message and exit")
      ("address", po::value<std::string>(), "Valid cryptonote miner's address")
      ("daemon-host", po::value<std::string>()->default_value(DEFAULT_DAEMON_HOST), "Daemon host")
      ("daemon-rpc-port", po::value<uint16_t>()->default_value(static_cast<uint16_t>(RPC_DEFAULT_PORT)), "Daemon's RPC port")
      ("daemon-address", po::value<std::string>(), "Daemon host:port. If you use this option you must not use --daemon-host and --daemon-port options")
      ("threads", po::value<size_t>()->default_value(CONCURRENCY_LEVEL), "Mining threads count. Must not be greater than you concurrency level. Default value is your hardware concurrency level")
      ("scan-time", po::value<size_t>()->default_value(DEFAULT_SCANT_PERIOD), "Blockchain polling interval (seconds). How often miner will check blockchain for updates")
      ("log-level", po::value<int>()->default_value(1), "Log level. Must be 0..5")
      ("limit", po::value<size_t>()->default_value(0), "Mine exact quantity of blocks. 0 means no limit")
      ("first-block-timestamp", po::value<uint64_t>()->default_value(0), "Set timestamp to the first mined block. 0 means leave timestamp unchanged")
      ("block-timestamp-interval", po::value<int64_t>()->default_value(0), "Timestamp step for each subsequent block. May be set only if --first-block-timestamp has been set."
                                                         " If not set blocks' timestamps remain unchanged");
}

void MiningConfig::parse(int argc, char** argv) {
  po::variables_map options;
  po::store(po::parse_command_line(argc, argv, cmdOptions), options);
  po::notify(options);

  if (options.count("help") != 0) {
    help = true;
    return;
  }

  if (options.count("address") == 0) {
    throw std::runtime_error("Specify --address option");
  }

  miningAddress = options["address"].as<std::string>();

  if (!options["daemon-address"].empty()) {
    if (!options["daemon-host"].defaulted() || !options["daemon-rpc-port"].defaulted()) {
      throw std::runtime_error("Either --daemon-host or --daemon-rpc-port is already specified. You must not specify --daemon-address");
    }

    parseDaemonAddress(options["daemon-address"].as<std::string>(), daemonHost, daemonPort);
  } else {
    daemonHost = options["daemon-host"].as<std::string>();
    daemonPort = options["daemon-rpc-port"].as<uint16_t>();
  }

  threadCount = options["threads"].as<size_t>();
  if (threadCount == 0 || threadCount > CONCURRENCY_LEVEL) {
    throw std::runtime_error("--threads option must be 1.." + std::to_string(CONCURRENCY_LEVEL));
  }

  scanPeriod = options["scan-time"].as<size_t>();
  if (scanPeriod == 0) {
    throw std::runtime_error("--scan-time must not be zero");
  }

  logLevel = static_cast<uint8_t>(options["log-level"].as<int>());
  if (logLevel > static_cast<uint8_t>(Logging::TRACE)) {
    throw std::runtime_error("--log-level value is too big");
  }

  blocksLimit = options["limit"].as<size_t>();

  if (!options["block-timestamp-interval"].defaulted() && options["first-block-timestamp"].defaulted()) {
    throw std::runtime_error("If you specify --block-timestamp-interval you must specify --first-block-timestamp either");
  }

  firstBlockTimestamp = options["first-block-timestamp"].as<uint64_t>();
  blockTimestampInterval = options["block-timestamp-interval"].as<int64_t>();
}

void MiningConfig::printHelp() {
  std::cout << cmdOptions << std::endl;
}

}
