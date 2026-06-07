// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#include "HttpClient.h"
#include "HttpParser.h"
#include <System/Ipv4Resolver.h>
#include <System/Ipv4Address.h>
#include <System/TcpConnector.h>
#include <System/SslTcpStreambuf.h>

namespace CryptoNote {

  ConnectException::ConnectException(const std::string& whatArg) 
    : std::runtime_error(whatArg.c_str()) {
  }

  HttpClient::HttpClient(System::Dispatcher& dispatcher,
    const std::string& address,
    uint16_t port)
    : m_dispatcher(dispatcher),
    m_address(address),
    m_port(port),
    m_useSsl(false) {
  }

  HttpClient::HttpClient(System::Dispatcher& dispatcher,
    const std::string& address,
    uint16_t port,
    const std::string& certFile,
    const std::string& keyFile,
    bool sslVerify)
    : m_dispatcher(dispatcher),
    m_address(address),
    m_port(port),
    m_useSsl(true),
    m_sslVerify(sslVerify),
    m_certFile(certFile),
    m_keyFile(keyFile) {

    try {
      m_sslContext = std::make_unique<boost::asio::ssl::context>(
        boost::asio::ssl::context::tls_client);

      if (sslVerify) {
      // Enable certificate verification
        m_sslContext->set_verify_mode(boost::asio::ssl::verify_peer);

        if (!certFile.empty()) {
        // Use custom CA certificate file
          m_sslContext->load_verify_file(certFile);
        }
        else {
        // Use system's default CA certificates (for public SSL sites)
          m_sslContext->set_default_verify_paths();
        }
      }
      else {
      // Disable certificate verification (for self-signed certs, testing, etc.)
        m_sslContext->set_verify_mode(boost::asio::ssl::verify_none);
      }

    // Load private key if provided (for client certificate authentication)
    // This is separate from verification - it's for mutual TLS
      if (!keyFile.empty()) {
        m_sslContext->use_private_key_file(keyFile, boost::asio::ssl::context::pem);
      }

    }
    catch (const std::exception& e) {
      throw ConnectException("Failed to initialize SSL context: " + std::string(e.what()));
    }
  }

  HttpClient::~HttpClient() {
    if (m_connected) {
      disconnect();
    }
  }

  void HttpClient::request(const HttpRequest& req, HttpResponse& res) {
    if (!m_connected) {
      if (m_useSsl) {
        connectSsl();
      }
      else {
        connect();
      }
    }

    try {
      std::iostream stream(m_useSsl ?
        static_cast<std::streambuf*>(m_sslStreamBuf.get()) :
        static_cast<std::streambuf*>(m_streamBuf.get()));
      HttpParser parser;

      stream << req;
      stream.flush();

      if (!stream) {
        throw std::runtime_error("Failed to send request");
      }

      parser.receiveResponse(stream, res);

    }
    catch (const std::exception&) {
      disconnect();
      throw;
    }
  }

  void HttpClient::connect() {
    try {
      auto ipAddr = System::Ipv4Resolver(m_dispatcher).resolve(m_address);
      m_connection = System::TcpConnector(m_dispatcher).connect(ipAddr, m_port);
      m_streamBuf.reset(new System::TcpStreambuf(m_connection));
      m_connected = true;
    }
    catch (const std::exception& e) {
      throw ConnectException(e.what());
    }
  }

  void HttpClient::connectSsl() {
    try {
      // Establish TCP connection
      auto ipAddr = System::Ipv4Resolver(m_dispatcher).resolve(m_address);
      m_connection = System::TcpConnector(m_dispatcher).connect(ipAddr, m_port);

      // Create SSL stream buffer (wraps the socket)
      m_sslStreamBuf.reset(new System::SslTcpStreambuf(
        m_dispatcher,
        m_connection,
        *m_sslContext,
        false));

      // Perform SSL handshake
      m_sslStreamBuf->handshake();

      m_connected = true;

    }
    catch (const std::exception& e) {
      throw ConnectException(std::string("SSL connection failed: ") + e.what());
    }
  }

  bool HttpClient::isConnected() const {
    return m_connected;
  }

  void HttpClient::disconnect() {
    m_sslStreamBuf.reset();
    m_streamBuf.reset();

    try {
      m_connection.write(nullptr, 0); // Socket shutdown
    }
    catch (const std::exception&) {
      // Ignoring possible exception
    }

    try {
      m_connection = System::TcpConnection();
    }
    catch (const std::exception&) {
      // Ignoring possible exception
    }

    m_connected = false;
  }

} // namespace CryptoNote