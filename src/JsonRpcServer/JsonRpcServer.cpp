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

#include "Rpc/JsonRpc.h"
#include "Common/base64.hpp"
#include "Common/JsonValue.h"
#include "Common/StringTools.h"
#include "Serialization/JsonInputValueSerializer.h"
#include "Serialization/JsonOutputStreamSerializer.h"

namespace CryptoNote {

JsonRpcServer::JsonRpcServer(System::Dispatcher* sys, System::Event* stopEvent, Logging::ILogger& loggerGroup) :
  m_dispatcher(sys),
  stopEvent(stopEvent),
  logger(loggerGroup, "JsonRpcServer"),
  m_enable_ssl(false),
  m_httpServer(nullptr),
  m_httpsServer(nullptr)
{
}

JsonRpcServer::~JsonRpcServer() {
}

void JsonRpcServer::start(const std::string& bindAddress, uint16_t bindPort, uint16_t bindPortSSL, const std::string& user, const std::string& password) {
  if (!m_httpServer) {
    throw std::runtime_error("JsonRpcServer not initialized. Call init() first.");
  }

  logger(Logging::INFO) << "Starting JSON-RPC server on " << bindAddress << ":" << bindPort;

  // Start HTTP server
  m_httpServer->start(bindAddress, bindPort, user, password);

  // Start HTTPS server if SSL is enabled
  if (m_enable_ssl && m_httpsServer) {
    logger(Logging::INFO) << "Starting JSON-RPC HTTPS server on " << bindAddress << ":" << bindPortSSL;
    m_httpsServer->startSsl(bindAddress, bindPortSSL, m_chain_file, m_key_file, "", user, password);
  }

  logger(Logging::INFO) << "JSON-RPC server started successfully";
}

void JsonRpcServer::stop() {
  logger(Logging::INFO) << "Stopping JSON-RPC server...";

  if (m_httpServer) {
    m_httpServer->stop();
  }

  if (m_httpsServer) {
    m_httpsServer->stop();
  }

  logger(Logging::INFO) << "JSON-RPC server stopped";
}

void JsonRpcServer::init(const std::string& chain_file, const std::string& key_file, bool server_ssl_enable){
  m_chain_file = chain_file;
  m_key_file = key_file;
  m_enable_ssl = server_ssl_enable;

  // Create HTTP server
  assert(m_dispatcher != nullptr);
  m_httpServer = std::make_unique<CryptoNote::HttpServer>(*m_dispatcher, logger.getLogger());
  m_httpServer->setRequestHandler(
    std::bind(&JsonRpcServer::processRequest, this, std::placeholders::_1, std::placeholders::_2));

  // Create HTTPS server if SSL is enabled
  if (server_ssl_enable) {
    m_httpsServer = std::make_unique<CryptoNote::HttpServer>(*m_dispatcher, logger.getLogger());
    m_httpsServer->setRequestHandler(
      std::bind(&JsonRpcServer::processRequest, this, std::placeholders::_1, std::placeholders::_2));
  }
}

void JsonRpcServer::processRequest(const CryptoNote::HttpRequest& req, CryptoNote::HttpResponse& resp) {
  try {
    // CORS headers on every response
    resp.addHeader("Access-Control-Allow-Origin", "*");
    resp.addHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    resp.addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");

    // Handle CORS preflight
    if (req.getMethod() == "OPTIONS") {
      resp.setStatus(CryptoNote::HttpResponse::STATUS_200);
      return;
    }

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
        resp.addHeader("Content-Type", "application/json");

        return;
      }

      processJsonRpcRequest(jsonRpcRequest, jsonRpcResponse);

      std::ostringstream jsonOutputStream;
      jsonOutputStream << jsonRpcResponse;

      resp.setStatus(CryptoNote::HttpResponse::STATUS_200);
      resp.setBody(jsonOutputStream.str());
      resp.addHeader("Content-Type", "application/json");
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
  code = static_cast<int64_t>(CryptoNote::JsonRpc::errParseError); //Application specific error code

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
  code = static_cast<int64_t>(CryptoNote::JsonRpc::errMethodNotFound); //ambigous declaration of JsonValue::operator= (between int and JsonValue)

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
  code = static_cast<int64_t>(CryptoNote::JsonRpc::errParseError); //ambigous declaration of JsonValue::operator= (between int and JsonValue)

  JsonValue message = "Parse error";

  error.insert("code", code);
  error.insert("message", message);

  resp.insert("error", error);
}

}
