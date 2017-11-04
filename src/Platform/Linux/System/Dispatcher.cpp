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

#include "Dispatcher.h"
#include <cassert>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include "ErrorMessage.h"

namespace System {

namespace {

struct ContextMakingData {
  Dispatcher* dispatcher;
  void* ucontext;
};

class MutextGuard {
public:
  MutextGuard(pthread_mutex_t& _mutex) : mutex(_mutex) {
    auto ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
      throw std::runtime_error("pthread_mutex_lock failed, " + errorMessage(ret));
    }
  }

  ~MutextGuard() {
    pthread_mutex_unlock(&mutex);
  }

private:
  pthread_mutex_t& mutex;
};

static_assert(Dispatcher::SIZEOF_PTHREAD_MUTEX_T == sizeof(pthread_mutex_t), "invalid pthread mutex size");

const size_t STACK_SIZE = 64 * 1024;

};

Dispatcher::Dispatcher() {
  std::string message;
  epoll = ::epoll_create1(0);
  if (epoll == -1) {
    message = "epoll_create1 failed, " + lastErrorMessage();
  } else {
    mainContext.ucontext = new ucontext_t;
    if (getcontext(reinterpret_cast<ucontext_t*>(mainContext.ucontext)) == -1) {
      message = "getcontext failed, " + lastErrorMessage();
    } else {
      remoteSpawnEvent = eventfd(0, O_NONBLOCK);
      if(remoteSpawnEvent == -1) {
        message = "eventfd failed, " + lastErrorMessage();
      } else {
        remoteSpawnEventContext.writeContext = nullptr;
        remoteSpawnEventContext.readContext = nullptr;

        epoll_event remoteSpawnEventEpollEvent;
        remoteSpawnEventEpollEvent.events = EPOLLIN;
        remoteSpawnEventEpollEvent.data.ptr = &remoteSpawnEventContext;

        if (epoll_ctl(epoll, EPOLL_CTL_ADD, remoteSpawnEvent, &remoteSpawnEventEpollEvent) == -1) {
          message = "epoll_ctl failed, " + lastErrorMessage();
        } else {
          *reinterpret_cast<pthread_mutex_t*>(this->mutex) = pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER);

          mainContext.interrupted = false;
          mainContext.group = &contextGroup;
          mainContext.groupPrev = nullptr;
          mainContext.groupNext = nullptr;
          contextGroup.firstContext = nullptr;
          contextGroup.lastContext = nullptr;
          contextGroup.firstWaiter = nullptr;
          contextGroup.lastWaiter = nullptr;
          currentContext = &mainContext;
          firstResumingContext = nullptr;
          firstReusableContext = nullptr;
          runningContextCount = 0;
          return;
        }

        auto result = close(remoteSpawnEvent);
        assert(result == 0);
      }
    }

    auto result = close(epoll);
    assert(result == 0);
  }

  throw std::runtime_error("Dispatcher::Dispatcher, "+message);
}

Dispatcher::~Dispatcher() {
  for (NativeContext* context = contextGroup.firstContext; context != nullptr; context = context->groupNext) {
    interrupt(context);
  }

  yield();
  assert(contextGroup.firstContext == nullptr);
  assert(contextGroup.firstWaiter == nullptr);
  assert(firstResumingContext == nullptr);
  assert(runningContextCount == 0);
  while (firstReusableContext != nullptr) {
    auto ucontext = static_cast<ucontext_t*>(firstReusableContext->ucontext);
    auto stackPtr = static_cast<uint8_t *>(firstReusableContext->stackPtr);
    firstReusableContext = firstReusableContext->next;
    delete[] stackPtr;
    delete ucontext;
  }

  while (!timers.empty()) {
    int result = ::close(timers.top());
    assert(result == 0);
    timers.pop();
  }

  auto result = close(epoll);
  assert(result == 0);
  result = close(remoteSpawnEvent);
  assert(result == 0);
  result = pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(this->mutex));
  assert(result == 0);
}

void Dispatcher::clear() {
  while (firstReusableContext != nullptr) {
    auto ucontext = static_cast<ucontext_t*>(firstReusableContext->ucontext);
    auto stackPtr = static_cast<uint8_t *>(firstReusableContext->stackPtr);
    firstReusableContext = firstReusableContext->next;
    delete[] stackPtr;
    delete ucontext;
  }

  while (!timers.empty()) {
    int result = ::close(timers.top());
    if (result == -1) {
      throw std::runtime_error("Dispatcher::clear, close failed, "  + lastErrorMessage());
    }

    timers.pop();
  }
}

void Dispatcher::dispatch() {
  NativeContext* context;
  for (;;) {
    if (firstResumingContext != nullptr) {
      context = firstResumingContext;
      firstResumingContext = context->next;
      break;
    }

    epoll_event event;
    int count = epoll_wait(epoll, &event, 1, -1);
    if (count == 1) {
      ContextPair *contextPair = static_cast<ContextPair*>(event.data.ptr);
      if(((event.events & (EPOLLIN | EPOLLOUT)) != 0) && contextPair->readContext == nullptr && contextPair->writeContext == nullptr) {
        uint64_t buf;
        auto transferred = read(remoteSpawnEvent, &buf, sizeof buf);
        if(transferred == -1) {
            throw std::runtime_error("Dispatcher::dispatch, read(remoteSpawnEvent) failed, " + lastErrorMessage());
        }

        MutextGuard guard(*reinterpret_cast<pthread_mutex_t*>(this->mutex));
        while (!remoteSpawningProcedures.empty()) {
          spawn(std::move(remoteSpawningProcedures.front()));
          remoteSpawningProcedures.pop();
        }

        continue;
      }

      if ((event.events & EPOLLOUT) != 0) {
        context = contextPair->writeContext->context;
        contextPair->writeContext->events = event.events;
      } else if ((event.events & EPOLLIN) != 0) {
        context = contextPair->readContext->context;
        contextPair->readContext->events = event.events;
      } else {
        continue;
      }

      assert(context != nullptr);
      break;
    }

    if (errno != EINTR) {
      throw std::runtime_error("Dispatcher::dispatch, epoll_wait failed, "  + lastErrorMessage());
    }
  }

  if (context != currentContext) {
    ucontext_t* oldContext = static_cast<ucontext_t*>(currentContext->ucontext);
    currentContext = context;
    if (swapcontext(oldContext, static_cast<ucontext_t *>(context->ucontext)) == -1) {
      throw std::runtime_error("Dispatcher::dispatch, swapcontext failed, " + lastErrorMessage());
    }
  }
}

NativeContext* Dispatcher::getCurrentContext() const {
  return currentContext;
}

void Dispatcher::interrupt() {
  interrupt(currentContext);
}

void Dispatcher::interrupt(NativeContext* context) {
  assert(context!=nullptr);
  if (!context->interrupted) {
    if (context->interruptProcedure != nullptr) {
      context->interruptProcedure();
      context->interruptProcedure = nullptr;
    } else {
      context->interrupted = true;
    }
  }
}

bool Dispatcher::interrupted() {
  if (currentContext->interrupted) {
    currentContext->interrupted = false;
    return true;
  }

  return false;
}

void Dispatcher::pushContext(NativeContext* context) {
  assert(context != nullptr);
  context->next = nullptr;
  if(firstResumingContext != nullptr) {
    assert(lastResumingContext != nullptr);
    lastResumingContext->next = context;
  } else {
    firstResumingContext = context;
  }

  lastResumingContext = context;
}

void Dispatcher::remoteSpawn(std::function<void()>&& procedure) {
  {
    MutextGuard guard(*reinterpret_cast<pthread_mutex_t*>(this->mutex));
    remoteSpawningProcedures.push(std::move(procedure));
  }
  uint64_t one = 1;
  auto transferred = write(remoteSpawnEvent, &one, sizeof one);
  if(transferred == - 1) {
    throw std::runtime_error("Dispatcher::remoteSpawn, write failed, " + lastErrorMessage());
  }
}

void Dispatcher::spawn(std::function<void()>&& procedure) {
  NativeContext* context = &getReusableContext();
  if(contextGroup.firstContext != nullptr) {
    context->groupPrev = contextGroup.lastContext;
    assert(contextGroup.lastContext->groupNext == nullptr);
    contextGroup.lastContext->groupNext = context;
  } else {
    context->groupPrev = nullptr;
    contextGroup.firstContext = context;
    contextGroup.firstWaiter = nullptr;
  }

  context->interrupted = false;
  context->group = &contextGroup;
  context->groupNext = nullptr;
  context->procedure = std::move(procedure);
  contextGroup.lastContext = context;
  pushContext(context);
}

void Dispatcher::yield() {
  for(;;){
    epoll_event events[16];
    int count = epoll_wait(epoll, events, 16, 0);
    if (count == 0) {
      break;
    }

    if(count > 0) {
      for(int i = 0; i < count; ++i) {
        ContextPair *contextPair = static_cast<ContextPair*>(events[i].data.ptr);
        if(((events[i].events & (EPOLLIN | EPOLLOUT)) != 0) && contextPair->readContext == nullptr && contextPair->writeContext == nullptr) {
          uint64_t buf;
          auto transferred = read(remoteSpawnEvent, &buf, sizeof buf);
          if(transferred == -1) {
            throw std::runtime_error("Dispatcher::dispatch, read(remoteSpawnEvent) failed, " + lastErrorMessage());
          }

          MutextGuard guard(*reinterpret_cast<pthread_mutex_t*>(this->mutex));
          while (!remoteSpawningProcedures.empty()) {
            spawn(std::move(remoteSpawningProcedures.front()));
            remoteSpawningProcedures.pop();
          }

          continue;
        }

        if ((events[i].events & EPOLLOUT) != 0) {
          contextPair->writeContext->context->interruptProcedure = nullptr;
          pushContext(contextPair->writeContext->context);
          contextPair->writeContext->events = events[i].events;
        } else if ((events[i].events & EPOLLIN) != 0) {
          contextPair->readContext->context->interruptProcedure = nullptr;
          pushContext(contextPair->readContext->context);
          contextPair->readContext->events = events[i].events;
        } else {
          continue;
        }
      }
    } else {
      if (errno != EINTR) {
        throw std::runtime_error("Dispatcher::dispatch, epoll_wait failed, " + lastErrorMessage());
      }
    }
  }

  if (firstResumingContext != nullptr) {
    pushContext(currentContext);
    dispatch();
  }
}

int Dispatcher::getEpoll() const {
  return epoll;
}

NativeContext& Dispatcher::getReusableContext() {
  if(firstReusableContext == nullptr) {
    ucontext_t* newlyCreatedContext = new ucontext_t;
    if (getcontext(newlyCreatedContext) == -1) { //makecontext precondition
      throw std::runtime_error("Dispatcher::getReusableContext, getcontext failed, " + lastErrorMessage());
    }

    auto stackPointer = new uint8_t[STACK_SIZE];
    newlyCreatedContext->uc_stack.ss_sp = stackPointer;
    newlyCreatedContext->uc_stack.ss_size = STACK_SIZE;

    ContextMakingData makingContextData {this, newlyCreatedContext};
    makecontext(newlyCreatedContext, (void(*)())contextProcedureStatic, 1, reinterpret_cast<int*>(&makingContextData));

    ucontext_t* oldContext = static_cast<ucontext_t*>(currentContext->ucontext);
    if (swapcontext(oldContext, newlyCreatedContext) == -1) {
      throw std::runtime_error("Dispatcher::getReusableContext, swapcontext failed, " + lastErrorMessage());
    }

    assert(firstReusableContext != nullptr);
    assert(firstReusableContext->ucontext == newlyCreatedContext);
    firstReusableContext->stackPtr = stackPointer;
  };

  NativeContext* context = firstReusableContext;
  firstReusableContext = firstReusableContext-> next;
  return *context;
}

void Dispatcher::pushReusableContext(NativeContext& context) {
  context.next = firstReusableContext;
  firstReusableContext = &context;
  --runningContextCount;
}

int Dispatcher::getTimer() {
  int timer;
  if (timers.empty()) {
    timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    epoll_event timerEvent;
    timerEvent.events = 0;
    timerEvent.data.ptr = nullptr;

    if (epoll_ctl(getEpoll(), EPOLL_CTL_ADD, timer, &timerEvent) == -1) {
      throw std::runtime_error("Dispatcher::getTimer, epoll_ctl failed, "  + lastErrorMessage());
    }
  } else {
    timer = timers.top();
    timers.pop();
  }

  return timer;
}

void Dispatcher::pushTimer(int timer) {
  timers.push(timer);
}

void Dispatcher::contextProcedure(void* ucontext) {
  assert(firstReusableContext == nullptr);
  NativeContext context;
  context.ucontext = ucontext;
  context.interrupted = false;
  context.next = nullptr;
  firstReusableContext = &context;
  ucontext_t* oldContext = static_cast<ucontext_t*>(context.ucontext);
  if (swapcontext(oldContext, static_cast<ucontext_t*>(currentContext->ucontext)) == -1) {
    throw std::runtime_error("Dispatcher::contextProcedure, swapcontext failed, " + lastErrorMessage());
  }

  for (;;) {
    ++runningContextCount;
    try {
      context.procedure();
    } catch(std::exception&) {
    }

    if (context.group != nullptr) {
      if (context.groupPrev != nullptr) {
        assert(context.groupPrev->groupNext == &context);
        context.groupPrev->groupNext = context.groupNext;
        if (context.groupNext != nullptr) {
          assert(context.groupNext->groupPrev == &context);
          context.groupNext->groupPrev = context.groupPrev;
        } else {
          assert(context.group->lastContext == &context);
          context.group->lastContext = context.groupPrev;
        }
      } else {
        assert(context.group->firstContext == &context);
        context.group->firstContext = context.groupNext;
        if (context.groupNext != nullptr) {
          assert(context.groupNext->groupPrev == &context);
          context.groupNext->groupPrev = nullptr;
        } else {
          assert(context.group->lastContext == &context);
          if (context.group->firstWaiter != nullptr) {
            if (firstResumingContext != nullptr) {
              assert(lastResumingContext->next == nullptr);
              lastResumingContext->next = context.group->firstWaiter;
            } else {
              firstResumingContext = context.group->firstWaiter;
            }

            lastResumingContext = context.group->lastWaiter;
            context.group->firstWaiter = nullptr;
          }
        }
      }

      pushReusableContext(context);
    }

    dispatch();
  }
};

void Dispatcher::contextProcedureStatic(void *context) {
  ContextMakingData* makingContextData = reinterpret_cast<ContextMakingData*>(context);
  makingContextData->dispatcher->contextProcedure(makingContextData->ucontext);
}

}
