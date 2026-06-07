// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#pragma once

#include <memory>
#include <string>
#include <boost/asio/ssl.hpp>

#include "HttpRequest.h"
#include "HttpResponse.h"
#include <System/Dispatcher.h>
#include <System/TcpConnection.h>
#include <System/TcpStream.h>
#include <System/SslTcpStreambuf.h>

namespace CryptoNote {

class ConnectException : public std::runtime_error {
public:
  explicit ConnectException(const std::string& whatArg);
};

class HttpClient {
public:
  HttpClient(System::Dispatcher& dispatcher, const std::string& address, uint16_t port);

  // SSL client
  HttpClient(System::Dispatcher& dispatcher, const std::string& address, uint16_t port,
    const std::string& certFile, const std::string& keyFile = "", bool sslVerify = true);

  ~HttpClient();

  void request(const HttpRequest& req, HttpResponse& res);

  bool isConnected() const;
  void disconnect();

private:
  void connect();
  void connectSsl();

  System::Dispatcher& m_dispatcher;
  std::string m_address;
  uint16_t m_port;

  bool m_connected{ false };
  System::TcpConnection m_connection;
  std::unique_ptr<System::TcpStreambuf> m_streamBuf;
  std::unique_ptr<System::SslTcpStreambuf> m_sslStreamBuf;

  // SSL support
  bool m_useSsl{ false };
  bool m_sslVerify{ true };
  std::unique_ptr<boost::asio::ssl::context> m_sslContext;
  std::string m_certFile;
  std::string m_keyFile;
};

} // namespace CryptoNote
