// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2025, The Karbo developers

#include "TcpConnection.h"
#include <boost/asio.hpp>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <cassert>
#include <stdexcept>

namespace System {
  namespace {
    void shutdownAndClose(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket) {
      if (!socket || !socket->is_open()) {
        return;
      }

      boost::system::error_code ignored;
      socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
      ignored.clear();
      socket->close(ignored);
    }
  }

  TcpConnection::TcpConnection() = default;

  TcpConnection::TcpConnection(TcpConnection&& other) noexcept
    : dispatcher(other.dispatcher), socketPtr(std::move(other.socketPtr)) {
    other.dispatcher = nullptr;
  }

  TcpConnection::~TcpConnection() {
    shutdownAndClose(socketPtr);
  }

  TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
    if (this != &other) {
      shutdownAndClose(socketPtr);
      dispatcher = other.dispatcher;
      socketPtr = std::move(other.socketPtr);
      other.dispatcher = nullptr;
    }
    return *this;
  }

  TcpConnection::TcpConnection(Dispatcher& disp, std::shared_ptr<boost::asio::ip::tcp::socket> sock)
    : dispatcher(&disp), socketPtr(std::move(sock)) {
  }

  size_t TcpConnection::read(uint8_t* data, size_t size) {
    assert(dispatcher != nullptr);
    if (dispatcher->interrupted()) throw InterruptedException();

    std::size_t transferred = 0;
    bool done = false;
    boost::system::error_code ec;

    NativeContext* waiting = dispatcher->getCurrentContext();

    // Set interrupt procedure: close socket to cancel outstanding read
    waiting->interruptProcedure = [socket = socketPtr]() {
      shutdownAndClose(socket);
      };

    // Use async_read_some (or async_read depending on desired semantics)
    socketPtr->async_read_some(boost::asio::buffer(data, size),
      [&, waiting](const boost::system::error_code& error, std::size_t bytes_transferred) {
        ec = error;
        transferred = bytes_transferred;
        done = true;
        // resume waiting fiber
        dispatcher->pushContext(waiting);
      });

    // Wait cooperatively for completion
    while (!done) {
      if (dispatcher->interrupted()) {
        shutdownAndClose(socketPtr);
      }
      dispatcher->dispatch();
    }

    // clear interrupt procedure
    waiting->interruptProcedure = nullptr;

    if (ec) {
      if (ec == boost::asio::error::operation_aborted) throw InterruptedException();
      if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset) return 0;
      throw std::runtime_error("TcpConnection::read failed: " + ec.message());
    }

    return transferred;
  }

  size_t TcpConnection::write(const uint8_t* data, size_t size) {
    assert(dispatcher != nullptr);
    if (dispatcher->interrupted()) throw InterruptedException();

    if (size == 0) {
      // shutdown send side
      boost::system::error_code ec;
      socketPtr->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
      if (ec) throw std::runtime_error("TcpConnection::write shutdown failed: " + ec.message());
      return 0;
    }

    std::size_t transferred = 0;
    bool done = false;
    boost::system::error_code ec;

    NativeContext* waiting = dispatcher->getCurrentContext();

    waiting->interruptProcedure = [socket = socketPtr]() {
      shutdownAndClose(socket);
      };

    boost::asio::async_write(*socketPtr, boost::asio::buffer(data, size),
      [&, waiting](const boost::system::error_code& error, std::size_t bytes_transferred) {
        ec = error;
        transferred = bytes_transferred;
        done = true;
        dispatcher->pushContext(waiting);
      });

    while (!done) {
      if (dispatcher->interrupted()) {
        shutdownAndClose(socketPtr);
      }
      dispatcher->dispatch();
    }

    waiting->interruptProcedure = nullptr;

    if (ec) {
      if (ec == boost::asio::error::operation_aborted) throw InterruptedException();
      throw std::runtime_error("TcpConnection::write failed: " + ec.message());
    }

    if (transferred != size) {
      throw std::runtime_error("TcpConnection::write failed: transferred != size");
    }

    return transferred;
  }

  std::pair<Ipv4Address, uint16_t> TcpConnection::getPeerAddressAndPort() const {
    boost::system::error_code ec;
    auto endpoint = socketPtr->remote_endpoint(ec);
    if (ec) throw std::runtime_error("Failed to get peer endpoint: " + ec.message());
    // note: to_uint() not available; use to_uint? in boost::asio older versions, use to_ulong/from_string etc.
    auto v4 = endpoint.address().to_v4();
    uint32_t addr_value = v4.to_uint(); // to_uint exists in some boost versions; if not, use to_ulong() or to_bytes() accordingly
    return { Ipv4Address(addr_value), endpoint.port() };
  }
  
  boost::asio::ip::tcp::socket& TcpConnection::getSocket() {
    if (!socketPtr) throw std::runtime_error("Socket not initialized");
    return *socketPtr;
  }

} // namespace System
