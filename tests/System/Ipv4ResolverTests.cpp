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
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/Ipv4Resolver.h>
#include <gtest/gtest.h>

using namespace System;

class Ipv4ResolverTests : public testing::Test {
public:
  Ipv4ResolverTests() : contextGroup(dispatcher), resolver(dispatcher) {
  }

  Dispatcher dispatcher;
  ContextGroup contextGroup;
  Ipv4Resolver resolver;
};

TEST_F(Ipv4ResolverTests, start) {
  contextGroup.spawn([&] { 
    ASSERT_NO_THROW(Ipv4Resolver(dispatcher).resolve("localhost")); 
  });
  contextGroup.wait();
}

TEST_F(Ipv4ResolverTests, stop) {
  contextGroup.spawn([&] {
    contextGroup.interrupt();
    ASSERT_THROW(resolver.resolve("localhost"), InterruptedException);
  });
  contextGroup.wait();
}

TEST_F(Ipv4ResolverTests, interruptWhileResolving) {
  contextGroup.spawn([&] {
    ASSERT_THROW(resolver.resolve("localhost"), InterruptedException);
  });
  contextGroup.interrupt();
  contextGroup.wait();
}

TEST_F(Ipv4ResolverTests, reuseAfterInterrupt) {
  contextGroup.spawn([&] {
    ASSERT_THROW(resolver.resolve("localhost"), InterruptedException);
  });
  contextGroup.interrupt();
  contextGroup.wait();
  contextGroup.spawn([&] {
    ASSERT_NO_THROW(resolver.resolve("localhost"));
  });
  contextGroup.wait();
}

TEST_F(Ipv4ResolverTests, resolve) {
  ASSERT_EQ(Ipv4Address("0.0.0.0"), resolver.resolve("0.0.0.0"));
  ASSERT_EQ(Ipv4Address("1.2.3.4"), resolver.resolve("1.2.3.4"));
  ASSERT_EQ(Ipv4Address("127.0.0.1"), resolver.resolve("127.0.0.1"));
  ASSERT_EQ(Ipv4Address("254.253.252.251"), resolver.resolve("254.253.252.251"));
  ASSERT_EQ(Ipv4Address("255.255.255.255"), resolver.resolve("255.255.255.255"));
  ASSERT_EQ(Ipv4Address("127.0.0.1"), resolver.resolve("localhost"));
//ASSERT_EQ(Ipv4Address("93.184.216.34"), resolver.resolve("example.com"));
  ASSERT_THROW(resolver.resolve(".0.0.0.0"), std::runtime_error);
  ASSERT_THROW(resolver.resolve("0..0.0.0"), std::runtime_error);
//ASSERT_THROW(resolver.resolve("0.0.0"), std::runtime_error);
  ASSERT_THROW(resolver.resolve("0.0.0."), std::runtime_error);
//ASSERT_THROW(resolver.resolve("0.0.0.0."), std::runtime_error);
  ASSERT_THROW(resolver.resolve("0.0.0.0.0"), std::runtime_error);
//ASSERT_THROW(resolver.resolve("0.0.0.00"), std::runtime_error);
//ASSERT_THROW(resolver.resolve("0.0.0.01"), std::runtime_error);
  ASSERT_THROW(resolver.resolve("0.0.0.256"), std::runtime_error);
//ASSERT_THROW(resolver.resolve("00.0.0.0"), std::runtime_error);
//ASSERT_THROW(resolver.resolve("01.0.0.0"), std::runtime_error);
  ASSERT_THROW(resolver.resolve("256.0.0.0"), std::runtime_error);
  ASSERT_THROW(resolver.resolve("invalid"), std::runtime_error);
}
