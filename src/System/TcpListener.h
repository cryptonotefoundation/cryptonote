// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2025, The Karbo developers

#pragma once

#include <System/TcpConnection.h>
#include <System/Dispatcher.h>
#include <System/Ipv4Address.h>

namespace System {

class TcpListener {
public:
    TcpListener();
    TcpListener(Dispatcher& dispatcher, const Ipv4Address& address, uint16_t port);
    TcpListener(const TcpListener&) = delete;
    TcpListener(TcpListener&& other) noexcept;
    ~TcpListener();
    TcpListener& operator=(const TcpListener&) = delete;
    TcpListener& operator=(TcpListener&& other) noexcept;

    TcpConnection accept();
    void close();

private:
    Dispatcher* dispatcher{nullptr};
    std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor;
};

} // namespace System

