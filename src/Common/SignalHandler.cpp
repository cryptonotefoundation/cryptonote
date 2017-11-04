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

#include "SignalHandler.h"

#include <mutex>
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <signal.h>
#include <cstring>
#endif

namespace {

  std::function<void(void)> m_handler;

  void handleSignal() {
    static std::mutex m_mutex;
    std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
      return;
    }
    m_handler();
  }


#if defined(WIN32)
BOOL WINAPI winHandler(DWORD type) {
  if (CTRL_C_EVENT == type || CTRL_BREAK_EVENT == type) {
    handleSignal();
    return TRUE;
  } else {
    std::cerr << "Got control signal " << type << ". Exiting without saving...";
    return FALSE;
  }
  return TRUE;
}

#else

void posixHandler(int /*type*/) {
  handleSignal();
}
#endif

}


namespace Tools {

  bool SignalHandler::install(std::function<void(void)> t)
  {
#if defined(WIN32)
    bool r = TRUE == ::SetConsoleCtrlHandler(&winHandler, TRUE);
    if (r)  {
      m_handler = t;
    }
    return r;
#else
    struct sigaction newMask;
    std::memset(&newMask, 0, sizeof(struct sigaction));
    newMask.sa_handler = posixHandler;
    if (sigaction(SIGINT, &newMask, nullptr) != 0) {
      return false;
    }

    if (sigaction(SIGTERM, &newMask, nullptr) != 0) {
      return false;
    }

    std::memset(&newMask, 0, sizeof(struct sigaction));
    newMask.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &newMask, nullptr) != 0) {
      return false;
    }

    m_handler = t;
    return true;
#endif
  }

}
