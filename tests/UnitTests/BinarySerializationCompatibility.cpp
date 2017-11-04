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

//#include "gtest/gtest.h"
//
//#include <sstream>
//#include <limits>
//#include <boost/variant/get.hpp>
//
//#include "Serialization/BinaryOutputStreamSerializer.h"
//#include "Serialization/BinaryInputStreamSerializer.h"
//
//#include "Serialization/serialization.h"
//#include "Serialization/binary_utils.h"
//
//#include "CryptoNoteCore/CryptoNoteBasic.h"
//#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
//#include "CryptoNoteCore/CryptoNoteSerialization.h"
//#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
//#include "serialization_structs_comparators.h"
//
//#include <iostream>
//
//#include <boost/functional/hash.hpp>
//
//template <typename Struct>
//void checkEqualBinary(Struct& original) {
//  std::stringstream newStream;
//  std::stringstream oldStream;
//
//  CryptoNote::BinaryOutputStreamSerializer binarySerializer(newStream);
//  // binarySerializer(original, "");
//  CryptoNote::serialize(original, binarySerializer);
//
//  binary_archive<true> ba(oldStream);
//  bool r = ::serialization::serialize(ba, original);
//  ASSERT_TRUE(r);
//
//  ASSERT_EQ(oldStream.str(), newStream.str());
//}
//
//template <typename Struct>
//void checkEnumeratorToLegacy(Struct& original) {
//  std::stringstream archive;
//
//  CryptoNote::BinaryOutputStreamSerializer binarySerializer(archive);
//  CryptoNote::serialize(original, binarySerializer);
//
//  Struct restored;
//  binary_archive<false> ba(archive);
//  bool r = ::serialization::serialize(ba, restored);
//  ASSERT_TRUE(r);
//
//  ASSERT_EQ(original, restored);
//}
//
//template <typename Struct>
//void checkLegacyToEnumerator(Struct& original) {
//  std::stringstream archive;
//
//  binary_archive<true> ba(archive);
//  bool r = ::serialization::serialize(ba, original);
//  ASSERT_TRUE(r);
//
//  Struct restored;
//
//  CryptoNote::BinaryInputStreamSerializer binarySerializer(archive);
//  binarySerializer(restored, "");
//
//  ASSERT_EQ(original, restored);
//}
//
//template <typename Struct>
//void checkEnumeratorToEnumerator(Struct& original) {
//  std::stringstream archive;
//
//  CryptoNote::BinaryOutputStreamSerializer output(archive);
//  output(original, "");
//
//  Struct restored;
//  CryptoNote::BinaryInputStreamSerializer input(archive);
//  input(restored, "");
//
//  ASSERT_EQ(original, restored);
//}
//
//template <typename Struct>
//void checkCompatibility(Struct& original) {
//  checkEqualBinary(original);
//  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToEnumerator(original));
//  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToLegacy(original));
//  ASSERT_NO_FATAL_FAILURE(checkLegacyToEnumerator(original));
//}
//
//void fillData(char* data, size_t size, char startByte) {
//  for (size_t i = 0; i < size; ++i) {
//    data[i] = startByte++;
//  }
//}
//
//void fillPublicKey(Crypto::PublicKey& key, char startByte = 120) {
//  fillData(reinterpret_cast<char *>(&key), sizeof(Crypto::PublicKey), startByte);
//}
//
//void fillHash(Crypto::Hash& hash, char startByte = 120) {
//  fillData(reinterpret_cast<char *>(&hash), sizeof(Crypto::Hash), startByte);
//}
//
//void fillKeyImage(Crypto::KeyImage& image, char startByte = 120) {
//  fillData(reinterpret_cast<char *>(&image), sizeof(Crypto::KeyImage), startByte);
//}
//
//void fillSignature(Crypto::Signature& sig, char startByte = 120) {
//  fillData(reinterpret_cast<char *>(&sig), sizeof(Crypto::Signature), startByte);
//}
//
//void fillTransactionOutputMultisignature(CryptoNote::TransactionOutputMultisignature& s) {
//  Crypto::PublicKey key;
//  fillPublicKey(key, 0);
//  s.keys.push_back(key);
//
//  char start = 120;
//
//  fillPublicKey(key, start++);
//  s.keys.push_back(key);
//
//  fillPublicKey(key, start++);
//  s.keys.push_back(key);
//
//  fillPublicKey(key, start++);
//  s.keys.push_back(key);
//
//  fillPublicKey(key, start++);
//  s.keys.push_back(key);
//
//  fillPublicKey(key, start++);
//  s.keys.push_back(key);
//
//  s.requiredSignatures = 12;
//}
//
//void fillTransaction(CryptoNote::Transaction& tx) {
//  tx.version = 1;
//  tx.unlockTime = 0x7f1234560089ABCD;
//
//  CryptoNote::TransactionInputGenerate gen;
//  gen.height = 0xABCDEF12;
//  tx.vin.push_back(gen);
//
//  CryptoNote::TransactionInputToKey key;
//  key.amount = 500123;
//  key.keyOffsets = {12,3323,0x7f0000000000, std::numeric_limits<uint64_t>::max(), 0};
//  fillKeyImage(key.keyImage);
//  tx.vin.push_back(key);
//
//  CryptoNote::TransactionInputMultisignature multisig;
//  multisig.amount = 490000000;
//  multisig.outputIndex = 424242;
//  multisig.signatures = 4;
//  tx.vin.push_back(multisig);
//
//  CryptoNote::TransactionOutput txOutput;
//  txOutput.amount = 0xfff000ffff778822;
//  CryptoNote::TransactionOutputToKey out;
//  fillPublicKey(out.key);
//  txOutput.target = out;
//  tx.vout.push_back(txOutput);
//
//  tx.extra = {1,2,3,127,0,128,255};
//
//  tx.signatures.resize(3);
//
//  for (size_t i = 0; i < boost::get<CryptoNote::TransactionInputToKey>(tx.vin[1]).keyOffsets.size(); ++i) {
//    Crypto::Signature sig;
//    fillSignature(sig, static_cast<char>(i));
//    tx.signatures[1].push_back(sig);
//  }
//
//  for (size_t i = 0; i < boost::get<CryptoNote::TransactionInputMultisignature>(tx.vin[2]).signatures; ++i) {
//    Crypto::Signature sig;
//    fillSignature(sig, static_cast<char>(i + 120));
//    tx.signatures[2].push_back(sig);
//  }
//}
//
//void fillParentBlock(CryptoNote::ParentBlock& pb) {
//  pb.majorVersion = 1;
//  pb.minorVersion = 1;
//
//  fillHash(pb.prevId, 120);
//
//  pb.numberOfTransactions = 3;
//  size_t branchSize = Crypto::tree_depth(pb.numberOfTransactions);
//  for (size_t i = 0; i < branchSize; ++i) {
//    Crypto::Hash hash;
//    fillHash(hash, static_cast<char>(i));
//    pb.minerTxBranch.push_back(hash);
//  }
//
//  fillTransaction(pb.minerTx);
//
//  CryptoNote::tx_extra_merge_mining_tag mmTag;
//  mmTag.depth = 10;
//  fillHash(mmTag.merkle_root);
//  pb.minerTx.extra.clear();
//  CryptoNote::append_mm_tag_to_extra(pb.minerTx.extra, mmTag);
//
//  std::string my;
//  std::copy(pb.minerTx.extra.begin(), pb.minerTx.extra.end(), std::back_inserter(my));
//
//  for (size_t i = 0; i < mmTag.depth; ++i) {
//    Crypto::Hash hash;
//    fillHash(hash, static_cast<char>(i));
//    pb.blockchainBranch.push_back(hash);
//  }
//}
//
//void fillBlockHeaderVersion1(CryptoNote::BlockHeader& header) {
//  header.majorVersion = 1;
//  header.minorVersion = 1;
//  header.nonce = 0x807F00AB;
//  header.timestamp = 1408106672;
//  fillHash(header.prevId);
//}
//
//void fillBlockHeaderVersion2(CryptoNote::BlockHeader& header) {
//  fillBlockHeaderVersion1(header);
//  header.majorVersion = 2;
//}
//
//TEST(BinarySerializationCompatibility, TransactionOutputMultisignature) {
//  CryptoNote::TransactionOutputMultisignature s;
//
//  fillTransactionOutputMultisignature(s);
//
//  checkCompatibility(s);
//}
//
//TEST(BinarySerializationCompatibility, TransactionInputGenerate) {
//  CryptoNote::TransactionInputGenerate s;
//  s.height = 0x80000001;
//  checkCompatibility(s);
//
//  s.height = 0x7FFFFFFF;
//  checkCompatibility(s);
//
//  s.height = 0;
//  checkCompatibility(s);
//};
//
//TEST(BinarySerializationCompatibility, TransactionInputToKey) {
//  CryptoNote::TransactionInputToKey s;
//
//  s.amount = 123456987032;
//  s.keyOffsets = {12,3323,0x7f00000000000000, std::numeric_limits<uint64_t>::max(), 0};
//  fillKeyImage(s.keyImage);
//
//  checkCompatibility(s);
//}
//
//TEST(BinarySerializationCompatibility, TransactionInputMultisignature) {
//  CryptoNote::TransactionInputMultisignature s;
//  s.amount = 0xfff000ffff778822;
//  s.signatures = 0x7f259200;
//  s.outputIndex = 0;
//
//  checkCompatibility(s);
//}
//
//TEST(BinarySerializationCompatibility, TransactionOutput_TransactionOutputToKey) {
//  CryptoNote::TransactionOutput s;
//  s.amount = 0xfff000ffff778822;
//
//  CryptoNote::TransactionOutputToKey out;
//  fillPublicKey(out.key);
//  s.target = out;
//
//  checkCompatibility(s);
//}
//
//TEST(BinarySerializationCompatibility, TransactionOutput_TransactionOutputMultisignature) {
//  CryptoNote::TransactionOutput s;
//  s.amount = 0xfff000ffff778822;
//
//  CryptoNote::TransactionOutputMultisignature out;
//  fillTransactionOutputMultisignature(out);
//  s.target = out;
//
//  checkCompatibility(s);
//}
//
//TEST(BinarySerializationCompatibility, Transaction) {
//  CryptoNote::Transaction tx;
//
//  fillTransaction(tx);
//
//  checkCompatibility(tx);
//}
//
//TEST(BinarySerializationCompatibility, TransactionHash) {
//  CryptoNote::Transaction tx;
//  fillTransaction(tx);
//
//  auto h1 = CryptoNote::getObjectHash(tx);
//
//  std::string blob;
//  serialization::dump_binary(tx, blob);
//  Crypto::Hash h2;
//  CryptoNote::get_blob_hash(blob, h2);
//
//
//  auto s1 = std::hash<Crypto::Hash>()(h1);
//  auto s2 = boost::hash<Crypto::Hash>()(h1);
//
//  ASSERT_EQ(h1, h2);
//}
//
//
//void compareParentBlocks(CryptoNote::ParentBlock& pb, CryptoNote::ParentBlock& restoredPb, bool headerOnly) {
//  EXPECT_EQ(pb.majorVersion, restoredPb.majorVersion);
//  EXPECT_EQ(pb.minorVersion, restoredPb.minorVersion);
//  EXPECT_EQ(pb.prevId, restoredPb.prevId);
//
//  if (headerOnly) {
//    return;
//  }
//
//  EXPECT_EQ(pb.numberOfTransactions, restoredPb.numberOfTransactions);
//  EXPECT_EQ(pb.minerTxBranch, restoredPb.minerTxBranch);
//  EXPECT_EQ(pb.minerTx, restoredPb.minerTx);
//  EXPECT_EQ(pb.blockchainBranch, restoredPb.blockchainBranch);
//}
//
//void checkEnumeratorToLegacy(CryptoNote::ParentBlock& pb, uint64_t ts, uint32_t nonce, bool hashingSerialization, bool headerOnly) {
//  std::stringstream archive;
//
//  CryptoNote::ParentBlockSerializer original(pb, ts, nonce, hashingSerialization, headerOnly);
//  CryptoNote::BinaryOutputStreamSerializer output(archive);
//  output(original, "");
//
//  CryptoNote::ParentBlock restoredPb;
//  uint64_t restoredTs;
//  uint32_t restoredNonce;
//
//  CryptoNote::ParentBlockSerializer restored(restoredPb, restoredTs, restoredNonce, hashingSerialization, headerOnly);
//  binary_archive<false> ba(archive);
//  bool r = ::serialization::serialize(ba, restored);
//  ASSERT_TRUE(r);
//
//  EXPECT_EQ(nonce, restoredNonce);
//  EXPECT_EQ(ts, restoredTs);
//
//  ASSERT_NO_FATAL_FAILURE(compareParentBlocks(pb, restoredPb, headerOnly));
//}
//
//void checkLegacyToEnumerator(CryptoNote::ParentBlock& pb, uint64_t ts, uint32_t nonce, bool hashingSerialization, bool headerOnly) {
//  std::stringstream archive;
//
//  CryptoNote::ParentBlockSerializer original(pb, ts, nonce, hashingSerialization, headerOnly);
//  binary_archive<true> ba(archive);
//  bool r = ::serialization::serialize(ba, original);
//  ASSERT_TRUE(r);
//
//  CryptoNote::ParentBlock restoredPb;
//  uint64_t restoredTs;
//  uint32_t restoredNonce;
//
//  CryptoNote::ParentBlockSerializer restored(restoredPb, restoredTs, restoredNonce, hashingSerialization, headerOnly);
//
//  CryptoNote::BinaryInputStreamSerializer input(archive);
//  input(restored, "");
//
//  EXPECT_EQ(nonce, restoredNonce);
//  EXPECT_EQ(ts, restoredTs);
//
//  ASSERT_NO_FATAL_FAILURE(compareParentBlocks(pb, restoredPb, headerOnly));
//}
//
//void checkEnumeratorToEnumerator(CryptoNote::ParentBlock& pb, uint64_t ts, uint32_t nonce, bool hashingSerialization, bool headerOnly) {
//  std::stringstream archive;
//
//  CryptoNote::ParentBlockSerializer original(pb, ts, nonce, hashingSerialization, headerOnly);
//  CryptoNote::BinaryOutputStreamSerializer output(archive);
//  output(original, "");
//
//  CryptoNote::ParentBlock restoredPb;
//  uint64_t restoredTs;
//  uint32_t restoredNonce;
//
//  CryptoNote::ParentBlockSerializer restored(restoredPb, restoredTs, restoredNonce, hashingSerialization, headerOnly);
//
//  CryptoNote::BinaryInputStreamSerializer input(archive);
//  input(restored, "");
//
//  EXPECT_EQ(nonce, restoredNonce);
//  EXPECT_EQ(ts, restoredTs);
//
//  ASSERT_NO_FATAL_FAILURE(compareParentBlocks(pb, restoredPb, headerOnly));
//}
//
//void checkCompatibility(CryptoNote::ParentBlock& pb, uint64_t ts, uint32_t nonce, bool hashingSerialization, bool headerOnly) {
//  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToEnumerator(pb, ts, nonce, hashingSerialization, headerOnly));
//  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToLegacy(pb, ts, nonce, hashingSerialization, headerOnly));
//  ASSERT_NO_FATAL_FAILURE(checkLegacyToEnumerator(pb, ts, nonce, hashingSerialization, headerOnly));
//}
//
//TEST(BinarySerializationCompatibility, ParentBlockSerializer) {
//  CryptoNote::ParentBlock pb;
//  fillParentBlock(pb);
//  uint64_t timestamp = 1408106672;
//  uint32_t nonce = 1234567;
//
//  checkCompatibility(pb, timestamp, nonce, false, false);
//  checkCompatibility(pb, timestamp, nonce, true, false);
//  checkCompatibility(pb, timestamp, nonce, false, true);
//}
//
//void compareBlocks(CryptoNote::Block& block, CryptoNote::Block& restoredBlock) {
//  ASSERT_EQ(block.majorVersion, restoredBlock.majorVersion);
//  ASSERT_EQ(block.minorVersion, restoredBlock.minorVersion);
//  if (block.majorVersion == CryptoNote::BLOCK_MAJOR_VERSION_1) {
//    ASSERT_EQ(block.timestamp, restoredBlock.timestamp);
//    ASSERT_EQ(block.prevId, restoredBlock.prevId);
//    ASSERT_EQ(block.nonce, restoredBlock.nonce);
//  } else if (block.majorVersion == CryptoNote::BLOCK_MAJOR_VERSION_2) {
//    ASSERT_EQ(block.prevId, restoredBlock.prevId);
//    ASSERT_NO_FATAL_FAILURE(compareParentBlocks(block.parentBlock, restoredBlock.parentBlock, false));
//  } else {
//    throw std::runtime_error("Unknown major block version. Check your test");
//  }
//  ASSERT_EQ(block.minerTx, restoredBlock.minerTx);
//  ASSERT_EQ(block.txHashes, restoredBlock.txHashes);
//}
//
//void checkEnumeratorToLegacy(CryptoNote::Block& block) {
//  std::stringstream archive;
//
//  CryptoNote::BinaryOutputStreamSerializer output(archive);
//  output(block, "");
//
//  CryptoNote::Block restoredBlock;
//
//  binary_archive<false> ba(archive);
//  bool r = ::serialization::serialize(ba, restoredBlock);
//  ASSERT_TRUE(r);
//
//  ASSERT_NO_FATAL_FAILURE(compareBlocks(block, restoredBlock));
//}
//
//void checkLegacyToEnumerator(CryptoNote::Block& block) {
//  std::stringstream archive;
//
//  binary_archive<true> ba(archive);
//  bool r = ::serialization::serialize(ba, block);
//  ASSERT_TRUE(r);
//
//  CryptoNote::Block restoredBlock;
//
//  CryptoNote::BinaryInputStreamSerializer output(archive);
//  output(restoredBlock, "");
//
//  ASSERT_NO_FATAL_FAILURE(compareBlocks(block, restoredBlock));
//}
//
//void checkEnumeratorToEnumerator(CryptoNote::Block& block) {
//  std::stringstream archive;
//
//  CryptoNote::BinaryOutputStreamSerializer output(archive);
//  output(block, "");
//
//  CryptoNote::Block restoredBlock;
//
//  CryptoNote::BinaryInputStreamSerializer input(archive);
//  input(restoredBlock, "");
//
//  ASSERT_NO_FATAL_FAILURE(compareBlocks(block, restoredBlock));
//}
//
//void checkCompatibility(CryptoNote::Block& block) {
//  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToEnumerator(block));
//  ASSERT_NO_FATAL_FAILURE(checkEnumeratorToLegacy(block));
//  ASSERT_NO_FATAL_FAILURE(checkLegacyToEnumerator(block));
//}
//
//TEST(BinarySerializationCompatibility, BlockVersion1) {
//  CryptoNote::Block block;
//  fillBlockHeaderVersion1(block);
//  fillParentBlock(block.parentBlock);
//  fillTransaction(block.minerTx);
//
//  for (size_t i = 0; i < 7; ++i) {
//    Crypto::Hash hash;
//    fillHash(hash, static_cast<char>(0x7F + i));
//    block.txHashes.push_back(hash);
//  }
//
//  checkCompatibility(block);
//}
//
//TEST(BinarySerializationCompatibility, BlockVersion2) {
//  CryptoNote::Block block;
//  fillBlockHeaderVersion2(block);
//  fillParentBlock(block.parentBlock);
//  fillTransaction(block.minerTx);
//
//  for (size_t i = 0; i < 7; ++i) {
//    Crypto::Hash hash;
//    fillHash(hash, static_cast<char>(0x7F + i));
//    block.txHashes.push_back(hash);
//  }
//
//  checkCompatibility(block);
//}
//
//TEST(BinarySerializationCompatibility, account_public_address) {
//  CryptoNote::AccountPublicAddress addr;
//
//  fillPublicKey(addr.m_spendPublicKey, '\x50');
//  fillPublicKey(addr.m_viewPublicKey, '\xAA');
//
//  checkCompatibility(addr);
//}
//
////TEST(BinarySerializationCompatibility, tx_extra_merge_mining_tag) {
////  CryptoNote::tx_extra_merge_mining_tag tag;
////  tag.depth = 0xdeadbeef;
////  fillHash(tag.merkle_root);
////
////  checkCompatibility(tag);
////}
//
//TEST(BinarySerializationCompatibility, readFromEmptyStream) {
//  CryptoNote::TransactionOutput t;
//  std::stringstream emptyStream;
//  CryptoNote::BinaryInputStreamSerializer s(emptyStream);
//
//  ASSERT_ANY_THROW(s(t, ""));
//}
//
//TEST(BinarySerializationCompatibility, writeToBadStream) {
//  CryptoNote::TransactionOutput t;
//  std::stringstream badStream;
//  CryptoNote::BinaryOutputStreamSerializer s(badStream);
//
//  badStream.setstate(std::ios::badbit);
//  ASSERT_ANY_THROW(s(t, ""));
//}
