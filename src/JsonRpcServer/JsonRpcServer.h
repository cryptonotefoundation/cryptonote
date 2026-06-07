// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright(c) 2014 - 2017 XDN - project developers
// Copyright(c) 2018 - 2026 The Karbo developers
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

#include "System/Dispatcher.h"
#include "System/Event.h"
#include "System/RemoteContext.h"
#include "Logging/ILogger.h"
#include "Logging/LoggerRef.h"
#include <HTTP/HttpServer.h>
#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>


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

class JsonRpcServer {
public:
  JsonRpcServer(System::Dispatcher* sys, System::Event* stopEvent, Logging::ILogger& loggerGroup);
  JsonRpcServer(const JsonRpcServer&) = delete;

  ~JsonRpcServer();

  void init(const std::string& chain_file, const std::string& key_file, bool server_ssl_enable = false);
  void start(const std::string& bindAddress, uint16_t bindPort, uint16_t bindPortSSL, const std::string& user = "", const std::string& password = "");
  void stop();

protected:
  static void makeErrorResponse(const std::error_code& ec, Common::JsonValue& resp);
  static void makeMethodNotFoundResponse(Common::JsonValue& resp);
  static void makeGenericErrorReponse(Common::JsonValue& resp, const char* what, int errorCode = -32001);
  static void fillJsonResponse(const Common::JsonValue& v, Common::JsonValue& resp);
  static void prepareJsonResponse(const Common::JsonValue& req, Common::JsonValue& resp);
  static void makeJsonParsingErrorResponse(Common::JsonValue& resp);

  virtual void processJsonRpcRequest(const Common::JsonValue& req, Common::JsonValue& resp) = 0;

private:
  void processRequest(const CryptoNote::HttpRequest& req, CryptoNote::HttpResponse& resp);

  System::Dispatcher* m_dispatcher;
  System::Event* stopEvent;
  Logging::LoggerRef logger;
 
  std::unique_ptr<CryptoNote::HttpServer> m_httpServer;
  std::unique_ptr<CryptoNote::HttpServer> m_httpsServer;

  std::string m_chain_file;
  std::string m_key_file;

  bool m_enable_ssl;
};

} //namespace CryptoNote
