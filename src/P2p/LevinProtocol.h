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

#include "CryptoNote.h"
#include <Common/MemoryInputStream.h>
#include <Common/VectorOutputStream.h>
#include "Serialization/KVBinaryInputStreamSerializer.h"
#include "Serialization/KVBinaryOutputStreamSerializer.h"

namespace System {
class TcpConnection;
}

namespace CryptoNote {

enum class LevinError: int32_t {
  OK = 0,
  ERROR_CONNECTION = -1,
  ERROR_CONNECTION_NOT_FOUND = -2,
  ERROR_CONNECTION_DESTROYED = -3,
  ERROR_CONNECTION_TIMEDOUT = -4,
  ERROR_CONNECTION_NO_DUPLEX_PROTOCOL = -5,
  ERROR_CONNECTION_HANDLER_NOT_DEFINED = -6,
  ERROR_FORMAT = -7,
};

const int32_t LEVIN_PROTOCOL_RETCODE_SUCCESS = 1;

class LevinProtocol {
public:

  LevinProtocol(System::TcpConnection& connection);

  template <typename Request, typename Response>
  bool invoke(uint32_t command, const Request& request, Response& response) {
    sendMessage(command, encode(request), true);

    Command cmd;
    readCommand(cmd);

    if (!cmd.isResponse) {
      return false;
    }

    return decode(cmd.buf, response); 
  }

  template <typename Request>
  void notify(uint32_t command, const Request& request, int) {
    sendMessage(command, encode(request), false);
  }

  struct Command {
    uint32_t command;
    bool isNotify;
    bool isResponse;
    BinaryArray buf;

    bool needReply() const;
  };

  bool readCommand(Command& cmd);

  void sendMessage(uint32_t command, const BinaryArray& out, bool needResponse);
  void sendReply(uint32_t command, const BinaryArray& out, int32_t returnCode);

  template <typename T>
  static bool decode(const BinaryArray& buf, T& value) {
    try {
      Common::MemoryInputStream stream(buf.data(), buf.size());
      KVBinaryInputStreamSerializer serializer(stream);
      serialize(value, serializer);
    } catch (std::exception&) {
      return false;
    }

    return true;
  }

  template <typename T>
  static BinaryArray encode(const T& value) {
    BinaryArray result;
    KVBinaryOutputStreamSerializer serializer;
    serialize(const_cast<T&>(value), serializer);
    Common::VectorOutputStream stream(result);
    serializer.dump(stream);
    return result;
  }

private:

  bool readStrict(uint8_t* ptr, size_t size);
  void writeStrict(const uint8_t* ptr, size_t size);
  System::TcpConnection& m_conn;
};

}
