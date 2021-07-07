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

#pragma once

#include <boost/program_options.hpp>

namespace CryptoNote {

class RpcServerConfig {

public:
  RpcServerConfig();

  static void initOptions(boost::program_options::options_description& desc);
  void init(const boost::program_options::variables_map& options);

  bool isEnabledSSL() const;
  uint16_t getBindPort() const;
  uint16_t getBindPortSSL() const;
  std::string getBindIP() const;
  std::string getBindAddress() const;
  std::string getBindAddressSSL() const;
  std::string getDhFile() const;
  std::string getChainFile() const;
  std::string getKeyFile() const;

//private:
  bool        restrictedRPC;
  bool        enableSSL;
  uint16_t    bindPort;
  uint16_t    bindPortSSL;
  std::string bindIp;
  std::string dhFile;
  std::string chainFile;
  std::string keyFile;
  std::string enableCors;
  std::string contactInfo;
  std::string nodeFeeAddress;
  std::string nodeFeeAmountStr;
  std::string nodeFeeViewKey;
};

}
