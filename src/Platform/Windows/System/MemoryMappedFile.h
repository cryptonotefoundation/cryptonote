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

#include <cstdint>
#include <string>
#include <system_error>

namespace System {

class MemoryMappedFile {
public:
  MemoryMappedFile();
  ~MemoryMappedFile();

  void create(const std::string& path, uint64_t size, bool overwrite, std::error_code& ec);
  void create(const std::string& path, uint64_t size, bool overwrite);
  void open(const std::string& path, std::error_code& ec);
  void open(const std::string& path);
  void close(std::error_code& ec);
  void close();

  const std::string& path() const;
  uint64_t size() const;
  const uint8_t* data() const;
  uint8_t* data();
  bool isOpened() const;

  void rename(const std::string& newPath, std::error_code& ec);
  void rename(const std::string& newPath);

  void flush(uint8_t* data, uint64_t size, std::error_code& ec);
  void flush(uint8_t* data, uint64_t size);

  void swap(MemoryMappedFile& other);

private:
  void* m_fileHandle;
  void* m_mappingHandle;
  std::string m_path;
  uint64_t m_size;
  uint8_t* m_data;
};

}
