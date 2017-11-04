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

#include "Event.h"
#include <cassert>
#include <System/Dispatcher.h>
#include <System/InterruptedException.h>

namespace System {

namespace {

struct EventWaiter {
  bool interrupted;
  EventWaiter* prev;
  EventWaiter* next;
  NativeContext* context;
};

}

Event::Event() : dispatcher(nullptr) {
}

Event::Event(Dispatcher& dispatcher) : dispatcher(&dispatcher), state(false), first(nullptr) {
}

Event::Event(Event&& other) : dispatcher(other.dispatcher) {
  if (dispatcher != nullptr) {
    state = other.state;
    if (!state) {
      assert(other.first == nullptr);
      first = nullptr;
    }

    other.dispatcher = nullptr;
  }
}

Event::~Event() {
  assert(dispatcher == nullptr || state || first == nullptr);
}

Event& Event::operator=(Event&& other) {
  assert(dispatcher == nullptr || state || first == nullptr);
  dispatcher = other.dispatcher;
  if (dispatcher != nullptr) {
    state = other.state;
    if (!state) {
      assert(other.first == nullptr);
      first = nullptr;
    }

    other.dispatcher = nullptr;
  }

  return *this;
}

bool Event::get() const {
  assert(dispatcher != nullptr);
  return state;
}

void Event::clear() {
  assert(dispatcher != nullptr);
  if (state) {
    state = false;
    first = nullptr;
  }
}

void Event::set() {
  assert(dispatcher != nullptr);
  if (!state) {
    state = true;
    for (EventWaiter* waiter = static_cast<EventWaiter*>(first); waiter != nullptr; waiter = waiter->next) {
      waiter->context->interruptProcedure = nullptr;
      dispatcher->pushContext(waiter->context);
    }
  }
}

void Event::wait() {
  assert(dispatcher != nullptr);
  if (dispatcher->interrupted()) {
    throw InterruptedException();
  }

  if (!state) {
    EventWaiter waiter = { false, nullptr, nullptr, dispatcher->getCurrentContext() };
    waiter.context->interruptProcedure = [&] {
      if (waiter.next != nullptr) {
        assert(waiter.next->prev == &waiter);
        waiter.next->prev = waiter.prev;
      } else {
        assert(last == &waiter);
        last = waiter.prev;
      }

      if (waiter.prev != nullptr) { 
        assert(waiter.prev->next == &waiter);
        waiter.prev->next = waiter.next;
      } else {
        assert(first == &waiter);
        first = waiter.next;
      }

      assert(!waiter.interrupted);
      waiter.interrupted = true;
      dispatcher->pushContext(waiter.context);
    };

    if (first != nullptr) {
      static_cast<EventWaiter*>(last)->next = &waiter;
      waiter.prev = static_cast<EventWaiter*>(last);
    } else {
      first = &waiter;
    }

    last = &waiter;
    dispatcher->dispatch();
    assert(waiter.context == dispatcher->getCurrentContext());
    assert( waiter.context->interruptProcedure == nullptr);
    assert(dispatcher != nullptr);
    if (waiter.interrupted) {
      throw InterruptedException();
    } 
  }
}

}
