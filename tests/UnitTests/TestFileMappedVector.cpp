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

#include <fstream>
#include <string>

#include <boost/filesystem.hpp>

#include "gtest/gtest.h"

#include "Common/FileMappedVector.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"

using namespace Common;

namespace Common {

void PrintTo(const FileMappedVector<char>::const_iterator& it, ::std::ostream* os) {
  *os << "<index=" << it.index() << ">";
}

void PrintTo(const FileMappedVector<char>::iterator& it, ::std::ostream* os) {
  *os << "<index=" << it.index() << ">";
}

}

namespace {

const std::string TEST_FILE_NAME = "FileMappedVectorTest.dat";
const std::string TEST_FILE_NAME_2 = "FileMappedVectorTest2.dat";
const std::string TEST_FILE_NAME_BAK = TEST_FILE_NAME + ".bak";
const std::string TEST_FILE_PREFIX = "!prefix!";
const std::string TEST_FILE_SUFFIX = "suffix";
const std::string TEST_VECTOR_DATA = "bytecoin";
const uint64_t TEST_VECTOR_SIZE = static_cast<uint64_t>(TEST_VECTOR_DATA.size());
const uint64_t TEST_VECTOR_CAPACITY = TEST_VECTOR_SIZE + 7;

class FileMappedVectorTest : public ::testing::Test {
public:

protected:
  virtual void SetUp() override {
    clean();
  }

  virtual void TearDown() override {
    clean();
  }

  void clean() {
    if (boost::filesystem::exists(TEST_FILE_NAME)) {
      boost::filesystem::remove_all(TEST_FILE_NAME);
    }

    if (boost::filesystem::exists(TEST_FILE_NAME_2)) {
      boost::filesystem::remove_all(TEST_FILE_NAME_2);
    }

    if (boost::filesystem::exists(TEST_FILE_NAME_BAK)) {
      boost::filesystem::remove_all(TEST_FILE_NAME_BAK);
    }
  }

  template<class Iterator>
  void createTestFile(const std::string& path, uint64_t capacity, Iterator first, Iterator last, const std::string& prefix, const std::string& suffix) {
    uint64_t size = static_cast<uint64_t>(std::distance(first, last));
    if (capacity < size) {
      throw std::invalid_argument("capacity is less than size");
    }

    std::ofstream stream(path, std::ios_base::binary);
    stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    stream.write(prefix.data(), prefix.size());
    stream.write(reinterpret_cast<const char*>(&capacity), sizeof(capacity));
    stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
    std::copy(first, last, std::ostream_iterator<char>(stream));

    for (size_t i = size; i < capacity; ++i) {
      stream.put('w');
    }

    stream.write(suffix.data(), suffix.size());
  }

  void createTestFile(const std::string& path, uint64_t capacity, const std::string& data) {
    createTestFile(path, capacity, data.begin(), data.end(), "", "");
  }

  void createTestFile(const std::string& path) {
    createTestFile(path, TEST_VECTOR_CAPACITY, TEST_VECTOR_DATA.begin(), TEST_VECTOR_DATA.end(), "", "");
  }

  void createTestFileWithPrefixAndSuffix(const std::string& path) {
    createTestFile(path, TEST_VECTOR_CAPACITY, TEST_VECTOR_DATA.begin(), TEST_VECTOR_DATA.end(), TEST_FILE_PREFIX, TEST_FILE_SUFFIX);
  }

  static void readVectorFile(const std::string& path, uint64_t prefixSize, uint64_t* capacity = nullptr, uint64_t* size = nullptr,
    std::vector<char>* data = nullptr, std::vector<char>* prefix = nullptr, std::vector<char>* suffix = nullptr) {

    std::ifstream stream(path, std::ios_base::binary);
    stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    stream.seekg(0, std::ios_base::end);
    auto fileSize = stream.tellg();
    stream.seekg(0, std::ios_base::beg);

    std::vector<char> tmp;
    tmp.resize(prefixSize);
    if (!tmp.empty()) {
      stream.read(tmp.data(), tmp.size());
    }

    if (prefix != nullptr) {
      prefix->swap(tmp);
    }

    if (capacity != nullptr) {
      stream.read(reinterpret_cast<char*>(capacity), sizeof(*capacity));

      if (size != nullptr) {
        stream.read(reinterpret_cast<char*>(size), sizeof(*size));

        if (data != nullptr) {
          data->resize(*size);
          if (!data->empty()) {
            stream.read(data->data(), data->size());
          }

          if (suffix != nullptr) {
            tmp.resize(*capacity - *size);
            if (!tmp.empty()) {
              stream.read(tmp.data(), tmp.size());
            }

            suffix->resize(fileSize - stream.tellg());
            if (!suffix->empty()) {
              stream.read(suffix->data(), suffix->size());
            }
          }
        }
      }
    }
  }

  static void readVectorFile(const std::string& path, uint64_t* capacity = nullptr, uint64_t* size = nullptr, std::vector<char>* data = nullptr) {
    readVectorFile(path, 0, capacity, size, data);
  }
};

TEST_F(FileMappedVectorTest, constructorOpensFileIfModeIsOpenAndFileExists) {
  createTestFile(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_NO_THROW({
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);
  });
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfModeIsOpenAndFileDoesNotExist) {
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_ANY_THROW({
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);
  });
}

TEST_F(FileMappedVectorTest, constructorCreatesFileIfModeIsCreateAndFileDoesNotExists) {
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_NO_THROW({
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::CREATE);
  });
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfModeIsCreateAndFileExists) {
  createTestFile(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_ANY_THROW({
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::CREATE);
  });
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfModeIsCreateAndBakFileExists) {
  createTestFile(TEST_FILE_NAME_BAK);

  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_ANY_THROW({
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::CREATE);
  });
}

TEST_F(FileMappedVectorTest, constructorOpensFileIfModeIsOpenOrCreateAndFileExists) {
  createTestFile(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_NO_THROW({
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN_OR_CREATE);
  });
}

TEST_F(FileMappedVectorTest, constructorCreatesFileIfModeIsOpenOrCreateAndFileDoesNotExist) {
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_NO_THROW({
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN_OR_CREATE);
  });
}

TEST_F(FileMappedVectorTest, constructorCreatesEmptyFile) {
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  {
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::CREATE);
  }

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_GT(boost::filesystem::file_size(TEST_FILE_NAME), sizeof(uint64_t));

  uint64_t capacity;
  uint64_t size;
  readVectorFile(TEST_FILE_NAME, &capacity, &size);
  ASSERT_LE(0, capacity);
  ASSERT_EQ(0, size);
}

TEST_F(FileMappedVectorTest, constructorCreatesEmptyFileWithPrefix) {
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  {
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::CREATE, TEST_FILE_PREFIX.size());
  }

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_GT(boost::filesystem::file_size(TEST_FILE_NAME), sizeof(uint64_t));

  uint64_t capacity;
  uint64_t size;
  std::vector<char> data;
  std::vector<char> prefix;
  std::vector<char> suffix;
  readVectorFile(TEST_FILE_NAME, TEST_FILE_PREFIX.size(), &capacity, &size, &data, &prefix, &suffix);
  ASSERT_LE(0, capacity);
  ASSERT_EQ(0, size);
  ASSERT_TRUE(data.empty());
  ASSERT_EQ(TEST_FILE_PREFIX.size(), prefix.size());
  ASSERT_TRUE(suffix.empty());
}

TEST_F(FileMappedVectorTest, constructorCorrectlyOpensExistentFile) {
  createTestFile(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);
  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
}

TEST_F(FileMappedVectorTest, constructorCorrectlyOpensFileWithPrefixAndSuffix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
  ASSERT_EQ(TEST_FILE_PREFIX, std::string(vec.prefix(), vec.prefix() + vec.prefixSize()));
  ASSERT_EQ(TEST_FILE_SUFFIX, std::string(vec.suffix(), vec.suffix() + vec.suffixSize()));
}

TEST_F(FileMappedVectorTest, constructorOpensFileIfItExistsAndRemovesBakFileIfItExists) {
  createTestFile(TEST_FILE_NAME);
  createTestFile(TEST_FILE_NAME_BAK, 10, std::string("bak"));

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
}

TEST_F(FileMappedVectorTest, constructorOpensAndRenamesBakFileIfItExists) {
  createTestFile(TEST_FILE_NAME_BAK);

  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfFailedToOpenExistentFile) {
  boost::filesystem::create_directory(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  try {
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);
    ASSERT_TRUE(false);
  } catch (...) {
  }
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfFailedToOpenExistentBakFile) {
  boost::filesystem::create_directory(TEST_FILE_NAME_BAK);

  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  try {
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);
    ASSERT_TRUE(false);
  } catch (...) {
  }
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfFailedToRemoveBakFile) {
  createTestFile(TEST_FILE_NAME);
  boost::filesystem::create_directory(TEST_FILE_NAME_BAK);
  boost::filesystem::create_directory(boost::filesystem::path(TEST_FILE_NAME_BAK) / TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  try {
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);
    ASSERT_TRUE(false);
  } catch (...) {
  }
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfFileDoesNotContainMetadata) {
  std::ofstream stream(TEST_FILE_NAME, std::ios_base::binary);
  stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  std::string content(FileMappedVector<char>::metadataSize - 1, '\0');
  stream.write(content.data(), content.size());
  stream.close();

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));

  try {
    FileMappedVector<char> vec(TEST_FILE_NAME);
    ASSERT_TRUE(false);
  } catch (const std::runtime_error&) {
  }
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfFileSizeIsLessThanCapacity) {
  std::ofstream stream(TEST_FILE_NAME, std::ios_base::binary);
  stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  uint64_t capacity = 1;
  uint64_t size = 0;
  stream.write(reinterpret_cast<const char*>(&capacity), sizeof(capacity));
  stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
  stream.close();

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));

  try {
    FileMappedVector<uint64_t> vec(TEST_FILE_NAME);
    ASSERT_TRUE(false);
  } catch (const std::runtime_error&) {
  }
}

TEST_F(FileMappedVectorTest, constructorThrowsExceptionIfFileCapacityIsLessThanVectorSize) {
  std::ofstream stream(TEST_FILE_NAME, std::ios_base::binary);
  stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  uint64_t capacity = 0;
  uint64_t size = 1;
  stream.write(reinterpret_cast<const char*>(&capacity), sizeof(capacity));
  stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
  stream.close();

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));

  try {
    FileMappedVector<uint64_t> vec(TEST_FILE_NAME);
    ASSERT_TRUE(false);
  } catch (const std::runtime_error&) {
  }
}

TEST_F(FileMappedVectorTest, constructorCanOpenFileWithZeroCapacity) {
  createTestFile(TEST_FILE_NAME, 0, "");

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN);
  ASSERT_EQ(0, vec.size());
  ASSERT_EQ(0, vec.capacity());
}

TEST_F(FileMappedVectorTest, destructorFlushesAllChangesToDisk) {
  {
    FileMappedVector<char> vec(TEST_FILE_NAME);
    vec.push_back('a');
    vec[0] = 'b';
  }

  uint64_t capacity;
  uint64_t size;
  std::vector<char> data;
  readVectorFile(TEST_FILE_NAME, &capacity, &size, &data);
  ASSERT_LE(1, capacity);
  ASSERT_EQ(1, size);
  ASSERT_EQ('b', data.front());
}

TEST_F(FileMappedVectorTest, newVectorIsEmpty) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  ASSERT_TRUE(vec.empty());
  ASSERT_EQ(0, vec.size());
}

TEST_F(FileMappedVectorTest, reserveIncreasesCapacity) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  uint64_t newCapacity = vec.capacity() + 1;
  vec.reserve(newCapacity);
  ASSERT_EQ(newCapacity, vec.capacity());
}

TEST_F(FileMappedVectorTest, reserveDoesNotDecreaseCapacity) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  uint64_t initialCapacity = vec.capacity();
  vec.reserve(initialCapacity + 1);
  vec.reserve(initialCapacity - 1);
  ASSERT_EQ(initialCapacity + 1, vec.capacity());
}

TEST_F(FileMappedVectorTest, reservePreservesFilePrefixAndSuffix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);
  uint64_t newCapacity = TEST_VECTOR_CAPACITY + 1;

  {
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
    vec.reserve(newCapacity);
  }

  uint64_t capacity;
  uint64_t size;
  std::vector<char> data;
  std::vector<char> prefix;
  std::vector<char> suffix;
  readVectorFile(TEST_FILE_NAME, TEST_FILE_PREFIX.size(), &capacity, &size, &data, &prefix, &suffix);
  ASSERT_EQ(newCapacity, capacity);
  ASSERT_EQ(TEST_VECTOR_SIZE, size);
  ASSERT_EQ(TEST_VECTOR_DATA, asString(data.data(), data.size()));
  ASSERT_EQ(TEST_FILE_PREFIX, asString(prefix.data(), prefix.size()));
  ASSERT_EQ(TEST_FILE_SUFFIX, asString(suffix.data(), suffix.size()));
}

TEST_F(FileMappedVectorTest, shrinkToFitSetCapacityToSize) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  while (vec.size() == vec.capacity()) {
    vec.push_back('w');
  }

  ASSERT_LT(vec.size(), vec.capacity());
  vec.shrink_to_fit();
  ASSERT_EQ(vec.size(), vec.capacity());
}

TEST_F(FileMappedVectorTest, beginReturnsIteratorPointsToFirstElement) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.push_back('b');
  ASSERT_EQ('a', *vec.begin());
}

TEST_F(FileMappedVectorTest, beginReturnsNonConstIterator) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  auto it = vec.begin();
  *it = 'b';
  ASSERT_EQ('b', vec[0]);
}

TEST_F(FileMappedVectorTest, beginAndEndAreEqualForEmptyVector) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  ASSERT_EQ(vec.begin(), vec.end());
}

TEST_F(FileMappedVectorTest, endReturnsIteratorPointsAfterLastElement) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('w');
  auto it = vec.begin();
  ++it;
  ASSERT_EQ(it, vec.end());
}

TEST_F(FileMappedVectorTest, squareBracketsOperatorReturnsCorrectElement) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.push_back('b');
  vec.push_back('c');

  ASSERT_EQ('a', vec[0]);
  ASSERT_EQ('b', vec[1]);
  ASSERT_EQ('c', vec[2]);
}

TEST_F(FileMappedVectorTest, squareBracketsOperatorReturnsNonConstReference) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec[0] = 'b';
  ASSERT_EQ('b', vec[0]);
}

TEST_F(FileMappedVectorTest, atThrowsOutOfRangeException) {
  FileMappedVector<char> vec(TEST_FILE_NAME);

  ASSERT_ANY_THROW(vec.at(0));

  vec.push_back('a');
  vec[0] = 'b';
  ASSERT_THROW(vec.at(1), std::out_of_range);
}

TEST_F(FileMappedVectorTest, atReturnsNonConstReference) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.at(0) = 'b';
  ASSERT_EQ('b', vec.at(0));
}

TEST_F(FileMappedVectorTest, frontReturnsFirstElement) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.push_back('b');
  ASSERT_EQ('a', vec.front());
}

TEST_F(FileMappedVectorTest, frontReturnsNonConstIterator) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.front() = 'w';
  ASSERT_EQ('w', vec[0]);
}

TEST_F(FileMappedVectorTest, backReturnsLastElement) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.push_back('b');
  ASSERT_EQ('b', vec.back());
}

TEST_F(FileMappedVectorTest, backReturnsNonConstIterator) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.push_back('b');
  vec.back() = 'w';
  ASSERT_EQ('w', vec[1]);
}

TEST_F(FileMappedVectorTest, dataReturnsPointerToVectorData) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.push_back('b');
  auto* data = vec.data();
  ASSERT_EQ('a', data[0]);
  ASSERT_EQ('b', data[1]);
}

TEST_F(FileMappedVectorTest, vectorDataCanBeChangedViaPointer) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  auto* data = vec.data();
  data[0] = 'b';
  ASSERT_EQ('b', vec[0]);
}

TEST_F(FileMappedVectorTest, clearRemovesAllElements) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  ASSERT_FALSE(vec.empty());
  vec.clear();
  ASSERT_TRUE(vec.empty());
}

TEST_F(FileMappedVectorTest, eraseCanRemoveTheFirstElement) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  vec.erase(vec.begin());
  ASSERT_EQ(TEST_VECTOR_SIZE - 1, vec.size());
  ASSERT_TRUE(std::equal(vec.begin(), vec.end(), TEST_VECTOR_DATA.begin() + 1));
}

TEST_F(FileMappedVectorTest, eraseCanRemoveTheLastElement) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  vec.erase(vec.end() - 1);
  ASSERT_EQ(TEST_VECTOR_SIZE - 1, vec.size());
  ASSERT_TRUE(std::equal(vec.begin(), vec.end(), TEST_VECTOR_DATA.begin()));
}

TEST_F(FileMappedVectorTest, eraseCanRemoveOneMiddleElement) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  vec.erase(vec.begin() + 1);
  ASSERT_EQ(TEST_VECTOR_SIZE - 1, vec.size());
  ASSERT_EQ(TEST_VECTOR_DATA[0], vec[0]);
  ASSERT_TRUE(std::equal(vec.begin() + 1, vec.end(), TEST_VECTOR_DATA.begin() + 2));
}

TEST_F(FileMappedVectorTest, eraseCanRemoveAllMiddleElements) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  vec.erase(vec.begin() + 1, vec.end() - 1);
  ASSERT_EQ(2, vec.size());
  ASSERT_EQ(TEST_VECTOR_DATA.front(), vec.front());
  ASSERT_EQ(TEST_VECTOR_DATA.back(), vec.back());
}

TEST_F(FileMappedVectorTest, eraseCanRemoveAllElements) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  vec.erase(vec.begin(), vec.end());
  ASSERT_TRUE(vec.empty());
}

TEST_F(FileMappedVectorTest, eraseReturnsIteratorPointsToFirstElementAfterErased) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  auto it = vec.erase(vec.begin() + 1);
  ASSERT_EQ(vec.cbegin() + 1, it);
}

TEST_F(FileMappedVectorTest, erasePreservesFilePrefixAndSuffix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  {
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
    vec.erase(vec.begin() + 1, vec.end());
  }

  uint64_t capacity;
  uint64_t size;
  std::vector<char> data;
  std::vector<char> prefix;
  std::vector<char> suffix;
  readVectorFile(TEST_FILE_NAME, TEST_FILE_PREFIX.size(), &capacity, &size, &data, &prefix, &suffix);
  ASSERT_EQ(TEST_VECTOR_CAPACITY, capacity);
  ASSERT_EQ(1, size);
  ASSERT_EQ(1, data.size());
  ASSERT_EQ(TEST_VECTOR_DATA.front(), data.front());
  ASSERT_EQ(TEST_FILE_PREFIX, asString(prefix.data(), prefix.size()));
  ASSERT_EQ(TEST_FILE_SUFFIX, asString(suffix.data(), suffix.size()));
}

TEST_F(FileMappedVectorTest, insertCanAddElementsToEmptyVector) {
  FileMappedVector<char> vec(TEST_FILE_NAME);

  char c = 'w';
  vec.insert(vec.begin(), c);
  ASSERT_EQ(1, vec.size());
  ASSERT_EQ(c, vec.front());
}

TEST_F(FileMappedVectorTest, insertCanAddElementToFront) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  char c = 'w';
  vec.insert(vec.begin(), c);
  ASSERT_EQ(TEST_VECTOR_SIZE + 1, vec.size());
  ASSERT_EQ(c, vec.front());
  ASSERT_TRUE(std::equal(vec.begin() + 1, vec.end(), TEST_VECTOR_DATA.begin()));
}

TEST_F(FileMappedVectorTest, insertCanAddElementToBack) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  char c = 'w';
  vec.insert(vec.end(), c);
  ASSERT_EQ(TEST_VECTOR_SIZE + 1, vec.size());
  ASSERT_EQ(c, vec.back());
  ASSERT_TRUE(std::equal(vec.begin(), vec.end() - 1, TEST_VECTOR_DATA.begin()));
}

TEST_F(FileMappedVectorTest, insertCanAddOneElementToMiddle) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  char c = 'w';
  vec.insert(vec.begin() + 1, c);
  ASSERT_EQ(TEST_VECTOR_SIZE + 1, vec.size());
  ASSERT_EQ(TEST_VECTOR_DATA[0], vec[0]);
  ASSERT_EQ(c, vec[1]);
  ASSERT_TRUE(std::equal(vec.begin() + 2, vec.end(), TEST_VECTOR_DATA.begin() + 1));
}

TEST_F(FileMappedVectorTest, insertCanAddSeveralElementsToMiddle) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  std::string str = "www";
  vec.insert(vec.begin() + 1, str.begin(), str.end());
  ASSERT_EQ(TEST_VECTOR_SIZE + str.size(), vec.size());
  ASSERT_EQ(TEST_VECTOR_DATA[0], vec[0]);
  ASSERT_TRUE(std::equal(vec.begin() + 1, vec.begin() + 1 + str.size(), str.begin()));
  ASSERT_TRUE(std::equal(vec.begin() + 1 + str.size(), vec.end(), TEST_VECTOR_DATA.begin() + 1));
}

TEST_F(FileMappedVectorTest, insertCanIncreaseCapacity) {
  FileMappedVector<char> vec(TEST_FILE_NAME);

  char c = 'w';
  uint64_t initialCapacity = vec.capacity();
  uint64_t insertCount = initialCapacity - vec.size() + 1;
  for (uint64_t i = 0; i < insertCount; ++i) {
    vec.insert(vec.begin(), c);
  }

  ASSERT_LT(initialCapacity, vec.capacity());
}

TEST_F(FileMappedVectorTest, insertReturnsIteratorPointsToFirstInsertedElement) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  std::string str = "abc";
  auto it = vec.insert(vec.begin() + 1, str.begin(), str.end());
  ASSERT_EQ(vec.cbegin() + 1, it);
  ASSERT_EQ('a', *it);
}

TEST_F(FileMappedVectorTest, insertPreservesFilePrefixAndSuffix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  uint64_t insertDataSize = TEST_VECTOR_CAPACITY - TEST_VECTOR_SIZE + 1;
  std::string insertData(insertDataSize, 'w');
  uint64_t newVectorSize = TEST_VECTOR_SIZE + insertDataSize;

  {
    FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
    vec.insert(vec.begin(), insertData.begin(), insertData.end());
  }

  uint64_t capacity;
  uint64_t size;
  std::vector<char> data;
  std::vector<char> prefix;
  std::vector<char> suffix;
  readVectorFile(TEST_FILE_NAME, TEST_FILE_PREFIX.size(), &capacity, &size, &data, &prefix, &suffix);
  ASSERT_LT(TEST_VECTOR_CAPACITY, capacity);
  ASSERT_EQ(newVectorSize, size);
  ASSERT_EQ(insertData + TEST_VECTOR_DATA, asString(data.data(), data.size()));
  ASSERT_EQ(TEST_FILE_PREFIX, asString(prefix.data(), prefix.size()));
  ASSERT_EQ(TEST_FILE_SUFFIX, asString(suffix.data(), suffix.size()));
}

TEST_F(FileMappedVectorTest, pushBackCanAppendElementToEmptyVector) {
  FileMappedVector<char> vec(TEST_FILE_NAME);

  char c = 'w';
  vec.push_back(c);
  ASSERT_EQ(1, vec.size());
  ASSERT_EQ(c, vec.front());
}

TEST_F(FileMappedVectorTest, pushBackCanAppendElementToNonEmptyVector) {
  FileMappedVector<char> vec(TEST_FILE_NAME);

  char c1 = 'w';
  char c2 = 'q';
  vec.push_back(c1);
  vec.push_back(c2);
  ASSERT_EQ(2, vec.size());
  ASSERT_EQ(c1, vec.front());
  ASSERT_EQ(c2, vec.back());
}

TEST_F(FileMappedVectorTest, pushBackCanIncreaseCapacity) {
  FileMappedVector<char> vec(TEST_FILE_NAME);

  char c = 'w';
  uint64_t initialCapacity = vec.capacity();
  uint64_t insertCount = initialCapacity - vec.size() + 1;
  for (uint64_t i = 0; i < insertCount; ++i) {
    vec.push_back(c);
  }

  ASSERT_LT(initialCapacity, vec.capacity());
}

TEST_F(FileMappedVectorTest, pushBackFlushesDataToDiskImmediately) {
  char c = Crypto::rand<char>();

  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back(c);

  uint64_t capacity;
  uint64_t size;
  std::vector<char> data;
  readVectorFile(TEST_FILE_NAME, &capacity, &size, &data);
  ASSERT_LE(1, capacity);
  ASSERT_EQ(1, size);
  ASSERT_EQ(c, data.front());
}

TEST_F(FileMappedVectorTest, popBackRemovesLastElement) {
  createTestFile(TEST_FILE_NAME);
  FileMappedVector<char> vec(TEST_FILE_NAME);

  vec.pop_back();
  ASSERT_EQ(TEST_VECTOR_DATA.size() - 1, vec.size());
  ASSERT_TRUE(std::equal(vec.begin(), vec.end(), TEST_VECTOR_DATA.begin()));
}

TEST_F(FileMappedVectorTest, popBackRemovesTheOnlyElement) {
  FileMappedVector<char> vec(TEST_FILE_NAME);

  vec.push_back('w');
  vec.pop_back();
  ASSERT_TRUE(vec.empty());
}

TEST_F(FileMappedVectorTest, swapWorksCorrectly) {
  FileMappedVector<char> vec1(TEST_FILE_NAME);
  FileMappedVector<char> vec2(TEST_FILE_NAME_2);

  vec1.push_back('a');
  vec1.push_back('b');

  vec2.push_back('c');

  vec1.swap(vec2);

  ASSERT_EQ(1, vec1.size());
  ASSERT_EQ('c', vec1[0]);

  ASSERT_EQ(2, vec2.size());
  ASSERT_EQ('a', vec2[0]);
  ASSERT_EQ('b', vec2[1]);
}

TEST_F(FileMappedVectorTest, resizePrefixCorrectlyShrinksPrefix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
  vec.resizePrefix(TEST_FILE_PREFIX.size() - 1);

  ASSERT_EQ(TEST_FILE_PREFIX.size() - 1, vec.prefixSize());
  ASSERT_EQ(TEST_FILE_PREFIX.substr(0, TEST_FILE_PREFIX.size() - 1), std::string(vec.prefix(), vec.prefix() + vec.prefixSize()));

  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
  ASSERT_EQ(TEST_FILE_SUFFIX, std::string(vec.suffix(), vec.suffix() + vec.suffixSize()));
}

TEST_F(FileMappedVectorTest, resizePrefixCorrectlyExpandsPrefix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
  vec.resizePrefix(TEST_FILE_PREFIX.size() + 1);

  ASSERT_EQ(TEST_FILE_PREFIX.size() + 1, vec.prefixSize());
  ASSERT_EQ(TEST_FILE_PREFIX, std::string(vec.prefix(), vec.prefix() + vec.prefixSize() - 1));

  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
  ASSERT_EQ(TEST_FILE_SUFFIX, std::string(vec.suffix(), vec.suffix() + vec.suffixSize()));
}

TEST_F(FileMappedVectorTest, resizePrefixCanRemovePrefix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
  ASSERT_LT(0, vec.prefixSize());
  vec.resizePrefix(0);
  ASSERT_EQ(0, vec.prefixSize());

  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
  ASSERT_EQ(TEST_FILE_SUFFIX, std::string(vec.suffix(), vec.suffix() + vec.suffixSize()));
}

TEST_F(FileMappedVectorTest, resizePrefixCanAddPrefixIfItDidNotExist) {
  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::CREATE, 0);
  ASSERT_EQ(0, vec.prefixSize());

  vec.resizePrefix(TEST_FILE_PREFIX.size());
  std::copy(TEST_FILE_PREFIX.begin(), TEST_FILE_PREFIX.end(), vec.prefix());

  ASSERT_EQ(TEST_FILE_PREFIX.size(), vec.prefixSize());
  ASSERT_EQ(TEST_FILE_PREFIX, std::string(vec.prefix(), vec.prefix() + vec.prefixSize()));

  ASSERT_EQ(0, vec.size());
  ASSERT_LE(0, vec.capacity());
}

TEST_F(FileMappedVectorTest, resizeSuffixCorrectlyShrinksSuffix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
  vec.resizeSuffix(TEST_FILE_SUFFIX.size() - 1);

  ASSERT_EQ(TEST_FILE_SUFFIX.size() - 1, vec.suffixSize());
  ASSERT_EQ(TEST_FILE_SUFFIX.substr(0, TEST_FILE_SUFFIX.size() - 1), std::string(vec.suffix(), vec.suffix() + vec.suffixSize()));

  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
  ASSERT_EQ(TEST_FILE_PREFIX, std::string(vec.prefix(), vec.prefix() + vec.prefixSize()));
}

TEST_F(FileMappedVectorTest, resizeSuffixCorrectlyExpandsSuffix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
  vec.resizeSuffix(TEST_FILE_SUFFIX.size() + 1);

  ASSERT_EQ(TEST_FILE_SUFFIX.size() + 1, vec.suffixSize());
  ASSERT_EQ(TEST_FILE_SUFFIX, std::string(vec.suffix(), vec.suffix() + vec.suffixSize() - 1));

  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
  ASSERT_EQ(TEST_FILE_PREFIX, std::string(vec.prefix(), vec.prefix() + vec.prefixSize()));
}

TEST_F(FileMappedVectorTest, resizeSuffixCanRemoveSuffix) {
  createTestFileWithPrefixAndSuffix(TEST_FILE_NAME);

  ASSERT_TRUE(boost::filesystem::exists(TEST_FILE_NAME));
  ASSERT_FALSE(boost::filesystem::exists(TEST_FILE_NAME_BAK));

  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::OPEN, TEST_FILE_PREFIX.size());
  ASSERT_LT(0, vec.suffixSize());
  vec.resizeSuffix(0);
  ASSERT_EQ(0, vec.suffixSize());

  ASSERT_EQ(TEST_VECTOR_SIZE, vec.size());
  ASSERT_EQ(TEST_VECTOR_CAPACITY, vec.capacity());
  ASSERT_EQ(TEST_VECTOR_DATA, std::string(vec.data(), vec.size()));
  ASSERT_EQ(TEST_FILE_PREFIX, std::string(vec.prefix(), vec.prefix() + vec.prefixSize()));
}

TEST_F(FileMappedVectorTest, resizeSuffixCanAddSuffixIfItDidNotExist) {
  FileMappedVector<char> vec(TEST_FILE_NAME, FileMappedVectorOpenMode::CREATE, 0);
  ASSERT_EQ(0, vec.suffixSize());

  vec.resizeSuffix(TEST_FILE_SUFFIX.size());
  std::copy(TEST_FILE_SUFFIX.begin(), TEST_FILE_SUFFIX.end(), vec.suffix());

  ASSERT_EQ(TEST_FILE_SUFFIX.size(), vec.suffixSize());
  ASSERT_EQ(TEST_FILE_SUFFIX, std::string(vec.suffix(), vec.suffix() + vec.suffixSize()));

  ASSERT_EQ(0, vec.size());
  ASSERT_LE(0, vec.capacity());
}

TEST_F(FileMappedVectorTest, atomicUpdateThrowsExceptionIfFailedToRemoveExistentBakFile) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');

  boost::filesystem::create_directory(TEST_FILE_NAME_BAK);
  boost::filesystem::create_directory(boost::filesystem::path(TEST_FILE_NAME_BAK) / TEST_FILE_NAME);
  ASSERT_ANY_THROW(vec.insert(vec.begin(), 'b'));
}

TEST_F(FileMappedVectorTest, atomicUpdateFailureDoesNotBrokeVector) {
  FileMappedVector<char> vec(TEST_FILE_NAME);
  vec.push_back('a');
  vec.push_back('b');
  vec.push_back('c');

  try {
    boost::filesystem::create_directory(TEST_FILE_NAME_BAK);
    boost::filesystem::create_directory(boost::filesystem::path(TEST_FILE_NAME_BAK) / TEST_FILE_NAME);
    vec.insert(vec.begin(), 'w');
    ASSERT_FALSE(true);
  } catch (...) {
  }

  ASSERT_EQ(3, vec.size());
  ASSERT_EQ('a', vec[0]);
  ASSERT_EQ('b', vec[1]);
  ASSERT_EQ('c', vec[2]);
}

}
