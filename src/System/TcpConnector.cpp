// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2025, The Karbo developers

#include "TcpConnector.h"
#include <boost/asio.hpp>
#include <System/InterruptedException.h>
#include <cassert>
#include <memory>
#include <stdexcept>

namespace System {

  TcpConnector::TcpConnector() = default;

  TcpConnector::TcpConnector(Dispatcher& disp) : dispatcher(&disp) {}

  TcpConnector::TcpConnector(TcpConnector&& other) noexcept : dispatcher(other.dispatcher) {
    other.dispatcher = nullptr;
  }

  TcpConnector::~TcpConnector() = default;

  TcpConnector& TcpConnector::operator=(TcpConnector&& other) noexcept {
    if (this != &other) {
      dispatcher = other.dispatcher;
      other.dispatcher = nullptr;
    }
    return *this;
  }

  TcpConnection TcpConnector::connect(const Ipv4Address& addr, uint16_t port) {
    assert(dispatcher != nullptr);

    if (dispatcher->interrupted()) throw InterruptedException();

    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(dispatcher->getIoContext());
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address_v4(addr.getValue()), port);

    // state shared between lambda and current stack
    bool done = false;
    boost::system::error_code ec;

    // Save waiting context so we can resume it from handler
    NativeContext* waiting = dispatcher->getCurrentContext();

    // Setup interrupt procedure for the waiting context: close socket to cancel connect
    waiting->interruptProcedure = [socket]() {
      boost::system::error_code ignored_ec;
      // Closing the socket will cancel outstanding async_connect
      socket->close(ignored_ec);
      };

    socket->async_connect(endpoint, [&, waiting](const boost::system::error_code& error) {
      ec = error;
      done = true;
      // schedule the waiting fiber to resume
      dispatcher->pushContext(waiting);
      });

    // Wait cooperatively for completion (dispatcher::dispatch will poll io_context)
    while (!done) {
      if (dispatcher->interrupted()) {
        // If dispatcher was interrupted while waiting, ensure socket is closed
        boost::system::error_code ignored_ec;
        socket->close(ignored_ec);
        // Let the handler detect ec and resume us; continue dispatching until done
      }
      dispatcher->dispatch();
    }

    // clear interrupt procedure
    waiting->interruptProcedure = nullptr;

    if (ec) {
      // If interrupted by user, Asio often returns operation_aborted
      if (ec == boost::asio::error::operation_aborted) {
        throw InterruptedException();
      }
      throw std::runtime_error("Connect failed: " + ec.message());
    }

    return TcpConnection(*dispatcher, socket);
  }

} // namespace System
