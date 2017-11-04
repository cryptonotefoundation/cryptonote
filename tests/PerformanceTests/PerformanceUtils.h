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

#include <iostream>

#include <boost/config.hpp>

#ifdef BOOST_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void set_process_affinity(int core)
{
#if defined (__APPLE__)
    return;
#elif defined(BOOST_WINDOWS)
  DWORD_PTR mask = 1;
  for (int i = 0; i < core; ++i)
  {
    mask <<= 1;
  }
  ::SetProcessAffinityMask(::GetCurrentProcess(), core);
#elif defined(BOOST_HAS_PTHREADS)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  if (0 != ::pthread_setaffinity_np(::pthread_self(), sizeof(cpuset), &cpuset))
  {
    std::cout << "pthread_setaffinity_np - ERROR" << std::endl;
  }
#endif
}

void set_thread_high_priority()
{
#if defined(__APPLE__)
    return;
#elif defined(BOOST_WINDOWS)
  ::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#elif defined(BOOST_HAS_PTHREADS)
  pthread_attr_t attr;
  int policy = 0;
  int max_prio_for_policy = 0;

  ::pthread_attr_init(&attr);
  ::pthread_attr_getschedpolicy(&attr, &policy);
  max_prio_for_policy = ::sched_get_priority_max(policy);

  if (0 != ::pthread_setschedprio(::pthread_self(), max_prio_for_policy))
  {
    std::cout << "pthread_setschedprio - ERROR" << std::endl;
  }

  ::pthread_attr_destroy(&attr);
#endif
}
