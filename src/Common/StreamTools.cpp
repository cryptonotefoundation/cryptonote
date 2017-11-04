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

#include "StreamTools.h"
#include <stdexcept>
#include "IInputStream.h"
#include "IOutputStream.h"

namespace Common {

void read(IInputStream& in, void* data, size_t size) {
  while (size > 0) {
    size_t readSize = in.readSome(data, size);
    if (readSize == 0) {
      throw std::runtime_error("Failed to read from IInputStream");
    }

    data = static_cast<uint8_t*>(data) + readSize;
    size -= readSize;
  }
}

void read(IInputStream& in, int8_t& value) {
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, int16_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, int32_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, int64_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, uint8_t& value) {
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, uint16_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, uint32_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, uint64_t& value) {
  // TODO: Convert from little endian on big endian platforms
  read(in, &value, sizeof(value));
}

void read(IInputStream& in, std::vector<uint8_t>& data, size_t size) {
  data.resize(size);
  read(in, data.data(), size);
}

void read(IInputStream& in, std::string& data, size_t size) {
  std::vector<char> temp(size);
  read(in, temp.data(), size);
  data.assign(temp.data(), size);
}

void readVarint(IInputStream& in, uint8_t& value) {
  uint8_t temp = 0;
  for (uint8_t shift = 0;; shift += 7) {
    uint8_t piece;
    read(in, piece);
    if (shift >= sizeof(temp) * 8 - 7 && piece >= 1 << (sizeof(temp) * 8 - shift)) {
      throw std::runtime_error("readVarint, value overflow");
    }

    temp |= static_cast<size_t>(piece & 0x7f) << shift;
    if ((piece & 0x80) == 0) {
      if (piece == 0 && shift != 0) {
        throw std::runtime_error("readVarint, invalid value representation");
      }

      break;
    }
  }

  value = temp;
}

void readVarint(IInputStream& in, uint16_t& value) {
  uint16_t temp = 0;
  for (uint8_t shift = 0;; shift += 7) {
    uint8_t piece;
    read(in, piece);
    if (shift >= sizeof(temp) * 8 - 7 && piece >= 1 << (sizeof(temp) * 8 - shift)) {
      throw std::runtime_error("readVarint, value overflow");
    }

    temp |= static_cast<size_t>(piece & 0x7f) << shift;
    if ((piece & 0x80) == 0) {
      if (piece == 0 && shift != 0) {
        throw std::runtime_error("readVarint, invalid value representation");
      }

      break;
    }
  }

  value = temp;
}

void readVarint(IInputStream& in, uint32_t& value) {
  uint32_t temp = 0;
  for (uint8_t shift = 0;; shift += 7) {
    uint8_t piece;
    read(in, piece);
    if (shift >= sizeof(temp) * 8 - 7 && piece >= 1 << (sizeof(temp) * 8 - shift)) {
      throw std::runtime_error("readVarint, value overflow");
    }

    temp |= static_cast<size_t>(piece & 0x7f) << shift;
    if ((piece & 0x80) == 0) {
      if (piece == 0 && shift != 0) {
        throw std::runtime_error("readVarint, invalid value representation");
      }

      break;
    }
  }

  value = temp;
}

void readVarint(IInputStream& in, uint64_t& value) {
  uint64_t temp = 0;
  for (uint8_t shift = 0;; shift += 7) {
    uint8_t piece;
    read(in, piece);
    if (shift >= sizeof(temp) * 8 - 7 && piece >= 1 << (sizeof(temp) * 8 - shift)) {
      throw std::runtime_error("readVarint, value overflow");
    }

    temp |= static_cast<uint64_t>(piece & 0x7f) << shift;
    if ((piece & 0x80) == 0) {
      if (piece == 0 && shift != 0) {
        throw std::runtime_error("readVarint, invalid value representation");
      }

      break;
    }
  }

  value = temp;
}

void write(IOutputStream& out, const void* data, size_t size) {
  while (size > 0) {
    size_t writtenSize = out.writeSome(data, size);
    if (writtenSize == 0) {
      throw std::runtime_error("Failed to write to IOutputStream");
    }

    data = static_cast<const uint8_t*>(data) + writtenSize;
    size -= writtenSize;
  }
}

void write(IOutputStream& out, int8_t value) {
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, int16_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, int32_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, int64_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, uint8_t value) {
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, uint16_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, uint32_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, uint64_t value) {
  // TODO: Convert to little endian on big endian platforms
  write(out, &value, sizeof(value));
}

void write(IOutputStream& out, const std::vector<uint8_t>& data) {
  write(out, data.data(), data.size());
}

void write(IOutputStream& out, const std::string& data) {
  write(out, data.data(), data.size());
}

void writeVarint(IOutputStream& out, uint32_t value) {
  while (value >= 0x80) {
    write(out, static_cast<uint8_t>(value | 0x80));
    value >>= 7;
  }

  write(out, static_cast<uint8_t>(value));
}

void writeVarint(IOutputStream& out, uint64_t value) {
  while (value >= 0x80) {
    write(out, static_cast<uint8_t>(value | 0x80));
    value >>= 7;
  }

  write(out, static_cast<uint8_t>(value));
}

}
