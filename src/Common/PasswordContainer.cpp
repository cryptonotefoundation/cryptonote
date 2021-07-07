// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.
// 
// Please see the included LICENSE file for more information.

//////////////////////////////
#include "PasswordContainer.h"
//////////////////////////////

#include <iostream>
#include <memory.h>
#include <stdio.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <Common/ColouredMsg.h>

namespace Tools
{
  namespace
  {
    bool is_cin_tty();
  }

  PasswordContainer::PasswordContainer()
    : m_empty(true)
  {
  }

  PasswordContainer::PasswordContainer(std::string&& password)
    : m_empty(false)
    , m_password(std::move(password))
  {
  }

  PasswordContainer::PasswordContainer(PasswordContainer&& rhs)
    : m_empty(std::move(rhs.m_empty))
    , m_password(std::move(rhs.m_password))
  {
  }

  PasswordContainer::~PasswordContainer()
  {
    clear();
  }

  void PasswordContainer::clear()
  {
    if (0 < m_password.capacity())
    {
      m_password.replace(0, m_password.capacity(), m_password.capacity(), '\0');
      m_password.resize(0);
    }
    m_empty = true;
  }

  bool PasswordContainer::read_password() {
    return read_password(false, "Enter password: ");
  }

  bool PasswordContainer::read_and_validate() {
	  std::string tmpPassword = m_password;

	  if (!read_password())
	  {
		  std::cout << "Failed to read password!";
		  return false;
	  }
	  bool validPass = m_password == tmpPassword;
	  m_password = tmpPassword;

	  return validPass;
  }

  bool PasswordContainer::read_password(bool verify)
  {
    clear();

    bool r;
    if (is_cin_tty())
    {
      std::cout << "password: ";
      if (verify)
      {
        std::string password1;
        std::string password2;
        r = read_from_tty(password1);
        if (r)
        {
          std::cout << "confirm password: ";
          r = read_from_tty(password2);
          if (r)
          {
            if (password1 == password2)
            {
              m_password = std::move(password2);
              m_empty = false;
	          return true;
            }
            else
            {
              std::cout << "Passwords do not match, try again." << std::endl;
              clear();
              return read_password(true);
            }
          }
        }
      }
      else
      {
        r = read_from_tty(m_password);
      }
    }
    else
    {
      r = read_from_file();
    }

    if (r)
    {
      m_empty = false;
    }
    else
    {
      clear();
    }

    return r;
  }

  bool PasswordContainer::read_and_validate(std::string msg) {
    std::string tmpPassword = m_password;

    if (msg == "") {
        if (!read_password()) {
            std::cout << WarningMsg("Failed to read password!") << std::endl;
            return false;
        }
    } else {
        if (!read_password(false, msg)) {
            std::cout << WarningMsg("Failed to read password!") << std::endl;
            return false;
        }
    }

    bool validPass = m_password == tmpPassword;

    m_password = tmpPassword;

    return validPass;
  }

  bool PasswordContainer::read_password(bool verify, std::string msg) {
    clear();

    bool r;
    if (is_cin_tty()) {
      std::cout << InformationMsg(msg);

      if (verify) {
        std::string password1;
        std::string password2;
        r = read_from_tty(password1);
        if (r) {
          std::cout << InformationMsg("Confirm your new password: ");
          r = read_from_tty(password2);
          if (r) {
            if (password1 == password2) {
              m_password = std::move(password2);
              m_empty = false;
	            return true;
            } else {
              std::cout << WarningMsg("Passwords do not match, try again.")
                        << std::endl;
              clear();
	            return read_password(true, msg);
            }
          }
	      }
      } else {
	      r = read_from_tty(m_password);
      }
    } else {
      r = read_from_file();
    }

    if (r) {
      m_empty = false;
    } else {
      clear();
    }

    return r;
  }

  bool PasswordContainer::read_from_file()
  {
    m_password.reserve(max_password_size);
    for (size_t i = 0; i < max_password_size; ++i)
    {
      char ch = static_cast<char>(std::cin.get());
      if (std::cin.eof() || ch == '\n' || ch == '\r')
      {
        break;
      }
      else if (std::cin.fail())
      {
        return false;
      }
      else
      {
        m_password.push_back(ch);
      }
    }

    return true;
  }

#if defined(_WIN32)

  namespace
  {
    bool is_cin_tty()
    {
      return 0 != _isatty(_fileno(stdin));
    }
  }

  bool PasswordContainer::read_from_tty(std::string& password)
  {
    const char BACKSPACE = 8;

    HANDLE h_cin = ::GetStdHandle(STD_INPUT_HANDLE);

    DWORD mode_old;
    ::GetConsoleMode(h_cin, &mode_old);
    DWORD mode_new = mode_old & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    ::SetConsoleMode(h_cin, mode_new);

    bool r = true;
    password.reserve(max_password_size);
    while (password.size() < max_password_size)
    {
      DWORD read;
      char ch;
      r = (TRUE == ::ReadConsoleA(h_cin, &ch, 1, &read, NULL));
      r &= (1 == read);
      if (!r)
      {
        break;
      }
      else if (ch == '\n' || ch == '\r')
      {
        std::cout << std::endl;
        break;
      }
      else if (ch == BACKSPACE)
      {
        if (!password.empty())
        {
          password.back() = '\0';
          password.resize(password.size() - 1);
          std::cout << "\b \b";
        }
      }
      else
      {
        password.push_back(ch);
        std::cout << '*';
      }
    }

    ::SetConsoleMode(h_cin, mode_old);

    return r;
  }

#else

  namespace
  {
    bool is_cin_tty()
    {
      return 0 != isatty(fileno(stdin));
    }

    int getch()
    {
      struct termios tty_old;
      tcgetattr(STDIN_FILENO, &tty_old);

      struct termios tty_new;
      tty_new = tty_old;
      tty_new.c_lflag &= ~(ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &tty_new);

      int ch = getchar();

      tcsetattr(STDIN_FILENO, TCSANOW, &tty_old);

      return ch;
    }
  }

  bool PasswordContainer::read_from_tty(std::string& password)
  {
    const char BACKSPACE = 127;

    password.reserve(max_password_size);
    while (password.size() < max_password_size)
    {
      int ch = getch();
      if (EOF == ch)
      {
        return false;
      }
      else if (ch == '\n' || ch == '\r')
      {
        std::cout << std::endl;
        break;
      }
      else if (ch == BACKSPACE)
      {
        if (!password.empty())
        {
          password.back() = '\0';
          password.resize(password.size() - 1);
          std::cout << "\b \b";
        }
      }
      else
      {
        password.push_back(ch);
        std::cout << '*';
      }
    }

    return true;
  }

#endif
}
