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

#include "LevinProtocol.h"
#include <System/TcpConnection.h>

using namespace CryptoNote;

namespace {

const uint64_t LEVIN_SIGNATURE = 0x0101010101012101LL;  //Bender's nightmare
const uint32_t LEVIN_PACKET_REQUEST = 0x00000001;
const uint32_t LEVIN_PACKET_RESPONSE = 0x00000002;
const uint32_t LEVIN_DEFAULT_MAX_PACKET_SIZE = 100000000;      //100MB by default
const uint32_t LEVIN_PROTOCOL_VER_1 = 1;

#pragma pack(push)
#pragma pack(1)
struct bucket_head2
{
  uint64_t m_signature;
  uint64_t m_cb;
  bool     m_have_to_return_data;
  uint32_t m_command;
  int32_t  m_return_code;
  uint32_t m_flags;
  uint32_t m_protocol_version;
};
#pragma pack(pop)

}

bool LevinProtocol::Command::needReply() const {
  return !(isNotify || isResponse);
}

LevinProtocol::LevinProtocol(System::TcpConnection& connection) 
  : m_conn(connection) {}

void LevinProtocol::sendMessage(uint32_t command, const BinaryArray& out, bool needResponse) {
  bucket_head2 head = { 0 };
  head.m_signature = LEVIN_SIGNATURE;
  head.m_cb = out.size();
  head.m_have_to_return_data = needResponse;
  head.m_command = command;
  head.m_protocol_version = LEVIN_PROTOCOL_VER_1;
  head.m_flags = LEVIN_PACKET_REQUEST;

  // write header and body in one operation
  BinaryArray writeBuffer;
  writeBuffer.reserve(sizeof(head) + out.size());

  Common::VectorOutputStream stream(writeBuffer);
  stream.writeSome(&head, sizeof(head));
  stream.writeSome(out.data(), out.size());

  writeStrict(writeBuffer.data(), writeBuffer.size());
}

bool LevinProtocol::readCommand(Command& cmd) {
  bucket_head2 head = { 0 };

  if (!readStrict(reinterpret_cast<uint8_t*>(&head), sizeof(head))) {
    return false;
  }

  if (head.m_signature != LEVIN_SIGNATURE) {
    throw std::runtime_error("Levin signature mismatch");
  }

  if (head.m_cb > LEVIN_DEFAULT_MAX_PACKET_SIZE) {
    throw std::runtime_error("Levin packet size is too big");
  }

  BinaryArray buf;

  if (head.m_cb != 0) {
    buf.resize(head.m_cb);
    if (!readStrict(&buf[0], head.m_cb)) {
      return false;
    }
  }

  cmd.command = head.m_command;
  cmd.buf = std::move(buf);
  cmd.isNotify = !head.m_have_to_return_data;
  cmd.isResponse = (head.m_flags & LEVIN_PACKET_RESPONSE) == LEVIN_PACKET_RESPONSE;

  return true;
}

void LevinProtocol::sendReply(uint32_t command, const BinaryArray& out, int32_t returnCode) {
  bucket_head2 head = { 0 };
  head.m_signature = LEVIN_SIGNATURE;
  head.m_cb = out.size();
  head.m_have_to_return_data = false;
  head.m_command = command;
  head.m_protocol_version = LEVIN_PROTOCOL_VER_1;
  head.m_flags = LEVIN_PACKET_RESPONSE;
  head.m_return_code = returnCode;

  BinaryArray writeBuffer;
  writeBuffer.reserve(sizeof(head) + out.size());

  Common::VectorOutputStream stream(writeBuffer);
  stream.writeSome(&head, sizeof(head));
  stream.writeSome(out.data(), out.size());

  writeStrict(writeBuffer.data(), writeBuffer.size());
}

void LevinProtocol::writeStrict(const uint8_t* ptr, size_t size) {
  size_t offset = 0;
  while (offset < size) {
    offset += m_conn.write(ptr + offset, size - offset);
  }
}

bool LevinProtocol::readStrict(uint8_t* ptr, size_t size) {
  size_t offset = 0;
  while (offset < size) {
    size_t read = m_conn.read(ptr + offset, size - offset);
    if (read == 0) {
      return false;
    }

    offset += read;
  }

  return true;
}
