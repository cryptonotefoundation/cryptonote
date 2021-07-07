// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright(c) 2014 - 2017 XDN - project developers
// Copyright(c) 2018 The Karbo developers
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

#include <system_error>

#include <System/Dispatcher.h>
#include <System/Event.h>
#include "Logging/ILogger.h"
#include "Logging/LoggerRef.h"
#include "Rpc/HttpServer.h"


namespace CryptoNote {
class HttpResponse;
class HttpRequest;
}

namespace Common {
class JsonValue;
}

namespace System {
class TcpConnection;
}

namespace CryptoNote {

class JsonRpcServer : HttpServer {
public:
  JsonRpcServer(System::Dispatcher& sys, System::Event& stopEvent, Logging::ILogger& loggerGroup);
  JsonRpcServer(const JsonRpcServer&) = delete;

  void setCerts(const std::string& chain_file, const std::string& key_file, const std::string& dh_file);

  void start(const std::string& bindAddress, uint16_t bindPort, uint16_t bindPortSSL,
             bool server_ssl_enable, const std::string& m_rpcUser, const std::string& m_rpcPassword);

protected:
  static void makeErrorResponse(const std::error_code& ec, Common::JsonValue& resp);
  static void makeMethodNotFoundResponse(Common::JsonValue& resp);
  static void makeGenericErrorReponse(Common::JsonValue& resp, const char* what, int errorCode = -32001);
  static void fillJsonResponse(const Common::JsonValue& v, Common::JsonValue& resp);
  static void prepareJsonResponse(const Common::JsonValue& req, Common::JsonValue& resp);
  static void makeJsonParsingErrorResponse(Common::JsonValue& resp);

  virtual void processJsonRpcRequest(const Common::JsonValue& req, Common::JsonValue& resp) = 0;

private:
  // HttpServer
  virtual void processRequest(const CryptoNote::HttpRequest& request, CryptoNote::HttpResponse& response) override;

  System::Dispatcher& system;
  System::Event& stopEvent;
  Logging::LoggerRef logger;
};

} //namespace CryptoNote
