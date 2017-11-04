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

#include "ICryptoNoteProtocolQueryStub.h"

bool ICryptoNoteProtocolQueryStub::addObserver(CryptoNote::ICryptoNoteProtocolObserver* observer) {
  return false;
}

bool ICryptoNoteProtocolQueryStub::removeObserver(CryptoNote::ICryptoNoteProtocolObserver* observer) {
  return false;
}

uint32_t ICryptoNoteProtocolQueryStub::getObservedHeight() const {
  return observedHeight;
}

size_t ICryptoNoteProtocolQueryStub::getPeerCount() const {
  return peers;
}

bool ICryptoNoteProtocolQueryStub::isSynchronized() const {
  return synchronized;
}

void ICryptoNoteProtocolQueryStub::setPeerCount(uint32_t count) {
  peers = count;
}

void ICryptoNoteProtocolQueryStub::setObservedHeight(uint32_t height) {
  observedHeight = height;
}

void ICryptoNoteProtocolQueryStub::setSynchronizedStatus(bool status) {
    synchronized = status;
}
