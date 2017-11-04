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

#include "MemoryMappedFile.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cassert>

#include "Common/ScopeExit.h"

namespace System {

MemoryMappedFile::MemoryMappedFile() :
  m_file(-1),
  m_size(0),
  m_data(nullptr) {
}

MemoryMappedFile::~MemoryMappedFile() {
  std::error_code ignore;
  close(ignore);
}

const std::string& MemoryMappedFile::path() const {
  assert(isOpened());

  return m_path;
}

uint64_t MemoryMappedFile::size() const {
  assert(isOpened());

  return m_size;
}

const uint8_t* MemoryMappedFile::data() const {
  assert(isOpened());

  return m_data;
}

uint8_t* MemoryMappedFile::data() {
  assert(isOpened());

  return m_data;
}

bool MemoryMappedFile::isOpened() const {
  return m_data != nullptr;
}

void MemoryMappedFile::create(const std::string& path, uint64_t size, bool overwrite, std::error_code& ec) {
  if (isOpened()) {
    close(ec);
    if (ec) {
      return;
    }
  }

  Tools::ScopeExit failExitHandler([this, &ec] {
    ec = std::error_code(errno, std::system_category());
    std::error_code ignore;
    close(ignore);
  });

  m_file = ::open(path.c_str(), O_RDWR | O_CREAT | (overwrite ? O_TRUNC : O_EXCL), S_IRUSR | S_IWUSR);
  if (m_file == -1) {
    return;
  }

  int result = ::ftruncate(m_file, static_cast<off_t>(size));
  if (result == -1) {
    return;
  }

  m_data = reinterpret_cast<uint8_t*>(::mmap(nullptr, static_cast<size_t>(size), PROT_READ | PROT_WRITE, MAP_SHARED, m_file, 0));
  if (m_data == MAP_FAILED) {
    return;
  }

  m_size = size;
  m_path = path;
  ec = std::error_code();

  failExitHandler.cancel();
}

void MemoryMappedFile::create(const std::string& path, uint64_t size, bool overwrite) {
  std::error_code ec;
  create(path, size, overwrite, ec);
  if (ec) {
    throw std::system_error(ec, "MemoryMappedFile::create");
  }
}

void MemoryMappedFile::open(const std::string& path, std::error_code& ec) {
  if (isOpened()) {
    close(ec);
    if (ec) {
      return;
    }
  }

  Tools::ScopeExit failExitHandler([this, &ec] {
    ec = std::error_code(errno, std::system_category());
    std::error_code ignore;
    close(ignore);
  });

  m_file = ::open(path.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
  if (m_file == -1) {
    return;
  }

  struct stat fileStat;
  int result = ::fstat(m_file, &fileStat);
  if (result == -1) {
    return;
  }

  m_size = static_cast<uint64_t>(fileStat.st_size);

  m_data = reinterpret_cast<uint8_t*>(::mmap(nullptr, static_cast<size_t>(m_size), PROT_READ | PROT_WRITE, MAP_SHARED, m_file, 0));
  if (m_data == MAP_FAILED) {
    return;
  }

  m_path = path;
  ec = std::error_code();

  failExitHandler.cancel();
}

void MemoryMappedFile::open(const std::string& path) {
  std::error_code ec;
  open(path, ec);
  if (ec) {
    throw std::system_error(ec, "MemoryMappedFile::open");
  }
}

void MemoryMappedFile::rename(const std::string& newPath, std::error_code& ec) {
  assert(isOpened());

  int result = ::rename(m_path.c_str(), newPath.c_str());
  if (result == 0) {
    m_path = newPath;
    ec = std::error_code();
  } else {
    ec = std::error_code(errno, std::system_category());
  }
}

void MemoryMappedFile::rename(const std::string& newPath) {
  assert(isOpened());

  std::error_code ec;
  rename(newPath, ec);
  if (ec) {
    throw std::system_error(ec, "MemoryMappedFile::rename");
  }
}

void MemoryMappedFile::close(std::error_code& ec) {
  int result;
  if (m_data != nullptr) {
    flush(m_data, m_size, ec);
    if (ec) {
      return;
    }

    result = ::munmap(m_data, static_cast<size_t>(m_size));
    if (result == 0) {
      m_data = nullptr;
    } else {
      ec = std::error_code(errno, std::system_category());
      return;
    }
  }

  if (m_file != -1) {
    result = ::close(m_file);
    if (result == 0) {
      m_file = -1;
      ec = std::error_code();
    } else {
      ec = std::error_code(errno, std::system_category());
      return;
    }
  }

  ec = std::error_code();
}

void MemoryMappedFile::close() {
  std::error_code ec;
  close(ec);
  if (ec) {
    throw std::system_error(ec, "MemoryMappedFile::close");
  }
}

void MemoryMappedFile::flush(uint8_t* data, uint64_t size, std::error_code& ec) {
  assert(isOpened());

  uintptr_t pageSize = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
  uintptr_t dataAddr = reinterpret_cast<uintptr_t>(data);
  uintptr_t pageOffset = (dataAddr / pageSize) * pageSize;

  int result = ::msync(reinterpret_cast<void*>(pageOffset), static_cast<size_t>(dataAddr % pageSize + size), MS_SYNC);
  if (result == 0) {
    result = ::fsync(m_file);
    if (result == 0) {
      ec = std::error_code();
      return;
    }
  }

  ec = std::error_code(errno, std::system_category());
}

void MemoryMappedFile::flush(uint8_t* data, uint64_t size) {
  assert(isOpened());

  std::error_code ec;
  flush(data, size, ec);
  if (ec) {
    throw std::system_error(ec, "MemoryMappedFile::flush");
  }
}

void MemoryMappedFile::swap(MemoryMappedFile& other) {
  std::swap(m_file, other.m_file);
  std::swap(m_path, other.m_path);
  std::swap(m_data, other.m_data);
  std::swap(m_size, other.m_size);
}

}
