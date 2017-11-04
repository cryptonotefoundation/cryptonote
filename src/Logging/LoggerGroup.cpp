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

#include "LoggerGroup.h"
#include <algorithm>

namespace Logging {

LoggerGroup::LoggerGroup(Level level) : CommonLogger(level) {
}

void LoggerGroup::addLogger(ILogger& logger) {
  loggers.push_back(&logger);
}

void LoggerGroup::removeLogger(ILogger& logger) {
  loggers.erase(std::remove(loggers.begin(), loggers.end(), &logger), loggers.end());
}

void LoggerGroup::operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) {
  if (level <= logLevel && disabledCategories.count(category) == 0) {
    for (auto& logger : loggers) {
      (*logger)(category, level, time, body);
    }
  }
}

}
