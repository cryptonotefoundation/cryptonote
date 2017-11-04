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
#include <System/EventLock.h>
#include <gtest/gtest.h>

using namespace System;

TEST(EventLockTests, eventLockIsLocking) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  bool done = false;
  Context<> context(dispatcher, [&]() {
    EventLock lock(event);
    done = true;
  });

  ASSERT_FALSE(done);
  dispatcher.yield();
  ASSERT_FALSE(done);
  event.set();
  dispatcher.yield();
  ASSERT_TRUE(done);
}

TEST(EventLockTests, eventLockIsNotLocking) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  event.set();
  bool done = false;
  Context<> context(dispatcher, [&]() {
    EventLock lock(event);
    done = true;
  });

  ASSERT_FALSE(done);
  dispatcher.yield();
  ASSERT_TRUE(done);
}

TEST(EventLockTests, eventLockIsUnlockOnlyOnce) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    EventLock lock(event);
    i++;
    dispatcher.yield();
    i++;
  });

  Context<> contextSecond(dispatcher, [&]() {
    EventLock lock(event);
    i += 2;
    dispatcher.yield();
    i += 2;
  });
  
  event.set();
  dispatcher.yield();
  ASSERT_EQ(i, 1);
  dispatcher.yield();
  ASSERT_EQ(i, 2);
  dispatcher.yield();
  ASSERT_EQ(i, 4);
  dispatcher.yield();
  ASSERT_EQ(i, 6);
}
