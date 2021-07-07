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

#include <memory>

#include <Common/base64.hpp>
#include <Common/StringTools.h>
#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>
#include <System/TcpConnection.h>
#include <System/TcpStream.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
#include "JsonRpc.h"

#include "Serialization/SerializationTools.h"

using boost::asio::ip::tcp;


namespace CryptoNote {

class ConnectException : public std::runtime_error  {
public:
  ConnectException(const std::string& whatArg);
};

class HttpClient {
public:

  HttpClient(System::Dispatcher& dispatcher, const std::string& address, uint16_t port, bool ssl_enable);
  ~HttpClient();
  void request(HttpRequest& req, HttpResponse& res);
  
  bool isConnected() const;

  void setRootCert(const std::string &path);
  void disableVerify();

private:
  void connect();
  void disconnect();

  const std::string m_address;
  const uint16_t m_port;

  std::string m_ssl_cert;

  bool m_connected = false;
  bool m_ssl_enable;
  bool m_ssl_no_verify;
  System::Dispatcher& m_dispatcher;
  System::TcpConnection m_connection;
  std::unique_ptr<System::TcpStreambuf> m_streamBuf;
  boost::asio::io_service m_io_service;
  std::unique_ptr<boost::asio::ssl::stream<tcp::socket>> m_ssl_sock;
};

template <typename Request, typename Response>
void invokeJsonCommand(HttpClient& client, const std::string& url, const Request& req, Response& res, const std::string& user = "", const std::string& password = "") {
  HttpRequest hreq;
  HttpResponse hres;

  hreq.addHeader("Connection", "keep-alive");
  hreq.addHeader("Content-Type", "application/json");
  if (!user.empty() || !password.empty()) {
    hreq.addHeader("Authorization", "Basic " + base64::encode(Common::asBinaryArray(user + ":" + password)));
  }
  hreq.setUrl(url);
  hreq.setBody(storeToJson(req));
  client.request(hreq, hres);

  if (hres.getStatus() != HttpResponse::STATUS_200) {
    throw std::runtime_error("HTTP status: " + std::to_string(hres.getStatus()));
  }

  if (!loadFromJson(res, hres.getBody())) {
    throw std::runtime_error("Failed to parse JSON response");
  }
}

template <typename Request, typename Response>
void invokeJsonRpcCommand(HttpClient& client, const std::string& method, const Request& req, Response& res, const std::string& user = "", const std::string& password = "") {
  try {

    JsonRpc::JsonRpcRequest jsReq;

    jsReq.setMethod(method);
    jsReq.setParams(req);

    HttpRequest httpReq;
    HttpResponse httpRes;

    httpReq.addHeader("Connection", "keep-alive");
    httpReq.addHeader("Content-Type", "application/json");
    if (!user.empty() || !password.empty()) {
      httpReq.addHeader("Authorization", "Basic " + base64::encode(Common::asBinaryArray(user + ":" + password)));
    }
    httpReq.setUrl("/json_rpc");
    httpReq.setBody(jsReq.getBody());

    client.request(httpReq, httpRes);

    JsonRpc::JsonRpcResponse jsRes;

    //if (httpRes.getStatus() == HttpResponse::STATUS_200) {
      jsRes.parse(httpRes.getBody());
      if (!jsRes.getResult(res)) {
        throw std::runtime_error("HTTP status: " + std::to_string(httpRes.getStatus()));
      }
    //}

  } catch (const ConnectException&) {
    throw std::runtime_error("HTTP status: CONNECT_ERROR");
  } catch (const std::exception&) {
    throw std::runtime_error("HTTP status: NETWORK_ERROR");
  }
}

template <typename Request, typename Response>
void invokeBinaryCommand(HttpClient& client, const std::string& url, const Request& req, Response& res, const std::string& user = "", const std::string& password = "") {
  HttpRequest hreq;
  HttpResponse hres;

  hreq.addHeader("Connection", "keep-alive");

  if (!user.empty() || !password.empty()) {
    hreq.addHeader("Authorization", "Basic " + base64::encode(Common::asBinaryArray(user + ":" + password)));
  }
  hreq.setUrl(url);
  hreq.setBody(storeToBinaryKeyValue(req));
  client.request(hreq, hres);

  if (!loadFromBinaryKeyValue(res, hres.getBody())) {
    throw std::runtime_error("Failed to parse binary response");
  }
}
  
}
