// Copyright (c) 2019 Helder Garcia <helder.garcia@gmail.com>
// Copyright (c) 2019, The Karbo developers

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <chrono>
#include <thread>
#include <vector>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include "Common/CommandLine.h"
#include "Common/JsonValue.h"
#include "Common/StringTools.h"
#include "Logging/LoggerRef.h"
#include "Logging/ConsoleLogger.h"
#include "Rpc/HttpClient.h"
#include "Rpc/JsonRpc.h"
#include "PaymentGate/PaymentServiceJsonRpcMessages.h"
#include "Serialization/ISerializer.h"
#include <System/Dispatcher.h>

namespace po = boost::program_options;
using Common::JsonValue;
using namespace Logging;
using namespace CryptoNote;
using namespace PaymentService;

#ifndef ENDL
#define ENDL std::endl
#endif

const uint64_t DEFAULT_THRESHOLD = UINT64_C(100000000000000);

namespace {
  const command_line::arg_descriptor<std::string> arg_address   = {"address", "Address of the wallet to optimize inputs. If not provided, all addresses will be checked and, if applicable, optimized using polling interval between each interaction. Default: All", "", true};
  const command_line::arg_descriptor<std::string> arg_ip        = {"walletd-ip", "IP address of walletd. Default: 127.0.0.1", "127.0.0.1"};
  const command_line::arg_descriptor<uint16_t>    arg_rpc_port  = {"walletd-port", "RPC port of walletd. Default: 8070", 8070};
  const command_line::arg_descriptor<std::string> arg_user      = {"walletd-user", "RPC user. Default: none", "", true};
  const command_line::arg_descriptor<std::string> arg_pass      = {"walletd-password", "RPC password. Default: none", "", true};
  const command_line::arg_descriptor<uint16_t>    arg_interval  = {"interval", "polling interval in seconds. Default: 5. Minimum: 1. Maximum: 120.", 5, true};
  const command_line::arg_descriptor<uint16_t>    arg_duration  = {"duration", "maximum execution time, in minutes. Default: 0 (unlimited)", 0, true};
  const command_line::arg_descriptor<uint64_t>    arg_threshold = {"threshold", "Only outputs lesser than the threshold value will be included into optimization. Default: 100000000000000 (do not use decimal point)", DEFAULT_THRESHOLD, true};
  const command_line::arg_descriptor<uint16_t>    arg_anonimity = {"anonymity", "Privacy level. Higher values give more privacy but bigger transactions. Default: 6", 6, true};
  const command_line::arg_descriptor<bool>        arg_preview   = {"preview", "print on screen what it would be doing, but not really doing it", false, true};
  Logging::ConsoleLogger log;
  Logging::LoggerRef logger(log, "optimizer");
  System::Dispatcher dispatcher;
}

bool validAddress(po::variables_map& vm, const std::string& address) {
  PaymentService::GetBalance::Request req;
  PaymentService::GetBalance::Response res;

  req.address = address;

  try {
    HttpClient httpClient(dispatcher, command_line::get_arg(vm, arg_ip), command_line::get_arg(vm, arg_rpc_port), false);
    if (command_line::has_arg(vm, arg_user) && command_line::has_arg(vm, arg_pass)) {
      JsonRpc::invokeJsonRpcCommand(httpClient, "getBalance", req, res, command_line::get_arg(vm, arg_user), command_line::get_arg(vm, arg_pass));
    }
    else {
      JsonRpc::invokeJsonRpcCommand(httpClient, "getBalance", req, res);
    }
  }
  catch (const std::exception& e) {
    logger((Logging::Level) ERROR, RED) << "Failed to connect to walletd: " << e.what() << ENDL;
    return false;
  }

  return true;
}

std::vector<std::string> getWalletsAddresses(po::variables_map& vm) {
  std::vector<std::string> containerAddresses;
  if (command_line::has_arg(vm, arg_address)) {
    std::string address = command_line::get_arg(vm, arg_address);
    if (validAddress(vm, address)) {
      containerAddresses.push_back(command_line::get_arg(vm, arg_address));
    }
  } else {
    PaymentService::GetAddresses::Request req;
    PaymentService::GetAddresses::Response res;

    try {
      HttpClient httpClient(dispatcher, command_line::get_arg(vm, arg_ip), command_line::get_arg(vm, arg_rpc_port), false);
      if (command_line::has_arg(vm, arg_user) && command_line::has_arg(vm, arg_pass)) {
        JsonRpc::invokeJsonRpcCommand(httpClient, "getAddresses", req, res, command_line::get_arg(vm, arg_user), command_line::get_arg(vm, arg_pass));
      }
      else {
        JsonRpc::invokeJsonRpcCommand(httpClient, "getAddresses", req, res);
      }
    }
    catch (const std::exception& e) {
      logger((Logging::Level) ERROR, RED) << "Failed to connect to walletd: " << e.what() << ENDL;
    }
    
    containerAddresses = res.addresses;
  }
  return containerAddresses;
}

bool isWalletEligible(po::variables_map& vm, std::string address) {
  uint64_t threshold = DEFAULT_THRESHOLD;

  if (command_line::has_arg(vm, arg_threshold)) {
    threshold = command_line::get_arg(vm, arg_threshold);
  }

  PaymentService::EstimateFusion::Request req;
  PaymentService::EstimateFusion::Response res;

  req.threshold = threshold;
  req.addresses.push_back(address);

  try {
    HttpClient httpClient(dispatcher, command_line::get_arg(vm, arg_ip), command_line::get_arg(vm, arg_rpc_port), false);
    if (command_line::has_arg(vm, arg_user) && command_line::has_arg(vm, arg_pass)) {
      JsonRpc::invokeJsonRpcCommand(httpClient, "estimateFusion", req, res, command_line::get_arg(vm, arg_user), command_line::get_arg(vm, arg_pass));
    }
    else {
      JsonRpc::invokeJsonRpcCommand(httpClient, "estimateFusion", req, res);
    }
  }
  catch (const std::exception& e) {
    logger((Logging::Level) ERROR, RED) << "Failed to connect to walletd: " << e.what() << ENDL;
    return false;
  }

  uint32_t fusionReadyCount = res.fusionReadyCount;
  if (fusionReadyCount > 0) {
    return true;
  }

  return false;
}

bool optimizeWallet(po::variables_map& vm, std::string address) {
  uint64_t threshold = DEFAULT_THRESHOLD;
  uint16_t anonymity = 6;

  if (command_line::has_arg(vm, arg_threshold)) {
    threshold = command_line::get_arg(vm, arg_threshold);
  }
  if (command_line::has_arg(vm, arg_anonimity)) {
    anonymity = command_line::get_arg(vm, arg_anonimity);
  }

  PaymentService::SendFusionTransaction::Request req;
  PaymentService::SendFusionTransaction::Response res;

  req.threshold = threshold;
  req.anonymity = anonymity;
  req.addresses.push_back(address);
  req.destinationAddress = address;

  try {
    logger((Logging::Level) INFO, GREEN) << "Optimizing wallet  : " << address;
    HttpClient httpClient(dispatcher, command_line::get_arg(vm, arg_ip), command_line::get_arg(vm, arg_rpc_port), false);
    if (command_line::has_arg(vm, arg_user) && command_line::has_arg(vm, arg_pass)) {
      JsonRpc::invokeJsonRpcCommand(httpClient, "sendFusionTransaction", req, res, command_line::get_arg(vm, arg_user), command_line::get_arg(vm, arg_pass));
    }
    else {
      JsonRpc::invokeJsonRpcCommand(httpClient, "sendFusionTransaction", req, res);
    }
  }
  catch (const std::exception& e) {
    logger((Logging::Level) ERROR, RED) << "Failed in wallet: " << address << " due to: " << e.what() << ENDL;
    return false;
  }

  logger(INFO, GREEN) << "Success. Tx hash   : " << res.transactionHash << ENDL;
  return true;
}

void processWallets(po::variables_map& vm, std::vector<std::string>& containerAddresses, int& optimized, int& notOptimized, const std::chrono::time_point<std::chrono::steady_clock>& start) {
  uint16_t timeInterval = 5;
  int32_t maxDuration = 0;
  if (command_line::has_arg(vm, arg_interval)) {
    timeInterval = command_line::get_arg(vm, arg_interval);
    if (timeInterval > 120) timeInterval = 120;
    if (timeInterval < 1) timeInterval = 1;
  }
  if (command_line::has_arg(vm, arg_duration)) {
    maxDuration = command_line::get_arg(vm, arg_duration);
  }
  bool previewMode = false;
  uint32_t count = 0;
  uint8_t steps = 0;
  size_t total = containerAddresses.size();
  if (total > 10000) {
    steps = 100;
  } else {
    steps = 10;
  }
  if (command_line::has_arg(vm, arg_preview)) {
    previewMode = true;
  }
  for (const auto& address : containerAddresses) {
    if (isWalletEligible(vm, address)) {
      if (previewMode) {
        logger(INFO, GREEN) << "Optimizable wallet   : " << address << ENDL;
        optimized++;
      } else {
        if (optimized > 0) {
          logger(INFO, GREEN) << "Sleeping for " << timeInterval << " seconds." << ENDL;
          std::this_thread::sleep_for(std::chrono::seconds(timeInterval));
        }
        if (optimizeWallet(vm, address)) {
          optimized++;
        } else {
          notOptimized++;
        }
      }
    } else {
      //logger(INFO, GREEN) << "Wallet not eligible: " << address << ENDL;
      notOptimized++;
    }
    count++;
    if (count % steps == 0) {
      logger(INFO, GREEN) << "Scanned " << count << " wallets." << ENDL;
    }
    if (maxDuration > 0) {
      auto dur = std::chrono::steady_clock::now() - start;
      if(std::chrono::duration_cast<std::chrono::minutes>(dur).count() >= maxDuration) {
        logger(INFO, GREEN) << "Maximum duration time reached." << ENDL;
        break;
      }
    }
  }
  return;
}

bool canConnect(po::variables_map& vm) {

  PaymentService::GetStatus::Request req;
  PaymentService::GetStatus::Response res;

  try {
    HttpClient httpClient(dispatcher, command_line::get_arg(vm, arg_ip), command_line::get_arg(vm, arg_rpc_port), false);
    if (command_line::has_arg(vm, arg_user) && command_line::has_arg(vm, arg_pass)) {
      JsonRpc::invokeJsonRpcCommand(httpClient, "getStatus", req, res, command_line::get_arg(vm, arg_user), command_line::get_arg(vm, arg_pass));
    }
    else {
      JsonRpc::invokeJsonRpcCommand(httpClient, "getStatus", req, res);
    }
  }
  catch (const std::exception& e) {
    logger((Logging::Level) ERROR, RED) << "Failed to connect to walletd: " << e.what();
    return false;
  }

  return true;
}

bool run_optimizer(po::variables_map& vm) {
  if (canConnect(vm)) {
    std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
    std::vector<std::string> addresses = getWalletsAddresses(vm);
    if (command_line::has_arg(vm, arg_address)) {
      logger(INFO, YELLOW) << "Starting optimizing." << ENDL;
    } else {
      logger(INFO, YELLOW) << "Starting optimizing. There are " << addresses.size() << " wallets in this container." << ENDL;
    }
    int optimized = 0;
    int notOptimized = 0;
    processWallets(vm, addresses, optimized, notOptimized, start);
    int processed = optimized + notOptimized;
    auto dur = std::chrono::steady_clock::now() - start;
    logger(INFO, YELLOW) << "Optimizing finished." << ENDL;
    if (!(command_line::has_arg(vm, arg_address))) {
      std::cout   << "============== SUMMARY =============" << ENDL;
      if (command_line::has_arg(vm, arg_preview)) {
        std::cout << "   Optimizable wallets     : " << optimized << ENDL;
        std::cout << "   Non optimizable wallets : " << notOptimized << ENDL;
      } else {
        std::cout << "   Wallets optimized       : " << optimized << ENDL;
        std::cout << "   Wallets not optimized   : " << notOptimized << ENDL;
      }
      std::cout   << "   Scanned wallets         : " << processed << ENDL;
      std::cout   << "   Total of wallets found  : " << addresses.size() << ENDL;
      std::cout   << "   Processing time (sec)   : " << std::chrono::duration_cast<std::chrono::seconds>(dur).count() << ENDL;
      std::cout   << "====================================" << ENDL;
    }
    return true;
  } else {
    return false;
  }
}

int main(int argc, char *argv[]) {
  po::options_description desc_general("General options");
  command_line::add_arg(desc_general, command_line::arg_help);
  po::options_description desc_params("Command options");
  command_line::add_arg(desc_params, arg_address);
  command_line::add_arg(desc_params, arg_ip);
  command_line::add_arg(desc_params, arg_rpc_port);
  command_line::add_arg(desc_params, arg_user);
  command_line::add_arg(desc_params, arg_pass);
  command_line::add_arg(desc_params, arg_interval);
  command_line::add_arg(desc_params, arg_duration);
  command_line::add_arg(desc_params, arg_threshold);
  command_line::add_arg(desc_params, arg_anonimity);
  command_line::add_arg(desc_params, arg_preview);

  po::options_description desc_all;
  desc_all.add(desc_general).add(desc_params);

  po::variables_map vm;
  bool r = command_line::handle_error_helper(desc_all, [&]() {
    po::store(command_line::parse_command_line(argc, argv, desc_general, true), vm);
    if (command_line::get_arg(vm, command_line::arg_help)) {
      std::cout << "Optimizer Copyright (C) 2019  Helder Garcia <helder.garcia@gmail.com>" << ENDL;
      std::cout << "This program comes with ABSOLUTELY NO WARRANTY;" << ENDL;
      std::cout << "This is free software, and you are welcome to redistribute it" << ENDL;
      std::cout << "under certain conditions see <https://www.gnu.org/licenses/>." << ENDL;
      std::cout << desc_all << ENDL;
      return false;
    }

    po::store(command_line::parse_command_line(argc, argv, desc_params, false), vm);
    po::notify(vm);

    return true;
  });

  if (!r) return 1;

  return run_optimizer(vm) ? 0 : 1;
}
