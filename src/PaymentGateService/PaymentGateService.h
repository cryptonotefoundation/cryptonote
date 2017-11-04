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

#pragma once

#include "ConfigurationManager.h"
#include "PaymentServiceConfiguration.h"

#include "Logging/ConsoleLogger.h"
#include "Logging/LoggerGroup.h"
#include "Logging/StreamLogger.h"

#include "PaymentGate/NodeFactory.h"
#include "PaymentGate/WalletService.h"

class PaymentGateService {
public:
  PaymentGateService();

  bool init(int argc, char** argv);

  const PaymentService::ConfigurationManager& getConfig() const { return config; }
  PaymentService::WalletConfiguration getWalletConfig() const;
  const CryptoNote::Currency getCurrency();

  void run();
  void stop();
  
  Logging::ILogger& getLogger() { return logger; }

private:

  void runInProcess(Logging::LoggerRef& log);
  void runRpcProxy(Logging::LoggerRef& log);

  void runWalletService(const CryptoNote::Currency& currency, CryptoNote::INode& node);

  System::Dispatcher* dispatcher;
  System::Event* stopEvent;
  PaymentService::ConfigurationManager config;
  PaymentService::WalletService* service;
  CryptoNote::CurrencyBuilder currencyBuilder;
  
  Logging::LoggerGroup logger;
  std::ofstream fileStream;
  Logging::StreamLogger fileLogger;
  Logging::ConsoleLogger consoleLogger;
};
