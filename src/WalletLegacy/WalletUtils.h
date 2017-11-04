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

#include <exception>
#include <iomanip>
#include <iostream>

#include "IWalletLegacy.h"
#include "Wallet/WalletErrors.h"

namespace CryptoNote {

inline void throwIf(bool expr, CryptoNote::error::WalletErrorCodes ec)
{
  if (expr)
    throw std::system_error(make_error_code(ec));
}

inline std::ostream& operator <<(std::ostream& ostr, const Crypto::Hash& hash) {
  std::ios_base::fmtflags flags = ostr.setf(std::ios_base::hex, std::ios_base::basefield);
  char fill = ostr.fill('0');

  for (auto b : hash.data) {
    ostr << std::setw(2) << static_cast<unsigned int>(b);
  }

  ostr.fill(fill);
  ostr.setf(flags);
  return ostr;
}

} //namespace CryptoNote
