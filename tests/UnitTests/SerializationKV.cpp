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

#include "gtest/gtest.h"

#include <boost/lexical_cast.hpp>

#include "Serialization/KVBinaryInputStreamSerializer.h"
#include "Serialization/KVBinaryOutputStreamSerializer.h"
#include "Serialization/SerializationOverloads.h"
#include "Serialization/SerializationTools.h"

#include <array>

using namespace CryptoNote;

namespace CryptoNote {

struct TestElement {
  std::string name;
  uint32_t nonce;
  std::array<uint8_t, 16> blob;
  std::vector<uint32_t> u32array;

  bool operator == (const TestElement& other) const {
    return 
      name == other.name && 
      nonce == other.nonce &&
      blob == other.blob && 
      u32array == other.u32array;
  }

  void serialize(ISerializer& s) {
    s(name, "name");
    s(nonce, "nonce");
    s.binary(blob.data(), blob.size(), "blob");
    serializeAsBinary(u32array, "u32array", s);
  }
};

struct TestStruct {
  uint8_t u8;
  uint32_t u32;
  uint64_t u64;
  std::vector<TestElement> vec1;
  std::vector<TestElement> vec2;
  std::vector<std::vector<TestElement>> vecOfVec;
  TestElement root;

  bool operator == (const TestStruct& other) const {
    return
      root == other.root &&
      u8 == other.u8 &&
      u32 == other.u32 &&
      u64 == other.u64 &&
      vec1 == other.vec1 &&
      vec2 == other.vec2 && 
      vecOfVec == other.vecOfVec;
  }

  void serialize(ISerializer& s) {
    s(root, "root");
    s(vec1, "vec1");
    s(vec2, "vec2");
    s(vecOfVec, "vecOfVec");
    s(u8, "u8");
    s(u32, "u32");
    s(u64, "u64");
  }

};

}


#include <chrono>

typedef std::chrono::high_resolution_clock hclock;

class HiResTimer {
public:
  HiResTimer() : 
    start(hclock::now()) {}

  std::chrono::duration<double> duration() {
    return hclock::now() - start;
  }

private:
  hclock::time_point start;
};

TEST(KVSerialize, Simple) {
  TestElement testData1, testData2;

  testData1.name = "hello";
  testData1.nonce = 12345;
  testData1.u32array.resize(128);

  testData2.name = "bye";
  testData2.nonce = 54321;

  std::string buf = CryptoNote::storeToBinaryKeyValue(testData1);
  ASSERT_TRUE(CryptoNote::loadFromBinaryKeyValue(testData2, buf));
  EXPECT_EQ(testData1, testData2);
}

TEST(KVSerialize, BigCollection) {
  TestStruct ts1;

  ts1.u8 = 100;
  ts1.u32 = 0xff0000;
  ts1.u64 = 1ULL << 60;
  ts1.root.name = "hello";

  TestElement sample;
  sample.nonce = 101;
  ts1.vec1.resize(0x10000 >> 2, sample);

  TestStruct ts2;

  std::string buf = CryptoNote::storeToBinaryKeyValue(ts1);
  ASSERT_TRUE(CryptoNote::loadFromBinaryKeyValue(ts2, buf));
  EXPECT_EQ(ts1, ts2);
}

TEST(KVSerialize, DISABLED_CollectionOfCollections) {
  TestStruct ts1;

  TestElement sample;
  sample.nonce = 101;
  ts1.vec1.resize(0x10000 >> 10, sample);
  ts1.vecOfVec.resize(0x10000 >> 14, ts1.vec1);

  TestStruct ts2;

  std::string buf = CryptoNote::storeToBinaryKeyValue(ts1);
  ASSERT_TRUE(CryptoNote::loadFromBinaryKeyValue(ts2, buf));
  EXPECT_EQ(ts1, ts2);
}
