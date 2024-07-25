// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TcpConnection.h"
#include <cassert>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2ipdef.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include "Dispatcher.h"
#include "ErrorMessage.h"
#include <stdexcept>

namespace System {

namespace {

struct TcpConnectionContext : public OVERLAPPED {
  NativeContext* context;
  bool interrupted;
};

}

TcpConnection::TcpConnection() : dispatcher(nullptr) {
}

TcpConnection::TcpConnection(TcpConnection&& other) : dispatcher(other.dispatcher) {
  if (dispatcher != nullptr) {
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
    int result = closesocket(connection);
    assert(result == 0);
  }
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) {
  if (dispatcher != nullptr) {
    assert(readContext == nullptr);
    assert(writeContext == nullptr);
    if (closesocket(connection) != 0) {
      throw std::runtime_error("TcpConnection::operator=, closesocket failed, " + errorMessage(WSAGetLastError()));
    }
  }

  dispatcher = other.dispatcher;
  if (dispatcher != nullptr) {
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

  WSABUF buf{static_cast<ULONG>(size), reinterpret_cast<char*>(data)};
  DWORD flags = 0;
  TcpConnectionContext context;
  context.hEvent = NULL;
  if (WSARecv(connection, &buf, 1, NULL, &flags, &context, NULL) != 0) {
    int lastError = WSAGetLastError();
    if (lastError != WSA_IO_PENDING) {
      throw std::runtime_error("TcpConnection::read, WSARecv failed, " + errorMessage(lastError));
    }
  }

  assert(flags == 0);
  context.context = dispatcher->getCurrentContext();
  context.interrupted = false;
  readContext = &context;
  dispatcher->getCurrentContext()->interruptProcedure = [&]() {
    assert(dispatcher != nullptr);
    assert(readContext != nullptr);
    TcpConnectionContext* context = static_cast<TcpConnectionContext*>(readContext);
    if (!context->interrupted) {
      if (CancelIoEx(reinterpret_cast<HANDLE>(connection), context) != TRUE) {
        DWORD lastError = GetLastError();
        if (lastError != ERROR_NOT_FOUND) {
          throw std::runtime_error("TcpConnection::stop, CancelIoEx failed, " + lastErrorMessage());
        }

        context->context->interrupted = true;
      }

      context->interrupted = true;
    }
  };

  dispatcher->dispatch();
  dispatcher->getCurrentContext()->interruptProcedure = nullptr;
  assert(context.context == dispatcher->getCurrentContext());
  assert(dispatcher != nullptr);
  assert(readContext == &context);
  readContext = nullptr;
  DWORD transferred;
  if (WSAGetOverlappedResult(connection, &context, &transferred, FALSE, &flags) != TRUE) {
    int lastError = WSAGetLastError();
    if (lastError != ERROR_OPERATION_ABORTED) {
      throw std::runtime_error("TcpConnection::read, WSAGetOverlappedResult failed, " + errorMessage(lastError));
    }

    assert(context.interrupted);
    throw InterruptedException();
  }

  assert(transferred <= size);
  assert(flags == 0);
  return transferred;
}

size_t TcpConnection::write(const uint8_t* data, size_t size) {
  assert(dispatcher != nullptr);
  assert(writeContext == nullptr);
  if (dispatcher->interrupted()) {
    throw InterruptedException();
  }

  if (size == 0) {
    if (shutdown(connection, SD_SEND) != 0) {
      throw std::runtime_error("TcpConnection::write, shutdown failed, " + errorMessage(WSAGetLastError()));
    }

    return 0;
  }

  WSABUF buf{static_cast<ULONG>(size), reinterpret_cast<char*>(const_cast<uint8_t*>(data))};
  TcpConnectionContext context;
  context.hEvent = NULL;
  if (WSASend(connection, &buf, 1, NULL, 0, &context, NULL) != 0) {
    int lastError = WSAGetLastError();
    if (lastError != WSA_IO_PENDING) {
      throw std::runtime_error("TcpConnection::write, WSASend failed, " + errorMessage(lastError));
    }
  }

  context.context = dispatcher->getCurrentContext();
  context.interrupted = false;
  writeContext = &context;
  dispatcher->getCurrentContext()->interruptProcedure = [&]() {
    assert(dispatcher != nullptr);
    assert(writeContext != nullptr);
    TcpConnectionContext* context = static_cast<TcpConnectionContext*>(writeContext);
    if (!context->interrupted) {
      if (CancelIoEx(reinterpret_cast<HANDLE>(connection), context) != TRUE) {
        DWORD lastError = GetLastError();
        if (lastError != ERROR_NOT_FOUND) {
          throw std::runtime_error("TcpConnection::stop, CancelIoEx failed, " + lastErrorMessage());
        }

        context->context->interrupted = true;
      }

      context->interrupted = true;
    }
  };

  dispatcher->dispatch();
  dispatcher->getCurrentContext()->interruptProcedure = nullptr;
  assert(context.context == dispatcher->getCurrentContext());
  assert(dispatcher != nullptr);
  assert(writeContext == &context);
  writeContext = nullptr;
  DWORD transferred;
  DWORD flags;
  if (WSAGetOverlappedResult(connection, &context, &transferred, FALSE, &flags) != TRUE) {
    int lastError = WSAGetLastError();
    if (lastError != ERROR_OPERATION_ABORTED) {
      throw std::runtime_error("TcpConnection::write, WSAGetOverlappedResult failed, " + errorMessage(lastError));
    }

    assert(context.interrupted);
    throw InterruptedException();
  }

  assert(transferred == size);
  assert(flags == 0);
  return transferred;
}

std::pair<Ipv4Address, uint16_t> TcpConnection::getPeerAddressAndPort() const {
  sockaddr_in address;
  int size = sizeof(address);
  if (getpeername(connection, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
    throw std::runtime_error("TcpConnection::getPeerAddress, getpeername failed, " + errorMessage(WSAGetLastError()));
  }

  assert(size == sizeof(sockaddr_in));
  return std::make_pair(Ipv4Address(htonl(address.sin_addr.S_un.S_addr)), htons(address.sin_port));
}

TcpConnection::TcpConnection(Dispatcher& dispatcher, size_t connection) : dispatcher(&dispatcher), connection(connection), readContext(nullptr), writeContext(nullptr) {
}

}
