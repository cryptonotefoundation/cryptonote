// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers

#include "SslTcpStreambuf.h"
#include <System/InterruptedException.h>
#include <stdexcept>

namespace System {

  SslTcpStreambuf::SslTcpStreambuf(Dispatcher& disp,
    TcpConnection& conn,
    boost::asio::ssl::context& context,
    bool server)
    : dispatcher(disp)
    , connection(conn)
    , handshakeDone(false)
    , isServer(server)
  {
    sslStream = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>>(
      connection.getSocket(), context);

    setg(readBuf, readBuf, readBuf);
    setp(writeBuf, writeBuf + sizeof(writeBuf));
  }

  SslTcpStreambuf::~SslTcpStreambuf() {
    try {
      sync();
      boost::system::error_code ec;
      sslStream->shutdown(ec);
    }
    catch (...) {
      // Suppress exceptions in destructor
    }
  }

  void SslTcpStreambuf::performHandshake() {
    bool done = false;
    boost::system::error_code ec;

    NativeContext* waiting = dispatcher.getCurrentContext();

    // Async handshake
    if (isServer) {
      sslStream->async_handshake(boost::asio::ssl::stream_base::server,
        [&, waiting](const boost::system::error_code& error) {
          ec = error;
          done = true;
          dispatcher.pushContext(waiting);
        });
    }
    else {
      sslStream->async_handshake(boost::asio::ssl::stream_base::client,
        [&, waiting](const boost::system::error_code& error) {
          ec = error;
          done = true;
          dispatcher.pushContext(waiting);
        });
    }

    // Wait for completion
    while (!done) {
      dispatcher.dispatch();
    }

    if (ec) {
      throw std::runtime_error("SSL handshake failed: " + ec.message());
    }
  }

  void SslTcpStreambuf::handshake() {
    if (handshakeDone) {
      return;
    }

    performHandshake();
    handshakeDone = true;
  }

  SslTcpStreambuf::int_type SslTcpStreambuf::overflow(int_type ch) {
    if (ch == traits_type::eof()) {
      return traits_type::eof();
    }

    if (pptr() == epptr()) {
      if (sync() == -1) {
        return traits_type::eof();
      }
    }

    *pptr() = static_cast<char>(ch);
    pbump(1);
    return ch;
  }

  SslTcpStreambuf::int_type SslTcpStreambuf::underflow() {
    if (gptr() < egptr()) {
      return traits_type::to_int_type(*gptr());
    }

    if (!handshakeDone) {
      handshake();
    }

    bool done = false;
    boost::system::error_code ec;
    size_t bytesRead = 0;

    NativeContext* waiting = dispatcher.getCurrentContext();

    // Async read
    sslStream->async_read_some(boost::asio::buffer(readBuf, sizeof(readBuf)),
      [&, waiting](const boost::system::error_code& error, size_t bytes_transferred) {
        ec = error;
        bytesRead = bytes_transferred;
        done = true;
        dispatcher.pushContext(waiting);
      });

    // Wait for completion
    while (!done) {
      dispatcher.dispatch();
    }

    if (ec) {
      if (ec == boost::asio::error::eof) {
        return traits_type::eof();
      }
      throw std::runtime_error("SSL read failed: " + ec.message());
    }

    if (bytesRead == 0) {
      return traits_type::eof();
    }

    setg(readBuf, readBuf, readBuf + bytesRead);
    return traits_type::to_int_type(*gptr());
  }

  int SslTcpStreambuf::sync() {
    if (pbase() == pptr()) {
      return 0;
    }

    if (!handshakeDone) {
      handshake();
    }

    size_t bytesToWrite = pptr() - pbase();
    bool done = false;
    boost::system::error_code ec;
    size_t bytesWritten = 0;

    NativeContext* waiting = dispatcher.getCurrentContext();

    // Async write
    boost::asio::async_write(*sslStream, boost::asio::buffer(writeBuf, bytesToWrite),
      [&, waiting](const boost::system::error_code& error, size_t bytes_transferred) {
        ec = error;
        bytesWritten = bytes_transferred;
        done = true;
        dispatcher.pushContext(waiting);
      });

    // Wait for completion
    while (!done) {
      dispatcher.dispatch();
    }

    if (ec) {
      throw std::runtime_error("SSL write failed: " + ec.message());
    }

    if (bytesWritten != bytesToWrite) {
      throw std::runtime_error("SSL partial write");
    }

    setp(writeBuf, writeBuf + sizeof(writeBuf));
    return 0;
  }

} // namespace System