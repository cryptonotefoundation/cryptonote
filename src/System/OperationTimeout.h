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

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Timer.h>

namespace System {

template<typename T> class OperationTimeout {
public:
  OperationTimeout(Dispatcher& dispatcher, T& object, std::chrono::nanoseconds timeout) :
    object(object), timerContext(dispatcher), timeoutTimer(dispatcher) {
    timerContext.spawn([this, timeout]() {
      try {
        timeoutTimer.sleep(timeout);
        timerContext.interrupt();
      } catch (std::exception&) {
      }
    });
  }

  ~OperationTimeout() {
    timerContext.interrupt();
    timerContext.wait();
  }

private:
  T& object;
  ContextGroup timerContext;
  Timer timeoutTimer;
};

}
