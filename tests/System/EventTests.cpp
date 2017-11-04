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
#include <gtest/gtest.h>

using namespace System;

TEST(EventTests, newEventIsNotSet) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  ASSERT_FALSE(event.get());
}

TEST(EventTests, eventIsWorking) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, movedEventIsWorking) {
  Dispatcher dispatcher;
  Event event{Event(dispatcher)};
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, movedEventKeepsState) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  Event event2(std::move(event));
  ASSERT_TRUE(event2.get());
}

TEST(EventTests, movedEventIsWorking2) {
  Dispatcher dispatcher;
  Event srcEvent(dispatcher);
  Event event;
  event = std::move(srcEvent);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, movedEventKeepsState2) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  Event dstEvent;
  dstEvent = std::move(event);
  ASSERT_TRUE(dstEvent.get());
}

TEST(EventTests, moveClearsEventState) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
  Event srcEvent(dispatcher);
  event = std::move(srcEvent);
  ASSERT_FALSE(event.get());
}

TEST(EventTests, movedEventIsTheSame) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto eventPtr1 = &event;
  Event srcEvent(dispatcher);
  event = std::move(srcEvent);
  auto eventPtr2 = &event;
  ASSERT_EQ(eventPtr1, eventPtr2);
}

TEST(EventTests, eventIsWorkingAfterClear) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  event.clear();
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, eventIsWorkingAfterClearOnWaiting) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.clear();
    event.set();
  });

  event.wait();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, eventIsReusableAfterClear) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
    dispatcher.yield();
    event.set();
  });

  event.wait();
  event.clear();
  event.wait();
  SUCCEED();
}

TEST(EventTests, eventSetIsWorkingOnNewEvent) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  event.set();
  ASSERT_TRUE(event.get());
}

TEST(EventTests, setActuallySets) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Context<> context(dispatcher, [&]() {
    event.set();
  });

  event.wait();
  SUCCEED();
}

TEST(EventTests, setJustSets) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  bool done = false;
  Context<> context(dispatcher, [&]() {
    event.wait();
    done = true;
  });

  dispatcher.yield();
  ASSERT_FALSE(done);
  event.set();
  ASSERT_FALSE(done);
  dispatcher.yield();
  ASSERT_TRUE(done);
}

TEST(EventTests, setSetsOnlyOnce) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    event.set();
    event.set();
    event.set();
    dispatcher.yield();
    i++;
  });

  event.wait();
  i++;
  event.wait();
  ASSERT_EQ(i, 1);
  dispatcher.yield();
  ASSERT_EQ(i, 2);
}

TEST(EventTests, waitIsWaiting) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  bool done = false;
  Context<> context(dispatcher, [&]() {
    event.wait();
    done = true;
  });

  dispatcher.yield();
  ASSERT_FALSE(done);
  event.set();
  dispatcher.yield();
  ASSERT_TRUE(done);
}

TEST(EventTests, setEventIsNotWaiting) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    event.set();
    dispatcher.yield();
    i++;
  });

  event.wait();
  i++;
  ASSERT_EQ(i, 1);
  event.wait();
  ASSERT_EQ(i, 1);
  dispatcher.yield();
  ASSERT_EQ(i, 2);
}

TEST(EventTests, waitIsParallel) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    i++;
    event.set();
  });

  ASSERT_EQ(i, 0);
  event.wait();
  ASSERT_EQ(i, 1);
}

TEST(EventTests, waitIsMultispawn) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    event.wait();
    i++;
  });

  Context<> contextSecond(dispatcher, [&]() {
    event.wait();
    i++;
  });

  ASSERT_EQ(i, 0);
  dispatcher.yield();
  ASSERT_EQ(i, 0);
  event.set();
  dispatcher.yield();
  ASSERT_EQ(i, 2);
}

TEST(EventTests, setEventInPastUnblocksWaitersEvenAfterClear) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    event.wait();
    i++;
  });

  Context<> contextSecond(dispatcher, [&]() {
    event.wait();
    i++;
  });

  dispatcher.yield();
  ASSERT_EQ(i, 0);
  event.set();
  event.clear();
  dispatcher.yield();
  ASSERT_EQ(i, 2);
}

TEST(EventTests, waitIsInterruptibleOnFront) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  bool interrupted = false;
  Context<>(dispatcher, [&] {
    try {
      event.wait();
    } catch (InterruptedException&) {
      interrupted = true;
    }
  });
  
  ASSERT_TRUE(interrupted);  
}

TEST(EventTests, waitIsInterruptibleOnBody) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  Event event2(dispatcher);
  bool interrupted = false;
  Context<> context(dispatcher, [&] {
    try {
      event2.set();
      event.wait();
    } catch (InterruptedException&) {
      interrupted = true;
    }
  });

  event2.wait();
  context.interrupt();
  context.get();
  ASSERT_TRUE(interrupted);
}
