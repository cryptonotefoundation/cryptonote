// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2025, nuclEar_chaos, The Karbo developers

#include "Dispatcher.h"

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <utility>

#include <boost/asio.hpp>
#include <boost/coroutine/symmetric_coroutine.hpp>

namespace System {

  using coro_t = boost::coroutines::symmetric_coroutine<void>;

  static inline uint64_t now_ms() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
  }

  Dispatcher::Dispatcher()
    : ioContext(),
    currentContext(&mainContext),
    firstResumingContext(nullptr),
    lastResumingContext(nullptr),
    firstReusableContext(nullptr),
    runningContextCount(0),
    wakeTimer(ioContext),
    wakeArmed(false),
    wakeExpiryMs(0) {

    // Initialize main context
    mainContext.coro = nullptr;
    mainContext.yield = nullptr;
    mainContext.interrupted = false;
    mainContext.inExecutionQueue = false;
    mainContext.group = &contextGroup;
    mainContext.groupPrev = nullptr;
    mainContext.groupNext = nullptr;
    mainContext.next = nullptr;
    mainContext.procedure = nullptr;
    mainContext.interruptProcedure = nullptr;

    contextGroup.firstContext = nullptr;
    contextGroup.lastContext = nullptr;
    contextGroup.firstWaiter = nullptr;
    contextGroup.lastWaiter = nullptr;
  }

  Dispatcher::~Dispatcher() {
    // Interrupt all contexts still in the group
    for (NativeContext* ctx = contextGroup.firstContext; ctx != nullptr; ctx = ctx->groupNext) {
      interrupt(ctx);
    }

    // Give them a chance to handle interrupts and exit
    yield();

    // Drain any remaining asio work
    ensureIoContextReady();
    for (;;) {
      std::size_t ran = ioContext.poll();
      if (ran == 0) break;
    }

    assert(contextGroup.firstContext == nullptr);
    assert(contextGroup.firstWaiter == nullptr);
    assert(firstResumingContext == nullptr);
    assert(runningContextCount == 0);

    // Free reusable coroutine objects
    while (firstReusableContext != nullptr) {
      auto* pcoro = firstReusableContext->coro;
      firstReusableContext->coro = nullptr;
      firstReusableContext->yield = nullptr;
      firstReusableContext = firstReusableContext->next;
      delete pcoro;
    }
  }

  void Dispatcher::clear() {
    while (firstReusableContext != nullptr) {
      auto* pcoro = firstReusableContext->coro;
      firstReusableContext->coro = nullptr;
      firstReusableContext->yield = nullptr;
      firstReusableContext = firstReusableContext->next;
      delete pcoro;
    }

    timers.clear();
    // Cancel any wake timer
    boost::system::error_code ignored;
    wakeTimer.cancel(ignored);
    wakeArmed = false;
    wakeExpiryMs = 0;
  }

  NativeContext* Dispatcher::getCurrentContext() const {
    return currentContext;
  }

  void Dispatcher::interrupt() {
    interrupt(currentContext);
  }

  void Dispatcher::interrupt(NativeContext* context) {
    assert(context != nullptr);
    if (!context->interrupted) {
      context->interrupted = true;
      if (context->interruptProcedure != nullptr) {
        context->interruptProcedure();
        context->interruptProcedure = nullptr;
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

    if (context->inExecutionQueue) {
      return;
    }

    context->next = nullptr;
    context->inExecutionQueue = true;

    if (firstResumingContext != nullptr) {
      assert(lastResumingContext->next == nullptr);
      lastResumingContext->next = context;
    }
    else {
      firstResumingContext = context;
    }
    lastResumingContext = context;
  }

  void Dispatcher::remoteSpawn(std::function<void()>&& procedure) {
    // Run spawn on the dispatcher thread via asio
    ioContext.post([this, p = std::move(procedure)]() mutable {
      this->spawn(std::move(p));
      });
  }

  void Dispatcher::spawn(std::function<void()>&& procedure) {
    NativeContext& context = getReusableContext();

    if (contextGroup.firstContext != nullptr) {
      context.groupPrev = contextGroup.lastContext;
      assert(contextGroup.lastContext->groupNext == nullptr);
      contextGroup.lastContext->groupNext = &context;
    }
    else {
      context.groupPrev = nullptr;
      contextGroup.firstContext = &context;
      contextGroup.firstWaiter = nullptr;
    }

    context.interrupted = false;
    context.group = &contextGroup;
    context.groupNext = nullptr;
    context.procedure = std::move(procedure);
    context.inExecutionQueue = false;
    context.interruptProcedure = nullptr;

    contextGroup.lastContext = &context;
    pushContext(&context);
  }

  void Dispatcher::yield() {
    // Expire due timers
    const uint64_t now = now_ms();
    for (auto it = timers.begin(); it != timers.end();) {
      if (it->first <= now) {
        it->second->interruptProcedure = nullptr;
        pushContext(it->second);
        it = timers.erase(it);
      }
      else {
        break;
      }
    }

    // Process all ready asio handlers without blocking
    try {
      ensureIoContextReady();
      ioContext.poll();
    }
    catch (...) {
      // swallow
    }

    if (firstResumingContext != nullptr) {
      pushContext(currentContext);
      dispatch();
    }
  }

  static inline void arm_wake_timer_if_needed(boost::asio::steady_timer& timer,
    bool& armed,
    uint64_t& armedMs,
    const std::multimap<uint64_t, NativeContext*>& timers) {
    if (timers.empty()) {
      if (armed) {
        boost::system::error_code ignored;
        timer.cancel(ignored);
        armed = false;
        armedMs = 0;
      }
      return;
    }

    uint64_t earliest = timers.begin()->first;
    if (!armed || earliest != armedMs) {
      using namespace std::chrono;
      auto tp = steady_clock::time_point(milliseconds(earliest));
      boost::system::error_code ignored;
      timer.expires_at(tp, ignored);
      timer.async_wait([](const boost::system::error_code&) {
        // no-op; dispatch() loop will re-check timers after run_one() returns
        });
      armed = true;
      armedMs = earliest;
    }
  }

  void Dispatcher::dispatch() {
    NativeContext* context = nullptr;

    for (;;) {
      // 1) If something is ready, pop and run it
      if (firstResumingContext != nullptr) {
        context = firstResumingContext;
        firstResumingContext = context->next;
        if (firstResumingContext == nullptr) {
          lastResumingContext = nullptr;
        }
        // Clear flags on dequeue
        context->inExecutionQueue = false;
        context->interruptProcedure = nullptr;
        break;
      }

      // 2) Expire timers
      const uint64_t now = now_ms();
      for (auto it = timers.begin(); it != timers.end();) {
        if (it->first <= now) {
          pushContext(it->second);
          it = timers.erase(it);
        }
        else {
          break;
        }
      }
      if (firstResumingContext != nullptr) {
        context = firstResumingContext;
        firstResumingContext = context->next;
        if (firstResumingContext == nullptr) {
          lastResumingContext = nullptr;
        }
        context->inExecutionQueue = false;
        context->interruptProcedure = nullptr;
        break;
      }

      // 3) Nothing ready - ensure wakeTimer is armed
      if (timers.empty()) {
        // Arm single long-lived dummy timer only once while there are no real timers.
        if (!wakeArmed) {
          using namespace std::chrono;
          // set a long duration (effectively "never" under normal conditions)
          wakeTimer.expires_after(hours(1));

          // capture 'this' pointer safely; handler will clear armed state on completion
          wakeTimer.async_wait([this](const boost::system::error_code& ec) {
            // When the dummy timer completes (either fired or was cancelled),
            // mark wakeArmed false so future loops may re-arm real timers as needed.
            // We don't need to re-arm here - dispatch() will re-evaluate state.
            this->wakeArmed = false;
            this->wakeExpiryMs = 0;
            (void)ec; // silence unused param in builds without logging
            });

          wakeArmed = true;
          // approximate expiry ms for diagnostics; exact value not critical
          wakeExpiryMs = now_ms() + static_cast<uint64_t>(60 * 60 * 1000); // +1 hour
        }
      }
      else {
        // There are real timers -> arm wake timer to earliest deadline (existing helper)
        arm_wake_timer_if_needed(wakeTimer, wakeArmed, wakeExpiryMs, timers);
      }

      // 4) Block until a handler runs or wakeTimer fires
      try {
        ensureIoContextReady();
        ioContext.run_one(); // will now block even if no real work is ready
      }
      catch (...) {
        // Swallow, keep loop alive
      }

      // Loop back; ready queues and timers will be re-checked
    }

    // 5) Switch to the selected context if different
    if (context != currentContext) {
      auto* prevYield = currentContext->yield;
      currentContext = context;

      if (prevYield != nullptr) {
        if (context->coro != nullptr) {
          (*prevYield)(*context->coro);
        }
        else {
          (*prevYield)();
        }
      }
      else {
        if (context->coro) {
          (*context->coro)();
        }
      }
    }
  }

  void Dispatcher::addTimer(uint64_t timeMs, NativeContext* context) {
    timers.emplace(timeMs, context);
    // If this is now the earliest timer, re-arm wake timer
    if (timers.begin()->first == timeMs) {
      arm_wake_timer_if_needed(wakeTimer, wakeArmed, wakeExpiryMs, timers);
    }
  }

  void Dispatcher::interruptTimer(uint64_t timeMs, NativeContext* context) {
    if (context->inExecutionQueue) {
      return;
    }
    auto range = timers.equal_range(timeMs);
    for (auto it = range.first; it != range.second; ++it) {
      if (it->second == context) {
        pushContext(context);
        bool wasEarliest = (it == timers.begin());
        timers.erase(it);
        if (wasEarliest) {
          arm_wake_timer_if_needed(wakeTimer, wakeArmed, wakeExpiryMs, timers);
        }
        break;
      }
    }
  }

  NativeContext& Dispatcher::getReusableContext() {
    if (firstReusableContext == nullptr) {
      // Create a coroutine whose first action is to publish its NativeContext
      auto* pCoro = new coro_t::call_type([this](coro_t::yield_type& y) {
        this->contextProcedure(y);
        });

      // Start it: it yields immediately after publishing NativeContext
      (*pCoro)();

      assert(firstReusableContext != nullptr);
      firstReusableContext->coro = pCoro;
    }

    NativeContext* ctx = firstReusableContext;
    firstReusableContext = ctx->next;
    return *ctx;
  }

  void Dispatcher::pushReusableContext(NativeContext& context) {
    context.next = firstReusableContext;
    firstReusableContext = &context;
    --runningContextCount;
  }

  void Dispatcher::contextProcedure(coro_t::yield_type& yield) {
    // NativeContext lives on this coroutine's stack
    NativeContext context{};
    context.coro = nullptr;                 // set after first yield by getReusableContext()
    context.yield = &yield;
    context.interrupted = false;
    context.inExecutionQueue = false;
    context.next = nullptr;
    context.group = nullptr;
    context.groupPrev = nullptr;
    context.groupNext = nullptr;
    context.procedure = nullptr;
    context.interruptProcedure = nullptr;

    assert(firstReusableContext == nullptr);
    firstReusableContext = &context;

    // Hand control back so creator can attach coro pointer
    yield();

    for (;;) {
      ++runningContextCount;
      try {
        if (context.procedure) {
          context.procedure();
        }
      }
      catch (...) {
        // keep scheduler alive
      }

      // Remove from group, mirroring original logic
      if (context.group != nullptr) {
        if (context.groupPrev != nullptr) {
          assert(context.groupPrev->groupNext == &context);
          context.groupPrev->groupNext = context.groupNext;
          if (context.groupNext != nullptr) {
            assert(context.groupNext->groupPrev == &context);
            context.groupNext->groupPrev = context.groupPrev;
          }
          else {
            assert(context.group->lastContext == &context);
            context.group->lastContext = context.groupPrev;
          }
        }
        else {
          assert(context.group->firstContext == &context);
          context.group->firstContext = context.groupNext;
          if (context.groupNext != nullptr) {
            assert(context.groupNext->groupPrev == &context);
            context.groupNext->groupPrev = nullptr;
          }
          else {
            assert(context.group->lastContext == &context);
            if (context.group->firstWaiter != nullptr) {
              if (firstResumingContext != nullptr) {
                assert(lastResumingContext->next == nullptr);
                lastResumingContext->next = context.group->firstWaiter;
              }
              else {
                firstResumingContext = context.group->firstWaiter;
              }
              lastResumingContext = context.group->lastWaiter;
              context.group->firstWaiter = nullptr;
            }
          }
        }

        pushReusableContext(context);
      }

      // Continue scheduling
      dispatch();
    }
  }

  void Dispatcher::ensureIoContextReady() {
    if (ioContext.stopped()) {
      ioContext.restart();
    }
  }

} // namespace System
