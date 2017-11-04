// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "TcpConnection.h"
#include <cassert>

#include <netinet/in.h>
#include <sys/event.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Dispatcher.h"
#include <System/ErrorMessage.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>

namespace System {

TcpConnection::TcpConnection() : dispatcher(nullptr) {
}

TcpConnection::TcpConnection(TcpConnection&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    assert(other.readContext == nullptr);
    assert(other.writeContext == nullptr);
    connection = other.connection;
    readContext = nullptr;
    writeContext = nullptr;
    other.dispatcher = nullptr;
  }
}

TcpConnection::~TcpConnection() {
  if (dispatcher != nullptr) {
    assert(readContext == nullptr);
    assert(writeContext == nullptr);
    int result = close(connection);
    assert(result != -1);
  }
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) {
  if (dispatcher != nullptr) {
    assert(readContext == nullptr);
    assert(writeContext == nullptr);
    if (close(connection) == -1) {
      throw std::runtime_error("TcpConnection::operator=, close failed, " + lastErrorMessage());
    }
  }

  dispatcher = other.dispatcher;
  if (other.dispatcher != nullptr) {
    assert(other.readContext == nullptr);
    assert(other.writeContext == nullptr);
    connection = other.connection;
    readContext = nullptr;
    writeContext = nullptr;
    other.dispatcher = nullptr;
  }

  return *this;
}

size_t TcpConnection::read(uint8_t* data, size_t size) {
  assert(dispatcher != nullptr);
  assert(readContext == nullptr);
  if (dispatcher->interrupted()) {
    throw InterruptedException();
  }

  std::string message;
  ssize_t transferred = ::recv(connection, (void *)data, size, 0);
  if (transferred == -1) {
    if (errno != EAGAIN  && errno != EWOULDBLOCK) {
      message = "recv failed, " + lastErrorMessage();
    } else {
      OperationContext context;
      context.context = dispatcher->getCurrentContext();
      context.interrupted = false;
      struct kevent event;
      EV_SET(&event, connection, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT, 0, 0, &context);
      if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
        message = "kevent failed, " + lastErrorMessage();
      } else {
        readContext = &context;
        dispatcher->getCurrentContext()->interruptProcedure = [&] {
          assert(dispatcher != nullptr);
          assert(readContext != nullptr);
          OperationContext* context = static_cast<OperationContext*>(readContext);
          if (!context->interrupted) {
            struct kevent event;
            EV_SET(&event, connection, EVFILT_READ, EV_DELETE | EV_DISABLE, 0, 0, NULL);
            
            if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
              throw std::runtime_error("TcpListener::interruptionProcedure, kevent failed, " + lastErrorMessage());
            }
            
            context->interrupted = true;
            dispatcher->pushContext(context->context);
          }
        };
        
        dispatcher->dispatch();
        dispatcher->getCurrentContext()->interruptProcedure = nullptr;
        assert(dispatcher != nullptr);
        assert(context.context == dispatcher->getCurrentContext());
        assert(readContext == &context);
        readContext = nullptr;
        context.context = nullptr;
        if (context.interrupted) {
          throw InterruptedException();
        }

        ssize_t transferred = ::recv(connection, (void *)data, size, 0);
        if (transferred == -1) {
          message = "recv failed, " + lastErrorMessage();
        } else {
          assert(transferred <= static_cast<ssize_t>(size));
          return transferred;
        }
      }
    }

    throw std::runtime_error("TcpConnection::read, " + message);
  }

  assert(transferred <= static_cast<ssize_t>(size));
  return transferred;
}

size_t TcpConnection::write(const uint8_t* data, size_t size) {
  assert(dispatcher != nullptr);
  assert(writeContext == nullptr);
  if (dispatcher->interrupted()) {
    throw InterruptedException();
  }

  std::string message;
  if (size == 0) {
    if (shutdown(connection, SHUT_WR) == -1) {
      throw std::runtime_error("TcpConnection::write, shutdown failed, " + lastErrorMessage());
    }

    return 0;
  }

  ssize_t transferred = ::send(connection, (void *)data, size, 0);
  if (transferred == -1) {
    if (errno != EAGAIN  && errno != EWOULDBLOCK) {
      message = "send failed, " + lastErrorMessage();
    } else {
      OperationContext context;
      context.context = dispatcher->getCurrentContext();
      context.interrupted = false;
      struct kevent event;
      EV_SET(&event, connection, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, &context);
      if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
        message = "kevent failed, " + lastErrorMessage();
      } else {
        writeContext = &context;
        dispatcher->getCurrentContext()->interruptProcedure = [&] {
          assert(dispatcher != nullptr);
          assert(writeContext != nullptr);
          OperationContext* context = static_cast<OperationContext*>(writeContext);
          if (!context->interrupted) {
            struct kevent event;
            EV_SET(&event, connection, EVFILT_WRITE, EV_DELETE | EV_DISABLE, 0, 0, NULL);
            
            if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1) {
              throw std::runtime_error("TcpListener::stop, kevent failed, " + lastErrorMessage());
            }
            
            context->interrupted = true;
            dispatcher->pushContext(context->context);            
          }
        };
        
        dispatcher->dispatch();
        dispatcher->getCurrentContext()->interruptProcedure = nullptr;
        assert(dispatcher != nullptr);
        assert(context.context == dispatcher->getCurrentContext());
        assert(writeContext == &context);
        writeContext = nullptr;
        context.context = nullptr;
        if (context.interrupted) {
          throw InterruptedException();
        }

        ssize_t transferred = ::send(connection, (void *)data, size, 0);
        if (transferred == -1) {
          message = "send failed, " + lastErrorMessage();
        } else {
          assert(transferred <= static_cast<ssize_t>(size));
          return transferred;
        }
      }
    }

    throw std::runtime_error("TcpConnection::write, " + message);
  }

  assert(transferred <= static_cast<ssize_t>(size));
  return transferred;
}

std::pair<Ipv4Address, uint16_t> TcpConnection::getPeerAddressAndPort() const {
  sockaddr_in addr;
  socklen_t size = sizeof(addr);
  if (getpeername(connection, reinterpret_cast<sockaddr*>(&addr), &size) != 0) {
    throw std::runtime_error("TcpConnection::getPeerAddress, getpeername failed, " + lastErrorMessage());
  }

  assert(size == sizeof(sockaddr_in));
  return std::make_pair(Ipv4Address(htonl(addr.sin_addr.s_addr)), htons(addr.sin_port));
}

TcpConnection::TcpConnection(Dispatcher& dispatcher, int socket) : dispatcher(&dispatcher), connection(socket), readContext(nullptr), writeContext(nullptr) {
  int val = 1;
  if (setsockopt(connection, SOL_SOCKET, SO_NOSIGPIPE, (void*)&val, sizeof val) == -1) {
    throw std::runtime_error("TcpConnection::TcpConnection, setsockopt failed, " + lastErrorMessage());
  }
}

}
