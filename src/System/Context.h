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

#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/InterruptedException.h>

namespace System {

template<typename ResultType = void>
class Context {
public:
  Context(Dispatcher& dispatcher, std::function<ResultType()>&& target) : 
    dispatcher(dispatcher), target(std::move(target)), ready(dispatcher), bindingContext(dispatcher.getReusableContext()) {
    bindingContext.interrupted = false;
    bindingContext.groupNext = nullptr;
    bindingContext.groupPrev = nullptr;
    bindingContext.group = nullptr;
    bindingContext.procedure = [this] {
      try {
        new(resultStorage) ResultType(this->target());
      } catch (...) {
        exceptionPointer = std::current_exception();
      }

      ready.set();
    };

    dispatcher.pushContext(&bindingContext);
  }

  Context(const Context&) = delete;  
  Context& operator=(const Context&) = delete;

  ~Context() {
    interrupt();
    wait();
    dispatcher.pushReusableContext(bindingContext);
  }

  ResultType& get() {
    wait();
    if (exceptionPointer != nullptr) {
      std::rethrow_exception(exceptionPointer);
    }

    return *reinterpret_cast<ResultType*>(resultStorage);
  }

  void interrupt() {
    dispatcher.interrupt(&bindingContext);
  }

  void wait() {
    for (;;) {
      try {
        ready.wait();
        break;
      } catch (InterruptedException&) {
        interrupt();
      }
    }
  }

private:
  uint8_t resultStorage[sizeof(ResultType)];
  Dispatcher& dispatcher;
  std::function<ResultType()> target;
  Event ready;
  NativeContext& bindingContext;
  std::exception_ptr exceptionPointer;
};

template<>
class Context<void> {
public:
  Context(Dispatcher& dispatcher, std::function<void()>&& target) :
    dispatcher(dispatcher), target(std::move(target)), ready(dispatcher), bindingContext(dispatcher.getReusableContext()) {
    bindingContext.interrupted = false;
    bindingContext.groupNext = nullptr;
    bindingContext.groupPrev = nullptr;
    bindingContext.group = nullptr;
    bindingContext.procedure = [this] {
      try {
        this->target();
      } catch (...) {
        exceptionPointer = std::current_exception();
      }

      ready.set();
    };

    dispatcher.pushContext(&bindingContext);
  }

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ~Context() {
    interrupt();
    wait();
    dispatcher.pushReusableContext(bindingContext);
  }

  void get() {
    wait();
    if (exceptionPointer != nullptr) {
      std::rethrow_exception(exceptionPointer);
    }
  }

  void interrupt() {
    dispatcher.interrupt(&bindingContext);
  }

  void wait() {
    for (;;) {
      try {
        ready.wait();
        break;
      } catch (InterruptedException&) {
        interrupt();
      }
    }
  }

private:
  Dispatcher& dispatcher;
  std::function<void()> target;
  Event ready;
  NativeContext& bindingContext;
  std::exception_ptr exceptionPointer;
};

}
