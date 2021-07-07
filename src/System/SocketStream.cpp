// Copyright (c) 2017-2019 The Karbowanec developers
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

#include <string.h>
#include <streambuf>
#include <array>
#include <iostream>
#include <System/SocketStream.h>


namespace System {

SocketStreambuf::SocketStreambuf(char *data, size_t lenght) {
  this->lenght = lenght;
  setg(data, data, data + lenght);
  setp(reinterpret_cast<char*>(&writeBuf.front()), reinterpret_cast<char *>(&writeBuf.front() + writeBuf.max_size()));
}

SocketStreambuf::~SocketStreambuf() {
  this->dumpBuffer(true);
}

void SocketStreambuf::setRespdata(const std::vector<uint8_t> &data) {
  this->readBuf = data;
}

std::streambuf::int_type SocketStreambuf::overflow(std::streambuf::int_type ch) {
  if (ch == traits_type::eof()) {
    return traits_type::eof();
  }
  if (pptr() == epptr()) {
    if (!dumpBuffer(false)) {
      return traits_type::eof();
    }
  }
  *pptr() = static_cast<char>(ch);
  pbump(1);
  return ch;
}

std::streambuf::int_type SocketStreambuf::underflow() {
  if (gptr() < egptr()) {
    return traits_type::to_int_type(*gptr());
  }

  setg((char *) readBuf.data(), (char *) readBuf.data(), (char *) readBuf.data() + readBuf.size());

  return traits_type::to_int_type(*gptr());
}

int SocketStreambuf::sync(){
  return dumpBuffer(true) ? 0 : -1;
}

bool SocketStreambuf::dumpBuffer(bool finalize) {
  size_t count = pptr() - pbase();
  if (count == 0) {
    return true;
  }
  size_t resp_data_size = this->resp_data.size();
  this->resp_data.resize(resp_data_size + count);
  memcpy(this->resp_data.data() + resp_data_size, &writeBuf.front(), count);
  pbump(-static_cast<int>(count));
  return true;
}

void SocketStreambuf::getRespdata(std::vector<uint8_t> &data) {
  data = this->resp_data;
}

}

