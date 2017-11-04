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

#include <string>
#include <stdexcept>
#include <cstdint>

#include <boost/program_options.hpp>

namespace PaymentService {

class ConfigurationError : public std::runtime_error {
public:
  ConfigurationError(const char* desc) : std::runtime_error(desc) {}
};

struct Configuration {
  Configuration();

  void init(const boost::program_options::variables_map& options);
  static void initOptions(boost::program_options::options_description& desc);

  std::string bindAddress;
  uint16_t bindPort;

  std::string containerFile;
  std::string containerPassword;
  std::string logFile;
  std::string serverRoot;

  bool generateNewContainer;
  bool daemonize;
  bool registerService;
  bool unregisterService;
  bool testnet;
  bool printAddresses;

  size_t logLevel;
};

} //namespace PaymentService
