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

#include <System/Ipv4Address.h>
#include <gtest/gtest.h>

using namespace System;

TEST(Ipv4AddressTest, value) {
  Ipv4Address address1(0x00000000);
  ASSERT_EQ(0x00000000, address1.getValue());
  Ipv4Address address2(0xfefdfcfb);
  ASSERT_EQ(0xfefdfcfb, address2.getValue());
  Ipv4Address address3 = address1;
  ASSERT_EQ(0x00000000, address3.getValue());
  Ipv4Address address4 = address2;
  ASSERT_EQ(0xfefdfcfb, address4.getValue());
  address3 = address2;
  ASSERT_EQ(0xfefdfcfb, address3.getValue());
  address4 = address1;
  ASSERT_EQ(0x00000000, address4.getValue());
}

TEST(Ipv4AddressTest, dottedDecimal) {
  ASSERT_EQ(0x00000000, Ipv4Address("0.0.0.0").getValue());
  ASSERT_EQ(0x01020304, Ipv4Address("1.2.3.4").getValue());
  ASSERT_EQ(0x7f000001, Ipv4Address("127.0.0.1").getValue());
  ASSERT_EQ(0xfefdfcfb, Ipv4Address("254.253.252.251").getValue());
  ASSERT_EQ(0xffffffff, Ipv4Address("255.255.255.255").getValue());
  ASSERT_EQ("0.0.0.0", Ipv4Address(0x00000000).toDottedDecimal());
  ASSERT_EQ("1.2.3.4", Ipv4Address(0x01020304).toDottedDecimal());
  ASSERT_EQ("127.0.0.1", Ipv4Address(0x7f000001).toDottedDecimal());
  ASSERT_EQ("254.253.252.251", Ipv4Address(0xfefdfcfb).toDottedDecimal());
  ASSERT_EQ("255.255.255.255", Ipv4Address(0xffffffff).toDottedDecimal());
  ASSERT_THROW(Ipv4Address(".0.0.0.0"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("0..0.0.0"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("0.0.0"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("0.0.0."), std::runtime_error);
  ASSERT_THROW(Ipv4Address("0.0.0.0."), std::runtime_error);
  ASSERT_THROW(Ipv4Address("0.0.0.0.0"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("0.0.0.00"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("0.0.0.01"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("0.0.0.256"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("00.0.0.0"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("01.0.0.0"), std::runtime_error);
  ASSERT_THROW(Ipv4Address("256.0.0.0"), std::runtime_error);
}

TEST(Ipv4AddressTest, isLoopback) {
  // 127.0.0.0/8
  ASSERT_TRUE(Ipv4Address("127.0.0.1").isLoopback());
  ASSERT_TRUE(Ipv4Address("127.1.1.1").isLoopback());
  ASSERT_TRUE(Ipv4Address("127.1.0.0").isLoopback());
  ASSERT_TRUE(Ipv4Address("127.255.255.255").isLoopback());

  ASSERT_FALSE(Ipv4Address("255.0.0.0").isLoopback());
  ASSERT_FALSE(Ipv4Address("255.255.255.255").isLoopback());
  ASSERT_FALSE(Ipv4Address("128.1.0.0").isLoopback());
  ASSERT_FALSE(Ipv4Address("192.168.1.1").isLoopback());
  ASSERT_FALSE(Ipv4Address("10.0.0.1").isLoopback());
}

TEST(Ipv4AddressTest, isPrivate) {
  // 10.0.0.0/8
  ASSERT_TRUE(Ipv4Address("10.0.0.0").isPrivate());
  ASSERT_TRUE(Ipv4Address("10.0.0.1").isPrivate());
  ASSERT_TRUE(Ipv4Address("10.0.0.255").isPrivate());
  ASSERT_TRUE(Ipv4Address("10.255.255.255").isPrivate());

  ASSERT_FALSE(Ipv4Address("11.0.0.255").isPrivate());
  ASSERT_FALSE(Ipv4Address("9.0.0.0").isPrivate());
  ASSERT_FALSE(Ipv4Address("138.0.0.1").isPrivate());
  ASSERT_FALSE(Ipv4Address("255.255.255.255").isPrivate());

  // 172.16.0.0/12 
  ASSERT_TRUE(Ipv4Address("172.16.0.255").isPrivate());
  ASSERT_TRUE(Ipv4Address("172.17.0.0").isPrivate());
  ASSERT_TRUE(Ipv4Address("172.19.1.1").isPrivate());
  ASSERT_TRUE(Ipv4Address("172.31.255.255").isPrivate());

  ASSERT_FALSE(Ipv4Address("172.32.0.0").isPrivate());
  ASSERT_FALSE(Ipv4Address("172.32.0.1").isPrivate());
  ASSERT_FALSE(Ipv4Address("172.15.0.0").isPrivate());
  ASSERT_FALSE(Ipv4Address("172.15.255.255").isPrivate());

  // 192.168.0.0/16
  ASSERT_TRUE(Ipv4Address("192.168.0.0").isPrivate());
  ASSERT_TRUE(Ipv4Address("192.168.1.1").isPrivate());
  ASSERT_TRUE(Ipv4Address("192.168.100.100").isPrivate());
  ASSERT_TRUE(Ipv4Address("192.168.255.255").isPrivate());

  ASSERT_FALSE(Ipv4Address("192.167.255.255").isPrivate());
  ASSERT_FALSE(Ipv4Address("191.168.255.255").isPrivate());
  ASSERT_FALSE(Ipv4Address("192.169.255.255").isPrivate());
  ASSERT_FALSE(Ipv4Address("192.169.0.0").isPrivate());

  ASSERT_FALSE(Ipv4Address("255.255.255.255").isPrivate());

}
