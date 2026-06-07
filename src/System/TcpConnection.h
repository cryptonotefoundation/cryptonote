// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <System/Dispatcher.h>

namespace System {

class Dispatcher;
class Ipv4Address;

class TcpConnection {
public:
    TcpConnection();
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&& other) noexcept;
    ~TcpConnection();
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    size_t read(uint8_t* data, size_t size);
    size_t write(const uint8_t* data, size_t size);
    std::pair<Ipv4Address, uint16_t> getPeerAddressAndPort() const;
    boost::asio::ip::tcp::socket& getSocket();

private:
    friend class TcpConnector;
    friend class TcpListener;

    TcpConnection(Dispatcher& dispatcher, std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    Dispatcher* dispatcher{nullptr};
    std::shared_ptr<boost::asio::ip::tcp::socket> socketPtr;
};

} // namespace System
