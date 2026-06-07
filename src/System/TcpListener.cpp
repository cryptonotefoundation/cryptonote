// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2025, The Karbo developers

#include "TcpListener.h"
#include <boost/asio.hpp>
#include <System/InterruptedException.h>
#include <cassert>
#include <memory>
#include <stdexcept>

namespace System {

  TcpListener::TcpListener() = default;

  TcpListener::TcpListener(Dispatcher& disp, const Ipv4Address& addr, uint16_t port)
    : dispatcher(&disp) {
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address_v4(addr.getValue()), port);
    acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(dispatcher->getIoContext(), endpoint);
    acceptor->listen();
  }

  TcpListener::TcpListener(TcpListener&& other) noexcept
    : dispatcher(other.dispatcher), acceptor(std::move(other.acceptor)) {
    other.dispatcher = nullptr;
  }

  TcpListener::~TcpListener() = default;

  TcpListener& TcpListener::operator=(TcpListener&& other) noexcept {
    if (this != &other) {
      dispatcher = other.dispatcher;
      acceptor = std::move(other.acceptor);
      other.dispatcher = nullptr;
    }
    return *this;
  }

  void TcpListener::close() {
    if (acceptor && acceptor->is_open()) {
      boost::system::error_code ignored_ec;

      // Cancel any pending async operations
      acceptor->cancel(ignored_ec);

      // Close the acceptor socket
      acceptor->close(ignored_ec);

      // We ignore errors during close
    }
  }

  TcpConnection TcpListener::accept() {
    assert(dispatcher != nullptr);
    assert(acceptor);

    if (dispatcher->interrupted()) throw InterruptedException();

    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(dispatcher->getIoContext());
    bool done = false;
    boost::system::error_code ec;

    NativeContext* waiting = dispatcher->getCurrentContext();

    // Setup interrupt procedure: cancel accept by closing acceptor
    waiting->interruptProcedure = [this]() {
      boost::system::error_code ignored_ec;
      if (acceptor && acceptor->is_open()) {
        acceptor->close(ignored_ec);
      }
      };

    acceptor->async_accept(*socket, [&, waiting](const boost::system::error_code& error) {
      ec = error;
      done = true;
      // resume waiting fiber
      dispatcher->pushContext(waiting);
      });

    // Wait cooperatively for accept completion
    while (!done) {
      if (dispatcher->interrupted()) {
        // try to cancel accept - interruptProcedure already set will close acceptor
        boost::system::error_code ignored_ec;
        if (acceptor && acceptor->is_open()) {
          acceptor->close(ignored_ec);
        }
      }
      dispatcher->dispatch();
    }

    // Clear interrupt procedure
    waiting->interruptProcedure = nullptr;

    if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
        throw InterruptedException();
      }
      throw std::runtime_error("Accept failed: " + ec.message());
    }

    return TcpConnection(*dispatcher, socket);
  }

} // namespace System
