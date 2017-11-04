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

#if defined(__GNUC__)
#define INITIALIZER(name) __attribute__((constructor(101))) static void name(void)
#define FINALIZER(name) __attribute__((destructor(101))) static void name(void)
#define REGISTER_FINALIZER(name) ((void) 0)

#elif defined(_MSC_VER)
#include <assert.h>
#include <stdlib.h>
// http://stackoverflow.com/questions/1113409/attribute-constructor-equivalent-in-vc
// http://msdn.microsoft.com/en-us/library/bb918180.aspx
#pragma section(".CRT$XCT", read)
#define INITIALIZER(name) \
  static void __cdecl name(void); \
  __declspec(allocate(".CRT$XCT")) void (__cdecl *const _##name)(void) = &name; \
  static void __cdecl name(void)
#define FINALIZER(name) \
  static void __cdecl name(void)
#define REGISTER_FINALIZER(name) \
  do { \
    int _res = atexit(name); \
    assert(_res == 0); \
  } while (0);

#else
#error Unsupported compiler
#endif
