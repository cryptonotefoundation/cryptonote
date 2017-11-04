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

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <queue>

namespace System {

struct NativeContextGroup;

struct NativeContext {
  void* fiber;
  bool interrupted;
  NativeContext* next;
  NativeContextGroup* group;
  NativeContext* groupPrev;
  NativeContext* groupNext;
  std::function<void()> procedure;
  std::function<void()> interruptProcedure;
};

struct NativeContextGroup {
  NativeContext* firstContext;
  NativeContext* lastContext;
  NativeContext* firstWaiter;
  NativeContext* lastWaiter;
};

class Dispatcher {
public:
  Dispatcher();
  Dispatcher(const Dispatcher&) = delete;
  ~Dispatcher();
  Dispatcher& operator=(const Dispatcher&) = delete;
  void clear();
  void dispatch();
  NativeContext* getCurrentContext() const;
  void interrupt();
  void interrupt(NativeContext* context);
  bool interrupted();
  void pushContext(NativeContext* context);
  void remoteSpawn(std::function<void()>&& procedure);
  void yield();

  // Platform-specific
  void addTimer(uint64_t time, NativeContext* context);
  void* getCompletionPort() const;
  NativeContext& getReusableContext();
  void pushReusableContext(NativeContext&);
  void interruptTimer(uint64_t time, NativeContext* context);

private:
  void spawn(std::function<void()>&& procedure);
  void* completionPort;
  uint8_t criticalSection[2 * sizeof(long) + 4 * sizeof(void*)];
  bool remoteNotificationSent;
  std::queue<std::function<void()>> remoteSpawningProcedures;
  uint8_t remoteSpawnOverlapped[4 * sizeof(void*)];
  uint32_t threadId;
  std::multimap<uint64_t, NativeContext*> timers;

  NativeContext mainContext;
  NativeContextGroup contextGroup;
  NativeContext* currentContext;
  NativeContext* firstResumingContext;
  NativeContext* lastResumingContext;
  NativeContext* firstReusableContext;
  size_t runningContextCount;

  void contextProcedure();
  static void __stdcall contextProcedureStatic(void* context);
};

}
