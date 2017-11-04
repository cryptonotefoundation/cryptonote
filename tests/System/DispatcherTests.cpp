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

#include <future>
#include <System/Context.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

class DispatcherTests : public testing::Test {
public:
  Dispatcher dispatcher;
};

TEST_F(DispatcherTests, clearRemainsDispatcherWorkable) {
  dispatcher.clear();
  bool spawnDone = false;
  Context<> context(dispatcher, [&]() {
    spawnDone = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}

TEST_F(DispatcherTests, clearRemainsDispatcherWorkableAfterAsyncOperation) {
  bool spawn1Done = false;
  bool spawn2Done = false;
  Context<> context(dispatcher, [&]() {
    spawn1Done = true;
  });
  
  dispatcher.yield();
  ASSERT_TRUE(spawn1Done);
  dispatcher.clear();
  Context<> contextSecond(dispatcher, [&]() {
    spawn2Done = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawn2Done);
}

TEST_F(DispatcherTests, clearCalledFromSpawnRemainsDispatcherWorkable) {
  bool spawn1Done = false;
  bool spawn2Done = false;
  Context<> context(dispatcher, [&]() {
    dispatcher.clear();
    spawn1Done = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawn1Done);
  Context<> contextSecond(dispatcher, [&]() {
    spawn2Done = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawn2Done);
}

TEST_F(DispatcherTests, timerIsHandledOnlyAfterAllSpawnedTasksAreHandled) {
  Event event1(dispatcher);
  Event event2(dispatcher);
  Context<> context(dispatcher, [&]() {
    event1.set();
    Timer(dispatcher).sleep(std::chrono::milliseconds(1));
    event2.set();
  });

  dispatcher.yield();
  ASSERT_TRUE(event1.get());
  ASSERT_FALSE(event2.get());
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  ASSERT_FALSE(event2.get());
  dispatcher.yield();
  ASSERT_TRUE(event2.get());
}

TEST_F(DispatcherTests, dispatchKeepsSpawnOrder) {
  std::deque<size_t> executionOrder;
  std::deque<size_t> expectedOrder = { 1, 2 };
  Context<> context(dispatcher, [&]() {
    executionOrder.push_back(1);
  });

  Context<> contextSecond(dispatcher, [&]() {
    executionOrder.push_back(2);
  });

  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  ASSERT_EQ(executionOrder, expectedOrder);
}

TEST_F(DispatcherTests, dispatchKeepsSpawnOrderWithNesting) {
  std::deque<size_t> executionOrder;
  std::deque<size_t> expectedOrder = { 1, 2, 3, 4 };
  auto mainContext = dispatcher.getCurrentContext();
  Context<> context(dispatcher, [&]() {
    executionOrder.push_back(1);
    Context<> context(dispatcher, [&]() {
      executionOrder.push_back(3);
    });
  });

  Context<> contextSecond(dispatcher, [&]() {
    executionOrder.push_back(2);
    Context<> context(dispatcher, [&]() {
      executionOrder.push_back(4);
      dispatcher.pushContext(mainContext);
    });
  });

  dispatcher.dispatch();
  ASSERT_EQ(executionOrder, expectedOrder);
}

TEST_F(DispatcherTests, dispatchKeepsSpawnResumingOrder) {
  std::deque<size_t> executionOrder;
  std::deque<size_t> expectedOrder = { 1, 2, 3, 4 };
  std::vector<NativeContext*> contexts;
  Context<> context(dispatcher, [&]() {
    executionOrder.push_back(1);
    contexts.push_back(dispatcher.getCurrentContext());
    dispatcher.dispatch();
    executionOrder.push_back(3);
  });

  Context<> contextSecond(dispatcher, [&]() {
    executionOrder.push_back(2);
    contexts.push_back(dispatcher.getCurrentContext());
    dispatcher.dispatch();
    executionOrder.push_back(4);
  });

  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  for (auto& ctx : contexts) {
    dispatcher.pushContext(ctx);
  }

  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  ASSERT_EQ(executionOrder, expectedOrder);
}

TEST_F(DispatcherTests, getCurrentContextDiffersForParallelSpawn) {
  void* ctx1 = nullptr;
  void* ctx2 = nullptr;
  Context<> context(dispatcher, [&]() {
    ctx1 = dispatcher.getCurrentContext();
  });

  Context<> contextSecond(dispatcher, [&]() {
    ctx2 = dispatcher.getCurrentContext();
  });

  dispatcher.yield();
  ASSERT_NE(ctx1, nullptr);
  ASSERT_NE(ctx2, nullptr);
  ASSERT_NE(ctx1, ctx2);
}

TEST_F(DispatcherTests, getCurrentContextSameForSequentialSpawn) {
  void* ctx1 = nullptr;
  void* ctx2 = nullptr;
  Context<> context(dispatcher, [&]() {
    ctx1 = dispatcher.getCurrentContext();
    dispatcher.yield();
    ctx2 = dispatcher.getCurrentContext();
  });

  dispatcher.yield();
  dispatcher.yield();
  ASSERT_NE(ctx1, nullptr);
  ASSERT_EQ(ctx1, ctx2);
}

TEST_F(DispatcherTests, pushedContextMustGoOn) {
  bool spawnDone = false;
  Context<> context(dispatcher, [&]() {
    spawnDone = true;
  });

  dispatcher.pushContext(dispatcher.getCurrentContext());
  dispatcher.dispatch();
  ASSERT_TRUE(spawnDone);
}

TEST_F(DispatcherTests, pushedContextMustGoOnFromNestedSpawns) {
  bool spawnDone = false;
  auto mainContext = dispatcher.getCurrentContext();
  Context<> context(dispatcher, [&]() {
    spawnDone = true;
    dispatcher.pushContext(mainContext);
  });

  dispatcher.dispatch();
  ASSERT_TRUE(spawnDone);
}

TEST_F(DispatcherTests, remoteSpawnActuallySpawns) {
  Event remoteSpawnDone(dispatcher);
  auto remoteSpawnThread = std::thread([&] {
    dispatcher.remoteSpawn([&]() {
      remoteSpawnDone.set();
    });
  });

  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }

  dispatcher.yield();
  ASSERT_TRUE(remoteSpawnDone.get());
}

TEST_F(DispatcherTests, remoteSpawnActuallySpawns2) {
  Event remoteSpawnDone(dispatcher);
  auto remoteSpawnThread = std::thread([&] { 
    dispatcher.remoteSpawn([&]() { 
      remoteSpawnDone.set(); 
    }); 
  });

  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }

  Timer(dispatcher).sleep(std::chrono::milliseconds(3));
  ASSERT_TRUE(remoteSpawnDone.get());
}

TEST_F(DispatcherTests, remoteSpawnActuallySpawns3) {
  Event remoteSpawnDone(dispatcher);
  auto mainCtx = dispatcher.getCurrentContext();
  auto remoteSpawnThread = std::thread([&, this] {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    dispatcher.remoteSpawn([&, this]() {
      remoteSpawnDone.set();
      dispatcher.pushContext(mainCtx);
    });
  });

  dispatcher.dispatch();
  ASSERT_TRUE(remoteSpawnDone.get());
  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }
}

TEST_F(DispatcherTests, remoteSpawnSpawnsProcedureInDispatcherThread) {
  Event remoteSpawnDone(dispatcher);
  auto mainSpawnThrId = std::this_thread::get_id();
  decltype(mainSpawnThrId) remoteSpawnThrId;
  auto remoteSpawnThread = std::thread([&] {
    dispatcher.remoteSpawn([&]() {
      remoteSpawnThrId = std::this_thread::get_id();
      remoteSpawnDone.set();
    });
  });

  remoteSpawnDone.wait();
  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }

  ASSERT_EQ(mainSpawnThrId, remoteSpawnThrId);
}

TEST_F(DispatcherTests, remoteSpawnSpawnsProcedureAndKeepsOrder) {
  Event remoteSpawnDone(dispatcher);
  std::deque<size_t> executionOrder;
  std::deque<size_t> expectedOrder = { 1, 2 };
  auto remoteSpawnThread = std::thread([&] {
    dispatcher.remoteSpawn([&]() {
      executionOrder.push_back(1);
    });

    dispatcher.remoteSpawn([&]() {
      executionOrder.push_back(2);
      remoteSpawnDone.set();
    });
  });

  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }

  remoteSpawnDone.wait();
  ASSERT_EQ(executionOrder, expectedOrder);
}

TEST_F(DispatcherTests, remoteSpawnActuallyWorksParallel) {
  Event remoteSpawnDone(dispatcher);
  auto remoteSpawnThread = std::thread([&] {
    dispatcher.remoteSpawn([&]() {
      remoteSpawnDone.set();
    });
  });

  Timer(dispatcher).sleep(std::chrono::milliseconds(100));
  ASSERT_TRUE(remoteSpawnDone.get());

  if (remoteSpawnThread.joinable()) {
    remoteSpawnThread.join();
  }
}

TEST_F(DispatcherTests, spawnActuallySpawns) {
  bool spawnDone = false;
  Context<> context(dispatcher, [&]() {
    spawnDone = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}

TEST_F(DispatcherTests, spawnJustSpawns) {
  bool spawnDone = false;
  Context<> context(dispatcher, [&]() {
    spawnDone = true;
  });

  ASSERT_FALSE(spawnDone);
  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}

TEST_F(DispatcherTests, yieldReturnsIfNothingToSpawn) {
  dispatcher.yield();
}

TEST_F(DispatcherTests, yieldReturnsAfterExecutionOfSpawnedProcedures) {
  bool spawnDone = false;
  Context<> context(dispatcher, [&]() {
    spawnDone = true;
  });

  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}

TEST_F(DispatcherTests, yieldReturnsAfterExecutionOfIO) {
  Context<> context(dispatcher, [&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    dispatcher.yield();
  });

  Timer(dispatcher).sleep(std::chrono::milliseconds(1));
  dispatcher.yield();
  SUCCEED();
}

TEST_F(DispatcherTests, yieldExecutesIoOnItsFront) {
  bool spawnDone = false;
  Context<> context(dispatcher, [&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    dispatcher.yield();
    spawnDone = true;
  });

  Timer(dispatcher).sleep(std::chrono::milliseconds(1));
  ASSERT_FALSE(spawnDone);
  dispatcher.yield();
  ASSERT_TRUE(spawnDone);
}
