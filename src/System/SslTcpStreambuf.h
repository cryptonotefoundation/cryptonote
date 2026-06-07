// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers

#pragma once

#include <iostream>
#include <memory>
#include <boost/asio/ssl.hpp>
#include <System/TcpConnection.h>
#include <System/Dispatcher.h>

namespace System {

  class SslTcpStreambuf : public std::streambuf {
  public:
    SslTcpStreambuf(Dispatcher& dispatcher,
      TcpConnection& connection,
      boost::asio::ssl::context& context,
      bool isServer = false);

    SslTcpStreambuf(const SslTcpStreambuf&) = delete;
    SslTcpStreambuf& operator=(const SslTcpStreambuf&) = delete;

    ~SslTcpStreambuf() override;

    void handshake();

  private:
    int_type overflow(int_type ch) override;
    int_type underflow() override;
    int sync() override;

    void performHandshake();

    Dispatcher& dispatcher;
    TcpConnection& connection;
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>> sslStream;
    bool handshakeDone;
    bool isServer;

    char readBuf[4096];
    char writeBuf[4096];
  };

} // namespace System