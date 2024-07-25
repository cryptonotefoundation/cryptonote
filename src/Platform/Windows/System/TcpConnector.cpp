// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TcpConnector.h"
#include <cassert>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <mswsock.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include "Dispatcher.h"
#include "ErrorMessage.h"
#include "TcpConnection.h"
#include <stdexcept>

namespace System {

namespace {

struct TcpConnectorContext : public OVERLAPPED {
  NativeContext* context;
  size_t connection;
  bool interrupted;
};

LPFN_CONNECTEX connectEx = nullptr;

}

TcpConnector::TcpConnector() : dispatcher(nullptr) {
}

TcpConnector::TcpConnector(Dispatcher& dispatcher) : dispatcher(&dispatcher), context(nullptr) {
}

TcpConnector::TcpConnector(TcpConnector&& other) : dispatcher(other.dispatcher) {
  if (dispatcher != nullptr) {
    assert(other.context == nullptr);
    context = nullptr;
    other.dispatcher = nullptr;
  }
}

TcpConnector::~TcpConnector() {
  assert(dispatcher == nullptr || context == nullptr);
}

TcpConnector& TcpConnector::operator=(TcpConnector&& other) {
  assert(dispatcher == nullptr || context == nullptr);
  dispatcher = other.dispatcher;
  if (dispatcher != nullptr) {
    assert(other.context == nullptr);
    context = nullptr;
    other.dispatcher = nullptr;
  }

  return *this;
}

TcpConnection TcpConnector::connect(const Ipv4Address& address, uint16_t port) {
  assert(dispatcher != nullptr);
  assert(context == nullptr);
  if (dispatcher->interrupted()) {
    throw InterruptedException();
  }

  std::string message;
  SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (connection == INVALID_SOCKET) {
    message = "socket failed, " + errorMessage(WSAGetLastError());
  } else {
    sockaddr_in bindAddress;
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = 0;
    bindAddress.sin_addr.s_addr = INADDR_ANY;
    if (bind(connection, reinterpret_cast<sockaddr*>(&bindAddress), sizeof bindAddress) != 0) {
      message = "bind failed, " + errorMessage(WSAGetLastError());
    } else {
      GUID guidConnectEx = WSAID_CONNECTEX;
      DWORD read = sizeof connectEx;
      if (connectEx == nullptr && WSAIoctl(connection, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidConnectEx, sizeof guidConnectEx, &connectEx, sizeof connectEx, &read, NULL, NULL) != 0) {
        message = "WSAIoctl failed, " + errorMessage(WSAGetLastError());
      } else {
        assert(read == sizeof connectEx);
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(connection), dispatcher->getCompletionPort(), 0, 0) != dispatcher->getCompletionPort()) {
          message = "CreateIoCompletionPort failed, " + lastErrorMessage();
        } else {
          sockaddr_in addressData;
          addressData.sin_family = AF_INET;
          addressData.sin_port = htons(port);
          addressData.sin_addr.S_un.S_addr = htonl(address.getValue());
          TcpConnectorContext context2;
          context2.hEvent = NULL;
          if (connectEx(connection, reinterpret_cast<sockaddr*>(&addressData), sizeof addressData, NULL, 0, NULL, &context2) == TRUE) {
            message = "ConnectEx returned immediately, which is not supported.";
          } else {
            int lastError = WSAGetLastError();
            if (lastError != WSA_IO_PENDING) {
              message = "ConnectEx failed, " + errorMessage(lastError);
            } else {
              context2.context = dispatcher->getCurrentContext();
              context2.connection = connection;
              context2.interrupted = false;
              context = &context2;
              dispatcher->getCurrentContext()->interruptProcedure = [&]() {
                assert(dispatcher != nullptr);
                assert(context != nullptr);
                TcpConnectorContext* context2 = static_cast<TcpConnectorContext*>(context);
                if (!context2->interrupted) {
                  if (CancelIoEx(reinterpret_cast<HANDLE>(context2->connection), context2) != TRUE) {
                    DWORD lastError = GetLastError();
                    if (lastError != ERROR_NOT_FOUND) {
                      throw std::runtime_error("TcpConnector::stop, CancelIoEx failed, " + lastErrorMessage());
                    }

                    context2->context->interrupted = true;
                  }

                  context2->interrupted = true;
                }
              };

              dispatcher->dispatch();
              dispatcher->getCurrentContext()->interruptProcedure = nullptr;
              assert(context2.context == dispatcher->getCurrentContext());
              assert(context2.connection == connection);
              assert(dispatcher != nullptr);
              assert(context == &context2);
              context = nullptr;
              DWORD transferred;
              DWORD flags;
              if (WSAGetOverlappedResult(connection, &context2, &transferred, FALSE, &flags) != TRUE) {
                lastError = WSAGetLastError();
                if (lastError != ERROR_OPERATION_ABORTED) {
                  message = "ConnectEx failed, " + errorMessage(lastError);
                } else {
                  assert(context2.interrupted);
                  if (closesocket(connection) != 0) {
                    throw std::runtime_error("TcpConnector::connect, closesocket failed, " + errorMessage(WSAGetLastError()));
                  } else {
                    throw InterruptedException();
                  }
                }
              } else {
                assert(transferred == 0);
                assert(flags == 0);
                DWORD value = 1;
                if (setsockopt(connection, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, reinterpret_cast<char*>(&value), sizeof(value)) != 0) {
                  message = "setsockopt failed, " + errorMessage(WSAGetLastError());
                } else {
                  return TcpConnection(*dispatcher, connection);
                }
              }
            }
          }
        }
      }
    }

    int result = closesocket(connection);
    assert(result == 0);
  }

  throw std::runtime_error("TcpConnector::connect, " + message);
}

}
