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

#include <System/Dispatcher.h>
#include <System/ContextGroup.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/TcpConnection.h>
#include <System/TcpConnector.h>
#include <System/TcpListener.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

class TcpConnectorTests : public testing::Test {
public:
  TcpConnectorTests() : event(dispatcher), listener(dispatcher, Ipv4Address("127.0.0.1"), 6666), contextGroup(dispatcher) {
  }
  
  Dispatcher dispatcher;
  Event event;
  TcpListener listener;
  ContextGroup contextGroup;
};

TEST_F(TcpConnectorTests, tcpConnector1) {
  contextGroup.spawn([&]() {
    listener.accept();
    event.set();
  });

  TcpConnector connector(dispatcher);
  contextGroup.spawn([&] { 
    connector.connect(Ipv4Address("127.0.0.1"), 6666); 
  });
  event.wait();
  dispatcher.yield();
}

TEST_F(TcpConnectorTests, tcpConnectorInterruptAfterStart) {
  contextGroup.spawn([&] { 
    ASSERT_THROW(TcpConnector(dispatcher).connect(Ipv4Address("127.0.0.1"), 6666), InterruptedException); 
  });
  contextGroup.interrupt();
}

TEST_F(TcpConnectorTests, tcpConnectorInterrupt) {
  TcpConnector connector(dispatcher);
  contextGroup.spawn([&]() {
    Timer(dispatcher).sleep(std::chrono::milliseconds(10));
    contextGroup.interrupt();
    event.set();
  });

  contextGroup.spawn([&] { 
    ASSERT_THROW(connector.connect(Ipv4Address("10.255.255.1"), 6666), InterruptedException); 
  });
  contextGroup.wait();
}

TEST_F(TcpConnectorTests, tcpConnectorUseAfterInterrupt) {
  TcpConnector connector(dispatcher);
  contextGroup.spawn([&]() {
    Timer(dispatcher).sleep(std::chrono::milliseconds(10));
    contextGroup.interrupt();
    event.set();
  });

  contextGroup.spawn([&] { 
    ASSERT_THROW(connector.connect(Ipv4Address("10.255.255.1"), 6666), InterruptedException); 
  });
  contextGroup.wait();
  contextGroup.spawn([&] { 
    ASSERT_NO_THROW(connector.connect(Ipv4Address("127.0.0.1"), 6666)); 
  });
  contextGroup.wait();
}

TEST_F(TcpConnectorTests, bindToTheSameAddressFails) {
  ASSERT_THROW(TcpListener listener2(dispatcher, Ipv4Address("127.0.0.1"), 6666), std::runtime_error);
}
