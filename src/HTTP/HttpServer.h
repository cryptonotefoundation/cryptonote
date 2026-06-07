// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <unordered_set>
#include <boost/asio/ssl.hpp>

#include "HttpRequest.h"
#include "HttpResponse.h"
#include <System/Dispatcher.h>
#include <System/ContextGroup.h>
#include <System/TcpListener.h>
#include <System/TcpConnection.h>
#include <Logging/LoggerRef.h>

namespace CryptoNote {

class HttpServer {
public:
  typedef std::function<void(const HttpRequest& request, HttpResponse& response)> RequestHandler;

  HttpServer(System::Dispatcher& dispatcher, Logging::ILogger& log);
  virtual ~HttpServer();

  // Start server without SSL
  void start(const std::string& address, uint16_t port, 
             const std::string& user = "", const std::string& password = "");
  
  // Start server with SSL
  void startSsl(const std::string& address, uint16_t port,
                const std::string& certFile, const std::string& keyFile,
                const std::string& dhFile = "",
                const std::string& user = "", const std::string& password = "");
  
  void stop();
  size_t getConnectionsCount() const;
  
  void setRequestHandler(RequestHandler handler);

protected:
  virtual void processRequest(const HttpRequest& request, HttpResponse& response);

private:
  void acceptLoop();
  void connectionWorker(System::TcpConnection& connection);
  bool authenticate(const HttpRequest& request) const;
  
  System::Dispatcher& m_dispatcher;
  System::ContextGroup workingContextGroup;
  Logging::LoggerRef logger;
  
  System::TcpListener m_listener;
  std::unordered_set<System::TcpConnection*> m_connections;
  
  // SSL support
  bool m_useSsl{false};
  std::unique_ptr<boost::asio::ssl::context> m_sslContext;
  
  // Authentication
  std::string m_credentials; // Base64 encoded user:password
  
  // Request handler
  RequestHandler m_requestHandler;

  std::atomic<bool> m_stopping{ false };
};

} // namespace CryptoNote
