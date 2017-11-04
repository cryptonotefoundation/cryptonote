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

#include <mutex>
#include <condition_variable>

template <typename T>
class ObservableValueBase {
public:
  ObservableValueBase(std::mutex& mtx, std::condition_variable& cv, const T defaultValue = T()) :
    m_mutex(mtx), m_cv(cv), m_value(defaultValue), m_updated(false) {
  }

  void set(T value) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_value = value;
    m_updated = true;
    m_cv.notify_all();
  }

  void increment() {
    std::lock_guard<std::mutex> lk(m_mutex);
    ++m_value;
    m_updated = true;
    m_cv.notify_all();
  }

  T get() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_value;
  }

  bool waitFor(std::chrono::milliseconds ms, T& value) {
    std::unique_lock<std::mutex> lk(m_mutex);

    if (m_cv.wait_for(lk, ms, [this] { return m_updated; })) {
      value = m_value;
      m_updated = false;
      return true;
    }

    return false;
  }

  T wait() {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cv.wait(lk, [this] { return m_updated; });
    m_updated = false;
    return m_value;
  }

private:

  std::mutex& m_mutex;
  std::condition_variable& m_cv;

  T m_value;
  bool m_updated;
};
