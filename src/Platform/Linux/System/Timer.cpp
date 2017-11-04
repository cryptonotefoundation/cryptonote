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

#include "Timer.h"
#include <cassert>
#include <stdexcept>

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "Dispatcher.h"
#include <System/ErrorMessage.h>
#include <System/InterruptedException.h>

namespace System {

Timer::Timer() : dispatcher(nullptr) {
}

Timer::Timer(Dispatcher& dispatcher) : dispatcher(&dispatcher), context(nullptr), timer(-1) {
}

Timer::Timer(Timer&& other) : dispatcher(other.dispatcher) {
  if (other.dispatcher != nullptr) {
    assert(other.context == nullptr);
    timer = other.timer;
    context = nullptr;
    other.dispatcher = nullptr;
  }
}

Timer::~Timer() {
  assert(dispatcher == nullptr || context == nullptr);
}

Timer& Timer::operator=(Timer&& other) {
  assert(dispatcher == nullptr || context == nullptr);
  dispatcher = other.dispatcher;
  if (other.dispatcher != nullptr) {
    assert(other.context == nullptr);
    timer = other.timer;
    context = nullptr;
    other.dispatcher = nullptr;
    other.timer = -1;
  }

  return *this;
}

void Timer::sleep(std::chrono::nanoseconds duration) {
  assert(dispatcher != nullptr);
  assert(context == nullptr);
  if (dispatcher->interrupted()) {
    throw InterruptedException();
  }

  if(duration.count() == 0 ) {
    dispatcher->yield();
  } else {
    timer = dispatcher->getTimer();

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    itimerspec expires;
    expires.it_interval.tv_nsec = expires.it_interval.tv_sec = 0;
    expires.it_value.tv_sec = seconds.count();
    expires.it_value.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds).count();
    timerfd_settime(timer, 0, &expires, NULL);

    ContextPair contextPair;
    OperationContext timerContext;
    timerContext.interrupted = false;
    timerContext.context = dispatcher->getCurrentContext();
    contextPair.writeContext = nullptr;
    contextPair.readContext = &timerContext;

    epoll_event timerEvent;
    timerEvent.events = EPOLLIN | EPOLLONESHOT;
    timerEvent.data.ptr = &contextPair;

    if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, timer, &timerEvent) == -1) {
      throw std::runtime_error("Timer::sleep, epoll_ctl failed, " + lastErrorMessage());
    }
    dispatcher->getCurrentContext()->interruptProcedure = [&]() {
        assert(dispatcher != nullptr);
        assert(context != nullptr);
        OperationContext* timerContext = static_cast<OperationContext*>(context);
        if (!timerContext->interrupted) {
          uint64_t value = 0;
          if(::read(timer, &value, sizeof value) == -1 ){
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
              timerContext->interrupted = true;
              dispatcher->pushContext(timerContext->context);
            } else {
              throw std::runtime_error("Timer::interrupt, read failed, "  + lastErrorMessage());
            }
          } else {
            assert(value>0);
            dispatcher->pushContext(timerContext->context);
          }

          epoll_event timerEvent;
          timerEvent.events = 0;
          timerEvent.data.ptr = nullptr;

          if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, timer, &timerEvent) == -1) {
            throw std::runtime_error("Timer::interrupt, epoll_ctl failed, " + lastErrorMessage());
          }
        }
    };

    context = &timerContext;
    dispatcher->dispatch();
    dispatcher->getCurrentContext()->interruptProcedure = nullptr;
    assert(dispatcher != nullptr);
    assert(timerContext.context == dispatcher->getCurrentContext());
    assert(contextPair.writeContext == nullptr);
    assert(context == &timerContext);
    context = nullptr;
    timerContext.context = nullptr;
    dispatcher->pushTimer(timer);
    if (timerContext.interrupted) {
      throw InterruptedException();
    }
  }
}

}
