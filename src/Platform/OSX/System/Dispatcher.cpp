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
#include <string>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "Context.h"
#include "ErrorMessage.h"

namespace System {

namespace{

struct ContextMakingData {
  void* uctx;
  Dispatcher* dispatcher;
};

class MutextGuard {
public:
  MutextGuard(pthread_mutex_t& _mutex) : mutex(_mutex) {
    auto ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
      throw std::runtime_error("MutextGuard::MutextGuard, pthread_mutex_lock failed, " + errorMessage(ret));
    }
  }

  ~MutextGuard() {
    pthread_mutex_unlock(&mutex);
  }

private:
  pthread_mutex_t& mutex;
};

const size_t STACK_SIZE = 64 * 1024;

}

static_assert(Dispatcher::SIZEOF_PTHREAD_MUTEX_T == sizeof(pthread_mutex_t), "invalid pthread mutex size");

Dispatcher::Dispatcher() : lastCreatedTimer(0) {
  std::string message;
  kqueue = ::kqueue();
  if (kqueue == -1) {
    message = "kqueue failed, " + lastErrorMessage();
  } else {
    mainContext.uctx = new uctx;
    if (getcontext(static_cast<uctx*>(mainContext.uctx)) == -1) {
      message = "getcontext failed, " + lastErrorMessage();
    } else {
      struct kevent event;
      EV_SET(&event, 0, EVFILT_USER, EV_ADD, NOTE_FFNOP, 0, NULL);
      if (kevent(kqueue, &event, 1, NULL, 0, NULL) == -1) {
        message = "kevent failed, " + lastErrorMessage();
      } else {
        if(pthread_mutex_init(reinterpret_cast<pthread_mutex_t*>(this->mutex), NULL) == -1) {
          message = "pthread_mutex_init failed, " + lastErrorMessage();
        } else {
          remoteSpawned = false;
          
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
      }
    }

    auto result = close(kqueue);
    assert(result == 0);
  }

  throw std::runtime_error("Dispatcher::Dispatcher, " + message);
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
    auto ucontext = static_cast<uctx*>(firstReusableContext->uctx);
    auto stackPtr = static_cast<uint8_t *>(firstReusableContext->stackPtr);
    firstReusableContext = firstReusableContext->next;
    delete[] stackPtr;
    delete ucontext;
  }
  
  auto result = close(kqueue);
  assert(result != -1);
  result = pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(this->mutex));
  assert(result != -1);
}

void Dispatcher::clear() {
  while (firstReusableContext != nullptr) {
    auto ucontext = static_cast<uctx*>(firstReusableContext->uctx);
    auto stackPtr = static_cast<uint8_t *>(firstReusableContext->stackPtr);
    firstReusableContext = firstReusableContext->next;
    delete[] stackPtr;
    delete ucontext;
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
    
    if(remoteSpawned.load() == true) {
      MutextGuard guard(*reinterpret_cast<pthread_mutex_t*>(this->mutex));
      while (!remoteSpawningProcedures.empty()) {
        spawn(std::move(remoteSpawningProcedures.front()));
        remoteSpawningProcedures.pop();
      }

      remoteSpawned = false;
      continue;
    }

    struct kevent event;
    int count = kevent(kqueue, NULL, 0, &event, 1, NULL);
    if (count == 1) {
      if (event.flags & EV_ERROR) {
        continue;
      }

      if (event.filter == EVFILT_USER && event.ident == 0) {
        struct kevent event;
        EV_SET(&event, 0, EVFILT_USER, EV_ADD | EV_DISABLE, NOTE_FFNOP, 0, NULL);
        if (kevent(kqueue, &event, 1, NULL, 0, NULL) == -1) {
          throw std::runtime_error("Dispatcher::dispatch, kevent failed, " + lastErrorMessage());
        }

        continue;
      }

      if (event.filter == EVFILT_WRITE) {
        event.flags = EV_DELETE | EV_DISABLE;
        kevent(kqueue, &event, 1, NULL, 0, NULL); // ignore error here
      }

      context = static_cast<OperationContext*>(event.udata)->context;
      break;
    }

    if (errno != EINTR) {
      throw std::runtime_error("Dispatcher::dispatch, kqueue failed, " + lastErrorMessage());
    } else {
      MutextGuard guard(*reinterpret_cast<pthread_mutex_t*>(this->mutex));
      while (!remoteSpawningProcedures.empty()) {
        spawn(std::move(remoteSpawningProcedures.front()));
        remoteSpawningProcedures.pop();
      }

    }
  }

  if (context != currentContext) {
    uctx* oldContext = static_cast<uctx*>(currentContext->uctx);
    currentContext = context;
    if (swapcontext(oldContext,static_cast<uctx*>(currentContext->uctx)) == -1) {
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
  assert(context!=nullptr);
  context->next = nullptr;
  if (firstResumingContext != nullptr) {
    assert(lastResumingContext != nullptr);
    lastResumingContext->next = context;
  } else {
    firstResumingContext = context;
  }

  lastResumingContext = context;
}

void Dispatcher::remoteSpawn(std::function<void()>&& procedure) {
  MutextGuard guard(*reinterpret_cast<pthread_mutex_t*>(this->mutex));
  remoteSpawningProcedures.push(std::move(procedure));
  if (remoteSpawned == false) {
    remoteSpawned = true;
    struct kevent event;
    EV_SET(&event, 0, EVFILT_USER, EV_ADD | EV_ENABLE, NOTE_FFCOPY | NOTE_TRIGGER, 0, NULL);
    if (kevent(kqueue, &event, 1, NULL, 0, NULL) == -1) {
      throw std::runtime_error("Dispatcher::remoteSpawn, kevent failed, " + lastErrorMessage());
    };
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
  struct timespec zeroTimeout = { 0, 0 };
  int updatesCounter = 0;
  for (;;) {
    struct kevent events[16];
    struct kevent updates[16];
    int count = kevent(kqueue, updates, updatesCounter, events, 16, &zeroTimeout);
    if (count == 0) {
      break;
    }

    updatesCounter = 0;
    if (count > 0) {
      for (int i = 0; i < count; ++i) {
        if (events[i].flags & EV_ERROR) {
          continue;
        }

        if (events[i].filter == EVFILT_USER && events[i].ident == 0) {
          EV_SET(&updates[updatesCounter++], 0, EVFILT_USER, EV_ADD | EV_DISABLE, NOTE_FFNOP, 0, NULL);
          
          MutextGuard guard(*reinterpret_cast<pthread_mutex_t*>(this->mutex));
          while (!remoteSpawningProcedures.empty()) {
            spawn(std::move(remoteSpawningProcedures.front()));
            remoteSpawningProcedures.pop();
          }

          remoteSpawned = false;
          continue;
        }

        static_cast<OperationContext*>(events[i].udata)->context->interruptProcedure = nullptr;
        pushContext(static_cast<OperationContext*>(events[i].udata)->context);
        if (events[i].filter == EVFILT_WRITE) {
          EV_SET(&updates[updatesCounter++], events[i].ident, EVFILT_WRITE, EV_DELETE | EV_DISABLE, 0, 0, NULL);
        }
      }
    } else {
      if (errno != EINTR) {
        throw std::runtime_error("Dispatcher::dispatch, kevent failed, " + lastErrorMessage());
      }
    }
  }

  if (firstResumingContext != nullptr) {
    pushContext(currentContext);
    dispatch();
  }
}

int Dispatcher::getKqueue() const {
  return kqueue;
}

NativeContext& Dispatcher::getReusableContext() {
  if(firstReusableContext == nullptr) {
   uctx* newlyCreatedContext = new uctx;
   uint8_t* stackPointer = new uint8_t[STACK_SIZE];
   static_cast<uctx*>(newlyCreatedContext)->uc_stack.ss_sp = stackPointer;
   static_cast<uctx*>(newlyCreatedContext)->uc_stack.ss_size = STACK_SIZE;
   
   ContextMakingData makingData{ newlyCreatedContext, this};
   makecontext(static_cast<uctx*>(newlyCreatedContext), reinterpret_cast<void(*)()>(contextProcedureStatic), reinterpret_cast<intptr_t>(&makingData));
   
   uctx* oldContext = static_cast<uctx*>(currentContext->uctx);
   if (swapcontext(oldContext, newlyCreatedContext) == -1) {
     throw std::runtime_error("Dispatcher::getReusableContext, swapcontext failed, " + lastErrorMessage());
   }
   
   assert(firstReusableContext != nullptr);
   assert(firstReusableContext->uctx == newlyCreatedContext);
   firstReusableContext->stackPtr = stackPointer;
  }
  
  NativeContext* context = firstReusableContext;
  firstReusableContext = firstReusableContext->next;
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
    timer = ++lastCreatedTimer;
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
  context.uctx = ucontext;
  context.interrupted = false;
  context.next = nullptr;
  firstReusableContext = &context;
  uctx* oldContext = static_cast<uctx*>(context.uctx);
  if (swapcontext(oldContext, static_cast<uctx*>(currentContext->uctx)) == -1) {
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
}

void Dispatcher::contextProcedureStatic(intptr_t context) {
  ContextMakingData* makingContextData = reinterpret_cast<ContextMakingData*>(context);
  makingContextData->dispatcher->contextProcedure(makingContextData->uctx);
}

}
