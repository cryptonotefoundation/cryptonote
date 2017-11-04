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

#include "Process.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace Tests {

  void Process::startChild(const std::string& executablePath, const std::vector<std::string>& args) {
   
#if defined WIN32
    std::stringstream ss;
    ss << "start /MIN " << executablePath;

    for (const auto& arg: args) {
      ss << " \"" << arg << "\"";
    }

    auto cmdline = ss.str();
    system(cmdline.c_str());

#else
    std::vector<const char*> cargs;
    cargs.push_back(executablePath.c_str());
    for (const auto& arg : args) {
      cargs.push_back(arg.c_str());
    }

    cargs.push_back(nullptr);

    auto pid = fork();

    if (pid == 0) {
      if (execv(executablePath.c_str(), (char**)&cargs[0]) == -1) {
        printf("Failed to start %s: %d\n", executablePath.c_str(), errno);
        exit(404);
      }
    } else if (pid > 0) {
      m_pid = pid;
    } else if (pid < 0) {
      throw std::runtime_error("fork() failed");
    }
#endif

  }

  void Process::wait() {
#ifndef _WIN32
    if (m_pid == 0) {
      return;
    }

    int status;
    waitpid(m_pid, &status, 0);
    m_pid = 0;
#endif
  }

}
