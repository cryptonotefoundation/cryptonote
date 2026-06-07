
// Copyright (c) 2017-2019, Karbo developers
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
// along with Karbo. If not, see <http://www.gnu.org/licenses/>.


#include <stdint.h>
#include <stdio.h>
#include <string>
#include <boost/regex.hpp>
#include "CryptoNoteConfig.h"


namespace Common {

const uint16_t HTTP_PORT = 80;
const uint16_t HTTPS_PORT = 443;
const std::string RPC_PATH = "/";

bool parseUrlAddress(const std::string& url, std::string& host, uint16_t& port, std::string& path, bool& ssl) {
  bool res = true;

  host.clear();
  path.clear();
  port = 0;
  ssl = false;

  boost::regex uri_exp("^(https://|http://|)(([a-z|A-Z|0-9]|[a-z|A-Z|0-9]-[a-z|A-Z|0-9]|[a-z|A-Z|0-9]-[a-z|A-Z|0-9]\\.|[a-z|A-Z|0-9]\\.)+)(:[0-9]{1,5}|)(/([\\w|-]+/)+|/|)$");
  boost::cmatch reg_res;
  if (boost::regex_match(url.c_str(), reg_res, uri_exp)) {
    if (reg_res.length(4) > 0) {
      int port_src = 0;
      if (sscanf(reg_res.str(4).c_str() + 1, "%d", &port_src) == 1) {
        if (port_src > 0 && port_src <= 0xFFFF) port = (uint16_t) port_src;
      }
    } else {
      if (strcmp(reg_res.str(1).c_str(), "http://") == 0) port = HTTP_PORT;
      else if (strcmp(reg_res.str(1).c_str(), "https://") == 0) port = HTTPS_PORT;
      else port = CryptoNote::RPC_DEFAULT_PORT;
    }
    if (port != 0) {
      if (strcmp(reg_res.str(1).c_str(), "https://") == 0) ssl = true;
      host.assign(reg_res[2].first, reg_res[2].second);
      path.assign(reg_res[5].first, reg_res[5].second);
      if (path.empty()) path = RPC_PATH;
    } else {
      res = false;
    }
  } else {
    res = false;
  }

  return res;
}

}

