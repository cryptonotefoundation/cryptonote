// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2025, The Karbo developers

#pragma once

#include <chrono>
#include <cassert>
#include "Dispatcher.h"
#include "InterruptedException.h"

namespace System {

  class Timer {
  public:
    Timer() = default;
    explicit Timer(Dispatcher& dispatcher) : dispatcher(&dispatcher) {}
    Timer(const Timer&) = delete;
    Timer(Timer&& other) noexcept : dispatcher(other.dispatcher) { other.dispatcher = nullptr; }
    ~Timer() = default;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&& other) noexcept {
      if (this != &other) {
        dispatcher = other.dispatcher;
        other.dispatcher = nullptr;
      }
      return *this;
    }

    void sleep(std::chrono::nanoseconds duration) {
      assert(dispatcher != nullptr);

      if (dispatcher->interrupted()) {
        throw InterruptedException();
      }

      // Convert to milliseconds (minimum 1 ms)
      uint64_t durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
      if (durationMs == 0) durationMs = 1;

      uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch()
        ).count()
        );
      uint64_t expireTime = now + durationMs;

      auto* context = dispatcher->getCurrentContext();
      bool interrupted = false;

      // Set interrupt procedure
      context->interruptProcedure = [&]() {
        if (!interrupted) {
          dispatcher->interruptTimer(expireTime, context);
          interrupted = true;
        }
        };

      // Register timer with dispatcher
      dispatcher->addTimer(expireTime, context);

      // Yield once; dispatcher will resume this fiber when timer fires or if interrupted
      dispatcher->dispatch();

      // Clear interrupt procedure
      context->interruptProcedure = nullptr;

      // Throw if fiber was interrupted (e.g., during shutdown)
      if (dispatcher->interrupted() || interrupted) {
        throw InterruptedException();
      }
    }

  private:
    Dispatcher* dispatcher{ nullptr };
  };

} // namespace System
