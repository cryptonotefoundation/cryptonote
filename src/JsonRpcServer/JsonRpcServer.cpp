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

#include "JsonRpcServer.h"

#include <fstream>
#include <future>
#include <system_error>
#include <memory>
#include <sstream>
#include "HTTP/HttpParserErrorCodes.h"

#include <System/TcpConnection.h>
#include <System/TcpListener.h>
#include <System/TcpStream.h>
#include <System/Ipv4Address.h>
#include "HTTP/HttpParser.h"
#include "HTTP/HttpResponse.h"

#include "Common/JsonValue.h"
#include "Serialization/JsonInputValueSerializer.h"
#include "Serialization/JsonOutputStreamSerializer.h"

namespace CryptoNote {

JsonRpcServer::JsonRpcServer(System::Dispatcher& sys, System::Event& stopEvent, Logging::ILogger& loggerGroup) :
  HttpServer(sys, loggerGroup), 
  system(sys),
  stopEvent(stopEvent),
  logger(loggerGroup, "JsonRpcServer")
{
}

void JsonRpcServer::start(const std::string& bindAddress, uint16_t bindPort) {
  HttpServer::start(bindAddress, bindPort);
  stopEvent.wait();
  HttpServer::stop();
}

void JsonRpcServer::processRequest(const CryptoNote::HttpRequest& req, CryptoNote::HttpResponse& resp) {
  try {
    logger(Logging::TRACE) << "HTTP request came: \n" << req;

    if (req.getUrl() == "/json_rpc") {
      std::istringstream jsonInputStream(req.getBody());
      Common::JsonValue jsonRpcRequest;
      Common::JsonValue jsonRpcResponse(Common::JsonValue::OBJECT);

      try {
        jsonInputStream >> jsonRpcRequest;
      } catch (std::runtime_error&) {
        logger(Logging::DEBUGGING) << "Couldn't parse request: \"" << req.getBody() << "\"";
        makeJsonParsingErrorResponse(jsonRpcResponse);
        resp.setStatus(CryptoNote::HttpResponse::STATUS_200);
        resp.setBody(jsonRpcResponse.toString());
        return;
      }

      processJsonRpcRequest(jsonRpcRequest, jsonRpcResponse);

      std::ostringstream jsonOutputStream;
      jsonOutputStream << jsonRpcResponse;

      resp.setStatus(CryptoNote::HttpResponse::STATUS_200);
      resp.setBody(jsonOutputStream.str());

    } else {
      logger(Logging::WARNING) << "Requested url \"" << req.getUrl() << "\" is not found";
      resp.setStatus(CryptoNote::HttpResponse::STATUS_404);
      return;
    }
  } catch (std::exception& e) {
    logger(Logging::WARNING) << "Error while processing http request: " << e.what();
    resp.setStatus(CryptoNote::HttpResponse::STATUS_500);
  }
}

void JsonRpcServer::prepareJsonResponse(const Common::JsonValue& req, Common::JsonValue& resp) {
  using Common::JsonValue;

  if (req.contains("id")) {
    resp.insert("id", req("id"));
  }
  
  resp.insert("jsonrpc", "2.0");
}

void JsonRpcServer::makeErrorResponse(const std::error_code& ec, Common::JsonValue& resp) {
  using Common::JsonValue;

  JsonValue error(JsonValue::OBJECT);

  JsonValue code;
  code = static_cast<int64_t>(-32000); //Application specific error code

  JsonValue message;
  message = ec.message();

  JsonValue data(JsonValue::OBJECT);
  JsonValue appCode;
  appCode = static_cast<int64_t>(ec.value());
  data.insert("application_code", appCode);

  error.insert("code", code);
  error.insert("message", message);
  error.insert("data", data);

  resp.insert("error", error);
}

void JsonRpcServer::makeGenericErrorReponse(Common::JsonValue& resp, const char* what, int errorCode) {
  using Common::JsonValue;

  JsonValue error(JsonValue::OBJECT);

  JsonValue code;
  code = static_cast<int64_t>(errorCode);

  std::string msg;
  if (what) {
    msg = what;
  } else {
    msg = "Unknown application error";
  }

  JsonValue message;
  message = msg;

  error.insert("code", code);
  error.insert("message", message);

  resp.insert("error", error);

}

void JsonRpcServer::makeMethodNotFoundResponse(Common::JsonValue& resp) {
  using Common::JsonValue;

  JsonValue error(JsonValue::OBJECT);

  JsonValue code;
  code = static_cast<int64_t>(-32601); //ambigous declaration of JsonValue::operator= (between int and JsonValue)

  JsonValue message;
  message = "Method not found";

  error.insert("code", code);
  error.insert("message", message);

  resp.insert("error", error);
}

void JsonRpcServer::fillJsonResponse(const Common::JsonValue& v, Common::JsonValue& resp) {
  resp.insert("result", v);
}

void JsonRpcServer::makeJsonParsingErrorResponse(Common::JsonValue& resp) {
  using Common::JsonValue;

  resp = JsonValue(JsonValue::OBJECT);
  resp.insert("jsonrpc", "2.0");
  resp.insert("id", nullptr);

  JsonValue error(JsonValue::OBJECT);
  JsonValue code;
  code = static_cast<int64_t>(-32700); //ambigous declaration of JsonValue::operator= (between int and JsonValue)

  JsonValue message = "Parse error";

  error.insert("code", code);
  error.insert("message", message);

  resp.insert("error", error);
}

}
