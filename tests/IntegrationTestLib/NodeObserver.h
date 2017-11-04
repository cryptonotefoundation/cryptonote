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

#include "INode.h"
#include <mutex>
#include <condition_variable>

namespace CryptoNote {


template <typename T>
class ObservableValue {
public:
  ObservableValue(const T defaultValue = 0) : 
    m_prev(defaultValue), m_value(defaultValue) {
  }

  void init(T value) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_value = m_prev = value;
  }

  void set(T value) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_value = value;
    m_cv.notify_all();
  }

  T get() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_value;
  }

  bool waitFor(std::chrono::milliseconds ms, T& value) {
    std::unique_lock<std::mutex> lk(m_mutex);

    if (m_cv.wait_for(lk, ms, [this] { return m_prev != m_value; })) {
      value = m_prev = m_value;
      return true;
    }

    return false;
  }

  T wait() {
    std::unique_lock<std::mutex> lk(m_mutex);

    m_cv.wait(lk, [this] { return m_prev != m_value; });
    m_prev = m_value;
    return m_value;
  }

private:

  std::mutex m_mutex;
  std::condition_variable m_cv;

  T m_prev;
  T m_value;
};

class NodeObserver: public INodeObserver {
public:

  NodeObserver(INode& node) : m_node(node) {
    m_knownHeight.init(node.getLastKnownBlockHeight());
    node.addObserver(this);
  }

  ~NodeObserver() {
    m_node.removeObserver(this);
  }

  virtual void lastKnownBlockHeightUpdated(uint32_t height) override {
    m_knownHeight.set(height);
  }

  virtual void localBlockchainUpdated(uint32_t height) override {
    m_localHeight.set(height);
  }

  virtual void peerCountUpdated(size_t count) override {
    m_peerCount.set(count);
  }

  bool waitLastKnownBlockHeightUpdated(std::chrono::milliseconds ms, uint32_t& value) {
    return m_knownHeight.waitFor(ms, value);
  }

  bool waitLocalBlockchainUpdated(std::chrono::milliseconds ms, uint32_t& value) {
    return m_localHeight.waitFor(ms, value);
  }

  uint32_t waitLastKnownBlockHeightUpdated() {
    return m_knownHeight.wait();
  }

  ObservableValue<uint32_t> m_knownHeight;
  ObservableValue<uint32_t> m_localHeight;
  ObservableValue<size_t> m_peerCount;

private:

  INode& m_node;
};


}
