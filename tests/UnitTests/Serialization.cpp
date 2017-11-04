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

#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/BinarySerializationTools.h"

using namespace Common;
using namespace CryptoNote;

TEST(BinarySerializer, uint16) {

  std::stringstream ss;
  
  uint16_t u16 = 0xfeff;
  uint32_t u32 = 0x3fddfd48;

  {
    StdOutputStream os(ss);
    BinaryOutputStreamSerializer s(os);
    s(u32, "u32");
    s(u16, "u16");
  }

  {
    StdInputStream is(ss);
    BinaryInputStreamSerializer s(is);
    
    uint32_t t32 = 0;
    uint16_t t16 = 0;
    
    s(t32, "u32");
    s(t16, "u16");

    ASSERT_EQ(u32, t32);
    ASSERT_EQ(u16, t16);
  }
}


//#include <cstring>
//#include <cstdint>
//#include <cstdio>
//#include <iostream>
//#include <vector>
//#include <boost/foreach.hpp>
//#include "CryptoNoteCore/CryptoNoteBasic.h"
//#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
//#include "Serialization/serialization.h"
//#include "Serialization/binary_archive.h"
//#include "Serialization/json_archive.h"
//#include "Serialization/variant.h"
//#include "Serialization/vector.h"
//#include "Serialization/binary_utils.h"
//#include "gtest/gtest.h"
//using namespace std;
//
//struct Struct
//{
//  int32_t a;
//  int32_t b;
//  char blob[8];
//};
//
//template <class Archive>
//struct serializer<Archive, Struct>
//{
//  static bool serialize(Archive &ar, Struct &s) {
//    ar.begin_object();
//    ar.tag("a");
//    ar.serialize_int(s.a);
//    ar.tag("b");
//    ar.serialize_int(s.b);
//    ar.tag("blob");
//    ar.serialize_blob(s.blob, sizeof(s.blob));
//    ar.end_object();
//    return true;
//  }
//};
//
//struct Struct1
//{
//  vector<boost::variant<Struct, int32_t>> si;
//  vector<int16_t> vi;
//
//  BEGIN_SERIALIZE_OBJECT()
//    FIELD(si)
//    FIELD(vi)
//  END_SERIALIZE()
//  /*template <bool W, template <bool> class Archive>
//  bool do_serialize(Archive<W> &ar)
//  {
//    ar.begin_object();
//    ar.tag("si");
//    ::do_serialize(ar, si);
//    ar.tag("vi");
//    ::do_serialize(ar, vi);
//    ar.end_object();
//  }*/
//};
//
//struct Blob
//{
//  uint64_t a;
//  uint32_t b;
//
//  bool operator==(const Blob& rhs) const
//  {
//    return a == rhs.a;
//  }
//};
//
//VARIANT_TAG(binary_archive, Struct, 0xe0);
//VARIANT_TAG(binary_archive, int, 0xe1);
//VARIANT_TAG(json_archive, Struct, "struct");
//VARIANT_TAG(json_archive, int, "int");
//
//BLOB_SERIALIZER(Blob);
//
//bool try_parse(const string &blob)
//{
//  Struct1 s1;
//  return serialization::parse_binary(blob, s1);
//}
//
//TEST(Serialization, BinaryArchiveInts) {
//  uint64_t x = 0xff00000000, x1;
//
//  ostringstream oss;
//  binary_archive<true> oar(oss);
//  oar.serialize_int(x);
//  ASSERT_TRUE(oss.good());
//  ASSERT_EQ(8, oss.str().size());
//  ASSERT_EQ(string("\0\0\0\0\xff\0\0\0", 8), oss.str());
//
//  istringstream iss(oss.str());
//  binary_archive<false> iar(iss);
//  iar.serialize_int(x1);
//  ASSERT_EQ(8, iss.tellg());
//  ASSERT_TRUE(iss.good());
//
//  ASSERT_EQ(x, x1);
//}
//
//TEST(Serialization, BinaryArchiveVarInts) {
//  uint64_t x = 0xff00000000, x1;
//
//  ostringstream oss;
//  binary_archive<true> oar(oss);
//  oar.serialize_varint(x);
//  ASSERT_TRUE(oss.good());
//  ASSERT_EQ(6, oss.str().size());
//  ASSERT_EQ(string("\x80\x80\x80\x80\xF0\x1F", 6), oss.str());
//
//  istringstream iss(oss.str());
//  binary_archive<false> iar(iss);
//  iar.serialize_varint(x1);
//  ASSERT_TRUE(iss.good());
//  ASSERT_EQ(x, x1);
//}
//
//TEST(Serialization, Test1) {
//  ostringstream str;
//  binary_archive<true> ar(str);
//
//  Struct1 s1;
//  s1.si.push_back(0);
//  {
//    Struct s;
//    s.a = 5;
//    s.b = 65539;
//    std::memcpy(s.blob, "12345678", 8);
//    s1.si.push_back(s);
//  }
//  s1.si.push_back(1);
//  s1.vi.push_back(10);
//  s1.vi.push_back(22);
//
//  string blob;
//  ASSERT_TRUE(serialization::dump_binary(s1, blob));
//  ASSERT_TRUE(try_parse(blob));
//
//  ASSERT_EQ('\xE0', blob[6]);
//  blob[6] = '\xE1';
//  ASSERT_FALSE(try_parse(blob));
//  blob[6] = '\xE2';
//  ASSERT_FALSE(try_parse(blob));
//}
//
//TEST(Serialization, Overflow) {
//  Blob x = { 0xff00000000 };
//  Blob x1;
//
//  string blob;
//  ASSERT_TRUE(serialization::dump_binary(x, blob));
//  ASSERT_EQ(sizeof(Blob), blob.size());
//
//  ASSERT_TRUE(serialization::parse_binary(blob, x1));
//  ASSERT_EQ(x, x1);
//
//  vector<Blob> bigvector;
//  ASSERT_FALSE(serialization::parse_binary(blob, bigvector));
//  ASSERT_EQ(0, bigvector.size());
//}
//
//TEST(Serialization, serializes_vector_uint64_as_varint)
//{
//  std::vector<uint64_t> v;
//  string blob;
//
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(1, blob.size());
//
//  // +1 byte
//  v.push_back(0);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(2, blob.size());
//
//  // +1 byte
//  v.push_back(1);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(3, blob.size());
//
//  // +2 bytes
//  v.push_back(0x80);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(5, blob.size());
//
//  // +2 bytes
//  v.push_back(0xFF);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(7, blob.size());
//
//  // +2 bytes
//  v.push_back(0x3FFF);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(9, blob.size());
//
//  // +3 bytes
//  v.push_back(0x40FF);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(12, blob.size());
//
//  // +10 bytes
//  v.push_back(0xFFFFFFFFFFFFFFFF);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(22, blob.size());
//}
//
//TEST(Serialization, serializes_vector_int64_as_fixed_int)
//{
//  std::vector<int64_t> v;
//  string blob;
//
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(1, blob.size());
//
//  // +8 bytes
//  v.push_back(0);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(9, blob.size());
//
//  // +8 bytes
//  v.push_back(1);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(17, blob.size());
//
//  // +8 bytes
//  v.push_back(0x80);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(25, blob.size());
//
//  // +8 bytes
//  v.push_back(0xFF);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(33, blob.size());
//
//  // +8 bytes
//  v.push_back(0x3FFF);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(41, blob.size());
//
//  // +8 bytes
//  v.push_back(0x40FF);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(49, blob.size());
//
//  // +8 bytes
//  v.push_back(0xFFFFFFFFFFFFFFFF);
//  ASSERT_TRUE(serialization::dump_binary(v, blob));
//  ASSERT_EQ(57, blob.size());
//}
//
//namespace
//{
//  template<typename T>
//  std::vector<T> linearize_vector2(const std::vector< std::vector<T> >& vec_vec)
//  {
//    std::vector<T> res;
//    BOOST_FOREACH(const auto& vec, vec_vec)
//    {
//      res.insert(res.end(), vec.begin(), vec.end());
//    }
//    return res;
//  }
//}
//
//TEST(Serialization, serializes_transacion_signatures_correctly)
//{
//  using namespace CryptoNote;
//
//  Transaction tx;
//  Transaction tx1;
//  string blob;
//
//  // Empty tx
//  tx.clear();
//  ASSERT_TRUE(serialization::dump_binary(tx, blob));
//  ASSERT_EQ(5, blob.size()); // 5 bytes + 0 bytes extra + 0 bytes signatures
//  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
//  ASSERT_EQ(tx, tx1);
//  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));
//
//  // Miner tx without signatures
//  TransactionInputGenerate txin_gen1;
//  txin_gen1.height = 0;
//  tx.clear();
//  tx.vin.push_back(txin_gen1);
//  ASSERT_TRUE(serialization::dump_binary(tx, blob));
//  ASSERT_EQ(7, blob.size()); // 5 bytes + 2 bytes vin[0] + 0 bytes extra + 0 bytes signatures
//  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
//  ASSERT_EQ(tx, tx1);
//  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));
//
//  // Miner tx with empty signatures 2nd vector
//  tx.signatures.resize(1);
//  ASSERT_TRUE(serialization::dump_binary(tx, blob));
//  ASSERT_EQ(7, blob.size()); // 5 bytes + 2 bytes vin[0] + 0 bytes extra + 0 bytes signatures
//  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
//  ASSERT_EQ(tx, tx1);
//  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));
//
//  // Miner tx with one signature
//  tx.signatures[0].resize(1);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // Miner tx with 2 empty vectors
//  tx.signatures.resize(2);
//  tx.signatures[0].resize(0);
//  tx.signatures[1].resize(0);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // Miner tx with 2 signatures
//  tx.signatures[0].resize(1);
//  tx.signatures[1].resize(1);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // Two TransactionInputGenerate, no signatures
//  tx.vin.push_back(txin_gen1);
//  tx.signatures.resize(0);
//  ASSERT_TRUE(serialization::dump_binary(tx, blob));
//  ASSERT_EQ(9, blob.size()); // 5 bytes + 2 * 2 bytes vins + 0 bytes extra + 0 bytes signatures
//  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
//  ASSERT_EQ(tx, tx1);
//  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));
//
//  // Two TransactionInputGenerate, signatures vector contains only one empty element
//  tx.signatures.resize(1);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // Two TransactionInputGenerate, signatures vector contains two empty elements
//  tx.signatures.resize(2);
//  ASSERT_TRUE(serialization::dump_binary(tx, blob));
//  ASSERT_EQ(9, blob.size()); // 5 bytes + 2 * 2 bytes vins + 0 bytes extra + 0 bytes signatures
//  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
//  ASSERT_EQ(tx, tx1);
//  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));
//
//  // Two TransactionInputGenerate, signatures vector contains three empty elements
//  tx.signatures.resize(3);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // Two TransactionInputGenerate, signatures vector contains two non empty elements
//  tx.signatures.resize(2);
//  tx.signatures[0].resize(1);
//  tx.signatures[1].resize(1);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // A few bytes instead of signature
//  tx.vin.clear();
//  tx.vin.push_back(txin_gen1);
//  tx.signatures.clear();
//  ASSERT_TRUE(serialization::dump_binary(tx, blob));
//  blob.append(std::string(sizeof(Crypto::Signature) / 2, 'x'));
//  ASSERT_FALSE(serialization::parse_binary(blob, tx1));
//
//  // blob contains one signature
//  blob.append(std::string(sizeof(Crypto::Signature) / 2, 'y'));
//  ASSERT_FALSE(serialization::parse_binary(blob, tx1));
//
//  // Not enough signature vectors for all inputs
//  TransactionInputToKey txin_to_key1;
//  txin_to_key1.keyOffsets.resize(2);
//  tx.vin.clear();
//  tx.vin.push_back(txin_to_key1);
//  tx.vin.push_back(txin_to_key1);
//  tx.signatures.resize(1);
//  tx.signatures[0].resize(2);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // Too much signatures for two inputs
//  tx.signatures.resize(3);
//  tx.signatures[0].resize(2);
//  tx.signatures[1].resize(2);
//  tx.signatures[2].resize(2);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // First signatures vector contains too little elements
//  tx.signatures.resize(2);
//  tx.signatures[0].resize(1);
//  tx.signatures[1].resize(2);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // First signatures vector contains too much elements
//  tx.signatures.resize(2);
//  tx.signatures[0].resize(3);
//  tx.signatures[1].resize(2);
//  ASSERT_FALSE(serialization::dump_binary(tx, blob));
//
//  // There are signatures for each input
//  tx.signatures.resize(2);
//  tx.signatures[0].resize(2);
//  tx.signatures[1].resize(2);
//  ASSERT_TRUE(serialization::dump_binary(tx, blob));
//  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
//  ASSERT_EQ(tx, tx1);
//  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));
//
//  // Blob doesn't contain enough data
//  blob.resize(blob.size() - sizeof(Crypto::Signature) / 2);
//  ASSERT_FALSE(serialization::parse_binary(blob, tx1));
//
//  // Blob contains too much data
//  blob.resize(blob.size() + sizeof(Crypto::Signature));
//  ASSERT_FALSE(serialization::parse_binary(blob, tx1));
//
//  // Blob contains one excess signature
//  blob.resize(blob.size() + sizeof(Crypto::Signature) / 2);
//  ASSERT_FALSE(serialization::parse_binary(blob, tx1));
//}
