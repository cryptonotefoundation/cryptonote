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

#include "Logger.h"

#include <algorithm>
#include <boost/date_time.hpp>

CLogger& CLogger::Instance()
{
	static CLogger theSingleInstance;
	return theSingleInstance;
}
void CLogger::init(LOG_LEVEL log_lvl)
{
  level_names[TRACE]   = "[ TRACE ]";
  level_names[DEBUG]   = "[ DEBUG ]";
  level_names[_ERROR]  = "[ ERROR ]";
  level_names[WARNING] = "[WARNING]";
  level_names[VERBOSE] = "[VERBOSE]";
  log_level = log_lvl;
  indent = 0;
}
void CLogger::Log(const std::string & log_info, LOG_LEVEL log_lvl, int indent_inc)
{
	if(log_lvl>=log_level)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (indent_inc<0)indent+=indent_inc;
		std::string sindent(std::max(0,indent),' ');
		if (indent_inc>0)indent+=indent_inc;
    (log_lvl<WARNING?std::cout:std::cerr)<<boost::posix_time::to_iso_extended_string(boost::posix_time::second_clock::local_time())<<level_names[log_lvl].c_str()<<sindent<<log_info.c_str()<<std::endl;
	}
}
