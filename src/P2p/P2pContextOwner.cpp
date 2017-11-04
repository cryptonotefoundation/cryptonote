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

#include "P2pContextOwner.h"
#include <cassert>
#include "P2pContext.h"

namespace CryptoNote {

P2pContextOwner::P2pContextOwner(P2pContext* ctx, ContextList& contextList) : contextList(contextList) {
  contextIterator = contextList.insert(contextList.end(), ContextList::value_type(ctx));
}

P2pContextOwner::P2pContextOwner(P2pContextOwner&& other) : contextList(other.contextList), contextIterator(other.contextIterator) {
  other.contextIterator = contextList.end();
}

P2pContextOwner::~P2pContextOwner() {
  if (contextIterator != contextList.end()) {
    contextList.erase(contextIterator);
  }
}

P2pContext& P2pContextOwner::get() {
  assert(contextIterator != contextList.end());
  return *contextIterator->get();
}

P2pContext* P2pContextOwner::operator -> () {
  return &get();
}

}
