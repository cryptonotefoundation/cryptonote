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

#include <System/RemoteContext.h>
#include <System/Dispatcher.h>
#include <System/ContextGroup.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

class RemoteContextTests : public testing::Test {
public:
  Dispatcher dispatcher;
};


TEST_F(RemoteContextTests, getReturnsResult) {
  RemoteContext<int> context(dispatcher, [&] { 
    return 2; 
  });

  ASSERT_EQ(2, context.get());
}

TEST_F(RemoteContextTests, getRethrowsException) {
  RemoteContext<> context(dispatcher, [&] {
    throw std::string("Hi there!"); 
  });

  ASSERT_THROW(context.get(), std::string);
}

TEST_F(RemoteContextTests, destructorIgnoresException) {
  ASSERT_NO_THROW(RemoteContext<>(dispatcher, [&] {
    throw std::string("Hi there!");
  }));
}

TEST_F(RemoteContextTests, canBeUsedWithoutObject) {
  ASSERT_EQ(42, RemoteContext<int>(dispatcher, [&] { return 42; }).get());
}

TEST_F(RemoteContextTests, interruptIsInterruptingWait) {
  ContextGroup cg(dispatcher);
  bool started = false;

  cg.spawn([&] {
    RemoteContext<> context(dispatcher, [&] {
      started = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
    ASSERT_NO_THROW(context.wait());
    ASSERT_TRUE(dispatcher.interrupted());
  });

  cg.interrupt();
  cg.wait();

  ASSERT_TRUE(started);
}

TEST_F(RemoteContextTests, interruptIsInterruptingGet) {
  ContextGroup cg(dispatcher);
  cg.spawn([&] {
    RemoteContext<> context(dispatcher, [&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
    ASSERT_NO_THROW(context.get());
    ASSERT_TRUE(dispatcher.interrupted());
  });

  cg.interrupt();
  cg.wait();
}

TEST_F(RemoteContextTests, destructorIgnoresInterrupt) {
  ContextGroup cg(dispatcher);
  cg.spawn([&] {
    ASSERT_NO_THROW(RemoteContext<>(dispatcher, [&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }));
  });

  cg.interrupt();
  cg.wait();
}

TEST_F(RemoteContextTests, canExecuteOtherContextsWhileWaiting) {
  auto start = std::chrono::high_resolution_clock::now();
  ContextGroup cg(dispatcher);
  cg.spawn([&] {
    RemoteContext<> context(dispatcher, [&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });
  });
  cg.spawn([&] {
    System::Timer(dispatcher).sleep(std::chrono::milliseconds(50));
    auto end = std::chrono::high_resolution_clock::now();
    ASSERT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 50);
    ASSERT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 100);
  });

  cg.wait();
}

TEST_F(RemoteContextTests, waitMethodWaitsForContexCompletion) {
  auto start = std::chrono::high_resolution_clock::now();
  ContextGroup cg(dispatcher);
  cg.spawn([&] {
    RemoteContext<> context(dispatcher, [&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
  });

  cg.wait();
  auto end = std::chrono::high_resolution_clock::now();
  ASSERT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 10);
}

TEST_F(RemoteContextTests, waitMethodWaitsForContexCompletionOnInterrupt) {
  auto start = std::chrono::high_resolution_clock::now();
  ContextGroup cg(dispatcher);
  cg.spawn([&] {
    RemoteContext<> context(dispatcher, [&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
  });

  cg.interrupt();
  cg.wait();
  auto end = std::chrono::high_resolution_clock::now();
  ASSERT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 10);
}

