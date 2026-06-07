// Copyright (c) 2017-2026, Karbo developers
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
// along with Karbo. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <string>
#include <vector>

namespace Common {

bool fetch_dns_txt(const std::string domain, std::vector<std::string>&records);

#ifndef __ANDROID__

  bool processServerAliasResponse(const std::string& s, std::string& address);
  std::string resolveAlias(const std::string& aliasUrl);
  std::vector<std::string> resolveAliases(const std::string& aliasUrl);

#endif

}
