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

#include <algorithm>
#include <mutex>
#include <vector>

namespace Tools {

template<typename T>
class ObserverManager {
public:
  bool add(T* observer) {
    std::unique_lock<std::mutex> lock(m_observersMutex);
    auto it = std::find(m_observers.begin(), m_observers.end(), observer);
    if (m_observers.end() == it) {
      m_observers.push_back(observer);
      return true;
    } else {
      return false;
    }
  }

  bool remove(T* observer) {
    std::unique_lock<std::mutex> lock(m_observersMutex);

    auto it = std::find(m_observers.begin(), m_observers.end(), observer);
    if (m_observers.end() == it) {
      return false;
    } else {
      m_observers.erase(it);
      return true;
    }
  }

  void clear() {
    std::unique_lock<std::mutex> lock(m_observersMutex);
    m_observers.clear();
  }

#if defined(_MSC_VER)
  template<typename F>
  void notify(F notification) {
    std::vector<T*> observersCopy;
    {
      std::unique_lock<std::mutex> lock(m_observersMutex);
      observersCopy = m_observers;
    }

    for (T* observer : observersCopy) {
      (observer->*notification)();
    }
  }

  template<typename F, typename Arg0>
  void notify(F notification, const Arg0& arg0) {
    std::vector<T*> observersCopy;
    {
      std::unique_lock<std::mutex> lock(m_observersMutex);
      observersCopy = m_observers;
    }

    for (T* observer : observersCopy) {
      (observer->*notification)(arg0);
    }
  }

  template<typename F, typename Arg0, typename Arg1>
  void notify(F notification, const Arg0& arg0, const Arg1& arg1) {
    std::vector<T*> observersCopy;
    {
      std::unique_lock<std::mutex> lock(m_observersMutex);
      observersCopy = m_observers;
    }

    for (T* observer : observersCopy) {
      (observer->*notification)(arg0, arg1);
    }
  }

  template<typename F, typename Arg0, typename Arg1, typename Arg2>
  void notify(F notification, const Arg0& arg0, const Arg1& arg1, const Arg2& arg2) {
    std::vector<T*> observersCopy;
    {
      std::unique_lock<std::mutex> lock(m_observersMutex);
      observersCopy = m_observers;
    }

    for (T* observer : observersCopy) {
      (observer->*notification)(arg0, arg1, arg2);
    }
  }

  template<typename F, typename Arg0, typename Arg1, typename Arg2, typename Arg3>
  void notify(F notification, const Arg0& arg0, const Arg1& arg1, const Arg2& arg2, const Arg3& arg3) {
    std::vector<T*> observersCopy;
    {
      std::unique_lock<std::mutex> lock(m_observersMutex);
      observersCopy = m_observers;
    }

    for (T* observer : observersCopy) {
      (observer->*notification)(arg0, arg1, arg2, arg3);
    }
  }

  template<typename F, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  void notify(F notification, const Arg0& arg0, const Arg1& arg1, const Arg2& arg2, const Arg3& arg3, const Arg4& arg4) {
    std::vector<T*> observersCopy;
    {
      std::unique_lock<std::mutex> lock(m_observersMutex);
      observersCopy = m_observers;
    }

    for (T* observer : observersCopy) {
      (observer->*notification)(arg0, arg1, arg2, arg3, arg4);
    }
  }

  template<typename F, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
  void notify(F notification, const Arg0& arg0, const Arg1& arg1, const Arg2& arg2, const Arg3& arg3, const Arg4& arg4, const Arg5& arg5) {
    std::vector<T*> observersCopy;
    {
      std::unique_lock<std::mutex> lock(m_observersMutex);
      observersCopy = m_observers;
    }

    for (T* observer : observersCopy) {
      (observer->*notification)(arg0, arg1, arg2, arg3, arg4, arg5);
    }
  }

#else

  template<typename F, typename... Args>
  void notify(F notification, Args... args) {
    std::vector<T*> observersCopy;
    {
      std::unique_lock<std::mutex> lock(m_observersMutex);
      observersCopy = m_observers;
    }

    for (T* observer : observersCopy) {
      (observer->*notification)(args...);
    }
  }
#endif

private:
  std::vector<T*> m_observers;
  std::mutex m_observersMutex;
};

}
