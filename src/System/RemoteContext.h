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

#include <cassert>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/Future.h>
#include <System/InterruptedException.h>

namespace System {

template<class T = void> class RemoteContext {
public:
  // Start a thread, execute operation in it, continue execution of current context.
  RemoteContext(Dispatcher& d, std::function<T()>&& operation)
      : dispatcher(d), event(d), procedure(std::move(operation)), future(System::Detail::async<T>([this] { return asyncProcedure(); })), interrupted(false) {
  }

  // Run other task on dispatcher until future is ready, then return lambda's result, or rethrow exception. UB if called more than once.
  T get() const {
    wait();
    return future.get();
  }

  // Run other task on dispatcher until future is ready.
  void wait() const {
    while (!event.get()) {
      try {
        event.wait();
      } catch (InterruptedException&) {
        interrupted = true;
      }
    }

    if (interrupted) {
      dispatcher.interrupt();
    }
  }

  // Wait future to complete.
  ~RemoteContext() {
    try {
      wait();
    } catch (std::exception&) {
    }

    try {
      // windows future implementation doesn't wait for completion on destruction
      if (future.valid()) {
        future.wait();
      }
    } catch (std::exception&) {
    }
  }

private:
  struct NotifyOnDestruction {
    NotifyOnDestruction(Dispatcher& d, Event& e) : dispatcher(d), event(e) {
    }

    ~NotifyOnDestruction() {
      // make a local copy; event reference will be dead when function is called
      auto localEvent = &event;
      // die if this throws...
      dispatcher.remoteSpawn([=] { localEvent->set(); });
    }

    Dispatcher& dispatcher;
    Event& event;
  };

  // This function is executed in future object
  T asyncProcedure() {
    NotifyOnDestruction guard(dispatcher, event);
    assert(procedure != nullptr);
    return procedure();
  }

  Dispatcher& dispatcher;
  mutable Event event;
  std::function<T()> procedure;
  mutable System::Detail::Future<T> future;
  mutable bool interrupted;
};

}
