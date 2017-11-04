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

#include <System/Context.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

TEST(ContextTests, getReturnsResult) {
  Dispatcher dispatcher;
  Context<int> context(dispatcher, [&] { 
    return 2; 
  });

  ASSERT_EQ(2, context.get());
}

TEST(ContextTests, getRethrowsException) {
  Dispatcher dispatcher;
  Context<> context(dispatcher, [&] {
    throw std::string("Hi there!"); 
  });

  ASSERT_THROW(context.get(), std::string);
}

TEST(ContextTests, destructorIgnoresException) {
  Dispatcher dispatcher;
  ASSERT_NO_THROW(Context<>(dispatcher, [&] {
    throw std::string("Hi there!");
  }));
}

TEST(ContextTests, interruptIsInterrupting) {
  Dispatcher dispatcher;
  Context<> context(dispatcher, [&] {
    if (dispatcher.interrupted()) {
      throw InterruptedException();
    }
  });

  context.interrupt();
  ASSERT_THROW(context.get(), InterruptedException);
}

TEST(ContextTests, getChecksInterruption) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<int> context1(dispatcher, [&] {
    event.wait();
    if (dispatcher.interrupted()) {
      return 11;
    }

    return 10;
  });

  Context<int> context2(dispatcher, [&] {
    event.set();
    return context1.get();
  });

  context2.interrupt();
  ASSERT_EQ(11, context2.get());
}

TEST(ContextTests, getIsInterruptible) {
  Dispatcher dispatcher;
  Event event1(dispatcher);
  Event event2(dispatcher);
  Context<int> context1(dispatcher, [&] {
    event2.wait();
    if (dispatcher.interrupted()) {
      return 11;
    }

    return 10;
  });

  Context<int> context2(dispatcher, [&] {
    event1.set();
    return context1.get();
  });

  event1.wait();
  context2.interrupt();
  event2.set();
  ASSERT_EQ(11, context2.get());
}

TEST(ContextTests, destructorInterrupts) {
  Dispatcher dispatcher;
  bool interrupted = false;
  {
    Context<> context(dispatcher, [&] {
      if (dispatcher.interrupted()) {
        interrupted = true;
      }
    });
  }

  ASSERT_TRUE(interrupted);
}
