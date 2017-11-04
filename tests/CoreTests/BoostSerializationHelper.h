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

#include <fstream>
#include <memory>
#include <string>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

namespace Tools {

template <class t_object>
bool serialize_obj_to_file(t_object& obj, const std::string& file_path) {
  try {
    std::ofstream file(file_path);
    boost::archive::binary_oarchive a(file);
    a << obj;
    if (file.fail()) {
      return false;
    }

    file.flush();
    return true;
  } catch (std::exception&) {
    return false;
  }
}

template <class t_object>
bool unserialize_obj_from_file(t_object& obj, const std::string& file_path) {
  try {
    std::ifstream dataFile;
    dataFile.open(file_path, std::ios_base::binary | std::ios_base::in);
    if (dataFile.fail()) {
      return false;
    }

    boost::archive::binary_iarchive a(dataFile);
    a >> obj;
    return !dataFile.fail();
  } catch (std::exception&) {
    return false;
  }
}

}
