// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2025, The Karbo developers

#pragma once

#include <System/TcpConnection.h>
#include <System/Dispatcher.h>
#include <System/Ipv4Address.h>

namespace System {

class TcpConnector {
public:
    TcpConnector();
    explicit TcpConnector(Dispatcher& dispatcher);
    TcpConnector(const TcpConnector&) = delete;
    TcpConnector(TcpConnector&& other) noexcept;
    ~TcpConnector();
    TcpConnector& operator=(const TcpConnector&) = delete;
    TcpConnector& operator=(TcpConnector&& other) noexcept;

    TcpConnection connect(const Ipv4Address& address, uint16_t port);

private:
    Dispatcher* dispatcher{nullptr};
};

} // namespace System

