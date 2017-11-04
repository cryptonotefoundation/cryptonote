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

#include "ContextGroup.h"
#include <cassert>

namespace System {

ContextGroup::ContextGroup(Dispatcher& dispatcher) : dispatcher(&dispatcher) {
  contextGroup.firstContext = nullptr;
}

ContextGroup::ContextGroup(ContextGroup&& other) : dispatcher(other.dispatcher) {
  if (dispatcher != nullptr) {
    assert(other.contextGroup.firstContext == nullptr);
    contextGroup.firstContext = nullptr;
    other.dispatcher = nullptr;
  }
}

ContextGroup::~ContextGroup() {
  if (dispatcher != nullptr) {
    interrupt();
    wait();
  }
}

ContextGroup& ContextGroup::operator=(ContextGroup&& other) {
  assert(dispatcher == nullptr || contextGroup.firstContext == nullptr);
  dispatcher = other.dispatcher;
  if (dispatcher != nullptr) {
    assert(other.contextGroup.firstContext == nullptr);
    contextGroup.firstContext = nullptr;
    other.dispatcher = nullptr;
  }

  return *this;
}

void ContextGroup::interrupt() {
  assert(dispatcher != nullptr);
  for (NativeContext* context = contextGroup.firstContext; context != nullptr; context = context->groupNext) {
    dispatcher->interrupt(context);
  }
}

void ContextGroup::spawn(std::function<void()>&& procedure) {
  assert(dispatcher != nullptr);
  NativeContext& context = dispatcher->getReusableContext();
  if (contextGroup.firstContext != nullptr) {
    context.groupPrev = contextGroup.lastContext;
    assert(contextGroup.lastContext->groupNext == nullptr);
    contextGroup.lastContext->groupNext = &context;
  } else {
    context.groupPrev = nullptr;
    contextGroup.firstContext = &context;
    contextGroup.firstWaiter = nullptr;
  }

  context.interrupted = false;
  context.group = &contextGroup;
  context.groupNext = nullptr;
  context.procedure = std::move(procedure);
  contextGroup.lastContext = &context;
  dispatcher->pushContext(&context);
}

void ContextGroup::wait() {
  if (contextGroup.firstContext != nullptr) {
    NativeContext* context = dispatcher->getCurrentContext();
    context->next = nullptr;
    if (contextGroup.firstWaiter != nullptr) {
      assert(contextGroup.lastWaiter->next == nullptr);
      contextGroup.lastWaiter->next = context;
    } else {
      contextGroup.firstWaiter = context;
    }

    contextGroup.lastWaiter = context;
    dispatcher->dispatch();
    assert(context == dispatcher->getCurrentContext());
  }
}

}
