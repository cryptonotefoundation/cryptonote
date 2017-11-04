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

#include <Common/ArrayRef.h>
#include <gtest/gtest.h>

using namespace Common;

TEST(ArrayRefTests, representations) {
  ASSERT_NE(nullptr, ArrayRef<>::EMPTY.getData());
  ASSERT_EQ(0, ArrayRef<>::EMPTY.getSize());
  ASSERT_EQ(nullptr, ArrayRef<>::NIL.getData());
  ASSERT_EQ(0, ArrayRef<>::NIL.getSize());
}

TEST(ArrayRefTests, directConstructor) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(data, ArrayRef<>(data, 4).getData());
  ASSERT_EQ(4, ArrayRef<>(data, 4).getSize());
}

TEST(ArrayRefTests, arrayConstructor) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(data, ArrayRef<>(data).getData());
  ASSERT_EQ(4, ArrayRef<>(data).getSize());
}

TEST(ArrayRefTests, copyConstructor) {
  uint8_t data[4] = {2, 3, 5, 7};
  const ArrayRef<> ref(data);
  ASSERT_EQ(ref.getData(), ArrayRef<>(ref).getData());
  ASSERT_EQ(ref.getSize(), ArrayRef<>(ref).getSize());
}

TEST(ArrayRefTests, copyAssignment) {
  uint8_t data[4] = {2, 3, 5, 7};
  const ArrayRef<> ref1(data);
  ArrayRef<> ref2;
  ref2 = ref1;
  ASSERT_EQ(ref1.getData(), ref2.getData());
  ASSERT_EQ(ref1.getSize(), ref2.getSize());
}

TEST(ArrayRefTests, arrayView) {
  uint8_t data[4] = {2, 3, 5, 7};
  const ArrayRef<> ref(data);
  ArrayView<> view = ref;
  ASSERT_EQ(ref.getData(), view.getData());
  ASSERT_EQ(ref.getSize(), view.getSize());
}

TEST(ArrayRefTests, emptyNil) {
  ASSERT_TRUE(ArrayRef<>::EMPTY.isEmpty());
  ASSERT_FALSE(ArrayRef<>::EMPTY.isNil());
  ASSERT_TRUE(ArrayRef<>::NIL.isEmpty());
  ASSERT_TRUE(ArrayRef<>::NIL.isNil());
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_TRUE(ArrayRef<>(data, 0).isEmpty());
  ASSERT_FALSE(ArrayRef<>(data, 0).isNil());
  ASSERT_FALSE(ArrayRef<>(data).isEmpty());
  ASSERT_FALSE(ArrayRef<>(data).isNil());
}

TEST(ArrayRefTests, squareBrackets) {
  uint8_t data[4] = {2, 3, 5, 7};
  const ArrayRef<> ref(data);
  ASSERT_EQ(data + 0, &ref[0]);
  ASSERT_EQ(data + 1, &ref[1]);
  ASSERT_EQ(data + 2, &ref[2]);
  ASSERT_EQ(data + 3, &ref[3]);
}

TEST(ArrayRefTests, firstLast) {
  uint8_t data[4] = {2, 3, 5, 7};
  const ArrayRef<> ref(data);
  ASSERT_EQ(data + 0, &ref.first());
  ASSERT_EQ(data + 3, &ref.last());
}

TEST(ArrayRefTests, beginEnd) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(nullptr, ArrayRef<>::NIL.begin());
  ASSERT_EQ(nullptr, ArrayRef<>::NIL.end());
  ASSERT_EQ(data, ArrayRef<>(data).begin());
  ASSERT_EQ(data + 4, ArrayRef<>(data).end());
  size_t offset = 0;
  for (uint8_t& value : ArrayRef<>(data)) {
    ASSERT_EQ(data[offset], value);
    ++offset;
  }
}

TEST(ArrayRefTests, comparisons) {
  uint8_t data1[3] = {2, 3, 5};
  uint8_t data2[4] = {2, 3, 5, 7};
  uint8_t data3[4] = {2, 3, 5, 7};
  uint8_t data4[5] = {2, 3, 5, 7, 11};
  uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_TRUE(ArrayRef<>::EMPTY == ArrayView<>::EMPTY);
  ASSERT_TRUE(ArrayRef<>::EMPTY == ArrayView<>::NIL);
  ASSERT_FALSE(ArrayRef<>::EMPTY == ArrayView<>(data1));
  ASSERT_TRUE(ArrayRef<>::NIL == ArrayView<>::EMPTY);
  ASSERT_TRUE(ArrayRef<>::NIL == ArrayView<>::NIL);
  ASSERT_FALSE(ArrayRef<>::NIL == ArrayView<>(data1));
  ASSERT_FALSE(ArrayRef<>(data2) == ArrayView<>::EMPTY);
  ASSERT_FALSE(ArrayRef<>(data2) == ArrayView<>::NIL);
  ASSERT_FALSE(ArrayRef<>(data2) == ArrayView<>(data1));
  ASSERT_TRUE(ArrayRef<>(data2) == ArrayView<>(data2));
  ASSERT_TRUE(ArrayRef<>(data2) == ArrayView<>(data3));
  ASSERT_FALSE(ArrayRef<>(data2) == ArrayView<>(data4));
  ASSERT_FALSE(ArrayRef<>(data2) == ArrayView<>(data5));
  ASSERT_FALSE(ArrayRef<>::EMPTY != ArrayView<>::EMPTY);
  ASSERT_FALSE(ArrayRef<>::EMPTY != ArrayView<>::NIL);
  ASSERT_TRUE(ArrayRef<>::EMPTY != ArrayView<>(data1));
  ASSERT_FALSE(ArrayRef<>::NIL != ArrayView<>::EMPTY);
  ASSERT_FALSE(ArrayRef<>::NIL != ArrayView<>::NIL);
  ASSERT_TRUE(ArrayRef<>::NIL != ArrayView<>(data1));
  ASSERT_TRUE(ArrayRef<>(data2) != ArrayView<>::EMPTY);
  ASSERT_TRUE(ArrayRef<>(data2) != ArrayView<>::NIL);
  ASSERT_TRUE(ArrayRef<>(data2) != ArrayView<>(data1));
  ASSERT_FALSE(ArrayRef<>(data2) != ArrayView<>(data2));
  ASSERT_FALSE(ArrayRef<>(data2) != ArrayView<>(data3));
  ASSERT_TRUE(ArrayRef<>(data2) != ArrayView<>(data4));
  ASSERT_TRUE(ArrayRef<>(data2) != ArrayView<>(data5));
}

TEST(ArrayRefTests, beginsWith) {
  uint8_t data1[3] = {2, 3, 5};
  uint8_t data2[4] = {2, 3, 5, 7};
  uint8_t data3[4] = {2, 3, 5, 7};
  uint8_t data4[5] = {2, 3, 5, 7, 11};
  uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_FALSE(ArrayRef<>::EMPTY.beginsWith(data1[0]));
  ASSERT_TRUE(ArrayRef<>::EMPTY.beginsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>::EMPTY.beginsWith(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayRef<>::EMPTY.beginsWith(ArrayView<>(data1)));
  ASSERT_FALSE(ArrayRef<>::NIL.beginsWith(data1[0]));
  ASSERT_TRUE(ArrayRef<>::NIL.beginsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>::NIL.beginsWith(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayRef<>::NIL.beginsWith(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayRef<>(data2).beginsWith(data1[0]));
  ASSERT_FALSE(ArrayRef<>(data2).beginsWith(data5[0]));
  ASSERT_TRUE(ArrayRef<>(data2).beginsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>(data2).beginsWith(ArrayView<>::NIL));
  ASSERT_TRUE(ArrayRef<>(data2).beginsWith(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayRef<>(data2).beginsWith(ArrayView<>(data2)));
  ASSERT_TRUE(ArrayRef<>(data2).beginsWith(ArrayView<>(data3)));
  ASSERT_FALSE(ArrayRef<>(data2).beginsWith(ArrayView<>(data4)));
  ASSERT_FALSE(ArrayRef<>(data2).beginsWith(ArrayView<>(data5)));
}

TEST(ArrayRefTests, contains) {
  uint8_t data1[2] = {3, 5};
  uint8_t data2[4] = {2, 3, 5, 7};
  uint8_t data3[4] = {2, 3, 5, 7};
  uint8_t data4[5] = {2, 3, 5, 7, 11};
  uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_FALSE(ArrayRef<>::EMPTY.contains(data1[1]));
  ASSERT_TRUE(ArrayRef<>::EMPTY.contains(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>::EMPTY.contains(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayRef<>::EMPTY.contains(ArrayView<>(data1)));
  ASSERT_FALSE(ArrayRef<>::NIL.contains(data1[1]));
  ASSERT_TRUE(ArrayRef<>::NIL.contains(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>::NIL.contains(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayRef<>::NIL.contains(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayRef<>(data2).contains(data1[1]));
  ASSERT_FALSE(ArrayRef<>(data2).contains(data5[1]));
  ASSERT_TRUE(ArrayRef<>(data2).contains(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>(data2).contains(ArrayView<>::NIL));
  ASSERT_TRUE(ArrayRef<>(data2).contains(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayRef<>(data2).contains(ArrayView<>(data2)));
  ASSERT_TRUE(ArrayRef<>(data2).contains(ArrayView<>(data3)));
  ASSERT_FALSE(ArrayRef<>(data2).contains(ArrayView<>(data4)));
  ASSERT_FALSE(ArrayRef<>(data2).contains(ArrayView<>(data5)));
}

TEST(ArrayRefTests, endsWith) {
  uint8_t data1[3] = {3, 5, 7};
  uint8_t data2[4] = {2, 3, 5, 7};
  uint8_t data3[4] = {2, 3, 5, 7};
  uint8_t data4[5] = {2, 3, 5, 7, 11};
  uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_FALSE(ArrayRef<>::EMPTY.endsWith(data1[2]));
  ASSERT_TRUE(ArrayRef<>::EMPTY.endsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>::EMPTY.endsWith(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayRef<>::EMPTY.endsWith(ArrayView<>(data1)));
  ASSERT_FALSE(ArrayRef<>::NIL.endsWith(data1[2]));
  ASSERT_TRUE(ArrayRef<>::NIL.endsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>::NIL.endsWith(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayRef<>::NIL.endsWith(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayRef<>(data2).endsWith(data1[2]));
  ASSERT_FALSE(ArrayRef<>(data2).endsWith(data5[3]));
  ASSERT_TRUE(ArrayRef<>(data2).endsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayRef<>(data2).endsWith(ArrayView<>::NIL));
  ASSERT_TRUE(ArrayRef<>(data2).endsWith(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayRef<>(data2).endsWith(ArrayView<>(data2)));
  ASSERT_TRUE(ArrayRef<>(data2).endsWith(ArrayView<>(data3)));
  ASSERT_FALSE(ArrayRef<>(data2).endsWith(ArrayView<>(data4)));
  ASSERT_FALSE(ArrayRef<>(data2).endsWith(ArrayView<>(data5)));
}

TEST(ArrayRefTests, find) {
  uint8_t data1[2] = {3, 5};
  uint8_t data2[6] = {2, 3, 5, 3, 5, 7};
  uint8_t data3[6] = {2, 3, 5, 3, 5, 7};
  uint8_t data4[7] = {2, 3, 5, 3, 5, 7, 11};
  uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>::EMPTY.find(data1[0]));
  ASSERT_EQ(0, ArrayRef<>::EMPTY.find(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayRef<>::EMPTY.find(ArrayView<>::NIL));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>::EMPTY.find(ArrayView<>(data1)));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>::NIL.find(data1[0]));
  ASSERT_EQ(0, ArrayRef<>::NIL.find(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayRef<>::NIL.find(ArrayView<>::NIL));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>::NIL.find(ArrayView<>(data1)));
  ASSERT_EQ(1, ArrayRef<>(data2).find(data1[0]));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>(data2).find(data5[1]));
  ASSERT_EQ(0, ArrayRef<>(data2).find(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayRef<>(data2).find(ArrayView<>::NIL));
  ASSERT_EQ(1, ArrayRef<>(data2).find(ArrayView<>(data1)));
  ASSERT_EQ(0, ArrayRef<>(data2).find(ArrayView<>(data2)));
  ASSERT_EQ(0, ArrayRef<>(data2).find(ArrayView<>(data3)));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>(data2).find(ArrayView<>(data4)));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>(data2).find(ArrayView<>(data5)));
}

TEST(ArrayRefTests, findLast) {
  uint8_t data1[2] = {3, 5};
  uint8_t data2[6] = {2, 3, 5, 3, 5, 7};
  uint8_t data3[6] = {2, 3, 5, 3, 5, 7};
  uint8_t data4[7] = {2, 3, 5, 3, 5, 7, 11};
  uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>::EMPTY.findLast(data1[0]));
  ASSERT_EQ(0, ArrayRef<>::EMPTY.findLast(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayRef<>::EMPTY.findLast(ArrayView<>::NIL));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>::EMPTY.findLast(ArrayView<>(data1)));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>::NIL.findLast(data1[0]));
  ASSERT_EQ(0, ArrayRef<>::NIL.findLast(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayRef<>::NIL.findLast(ArrayView<>::NIL));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>::NIL.findLast(ArrayView<>(data1)));
  ASSERT_EQ(3, ArrayRef<>(data2).findLast(data1[0]));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>(data2).findLast(data5[1]));
  ASSERT_EQ(6, ArrayRef<>(data2).findLast(ArrayView<>::EMPTY));
  ASSERT_EQ(6, ArrayRef<>(data2).findLast(ArrayView<>::NIL));
  ASSERT_EQ(3, ArrayRef<>(data2).findLast(ArrayView<>(data1)));
  ASSERT_EQ(0, ArrayRef<>(data2).findLast(ArrayView<>(data2)));
  ASSERT_EQ(0, ArrayRef<>(data2).findLast(ArrayView<>(data3)));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>(data2).findLast(ArrayView<>(data4)));
  ASSERT_EQ(ArrayRef<>::INVALID, ArrayRef<>(data2).findLast(ArrayView<>(data5)));
}

TEST(ArrayRefTests, head) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayRef<>::EMPTY.head(0).getSize());
  ASSERT_EQ(ArrayRef<>(nullptr, 0), ArrayRef<>::NIL.head(0));
  ASSERT_EQ(ArrayRef<>(data, 0), ArrayRef<>(data).head(0));
  ASSERT_EQ(ArrayRef<>(data, 2), ArrayRef<>(data).head(2));
  ASSERT_EQ(ArrayRef<>(data, 4), ArrayRef<>(data).head(4));
}

TEST(ArrayRefTests, tail) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayRef<>::EMPTY.tail(0).getSize());
  ASSERT_EQ(ArrayRef<>(nullptr, 0), ArrayRef<>::NIL.tail(0));
  ASSERT_EQ(ArrayRef<>(data + 4, 0), ArrayRef<>(data).tail(0));
  ASSERT_EQ(ArrayRef<>(data + 2, 2), ArrayRef<>(data).tail(2));
  ASSERT_EQ(ArrayRef<>(data, 4), ArrayRef<>(data).tail(4));
}

TEST(ArrayRefTests, unhead) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayRef<>::EMPTY.unhead(0).getSize());
  ASSERT_EQ(ArrayRef<>(nullptr, 0), ArrayRef<>::NIL.unhead(0));
  ASSERT_EQ(ArrayRef<>(data, 4), ArrayRef<>(data).unhead(0));
  ASSERT_EQ(ArrayRef<>(data + 2, 2), ArrayRef<>(data).unhead(2));
  ASSERT_EQ(ArrayRef<>(data + 4, 0), ArrayRef<>(data).unhead(4));
}

TEST(ArrayRefTests, untail) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayRef<>::EMPTY.untail(0).getSize());
  ASSERT_EQ(ArrayRef<>(nullptr, 0), ArrayRef<>::NIL.untail(0));
  ASSERT_EQ(ArrayRef<>(data, 4), ArrayRef<>(data).untail(0));
  ASSERT_EQ(ArrayRef<>(data, 2), ArrayRef<>(data).untail(2));
  ASSERT_EQ(ArrayRef<>(data, 0), ArrayRef<>(data).untail(4));
}

TEST(ArrayRefTests, range) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayRef<>::EMPTY.range(0, 0).getSize());
  ASSERT_EQ(ArrayRef<>(nullptr, 0), ArrayRef<>::NIL.range(0, 0));
  ASSERT_EQ(ArrayRef<>(data + 0, 0), ArrayRef<>(data).range(0, 0));
  ASSERT_EQ(ArrayRef<>(data + 0, 2), ArrayRef<>(data).range(0, 2));
  ASSERT_EQ(ArrayRef<>(data + 0, 4), ArrayRef<>(data).range(0, 4));
  ASSERT_EQ(ArrayRef<>(data + 2, 0), ArrayRef<>(data).range(2, 2));
  ASSERT_EQ(ArrayRef<>(data + 2, 2), ArrayRef<>(data).range(2, 4));
  ASSERT_EQ(ArrayRef<>(data + 4, 0), ArrayRef<>(data).range(4, 4));
}

TEST(ArrayRefTests, slice) {
  uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayRef<>::EMPTY.slice(0, 0).getSize());
  ASSERT_EQ(ArrayRef<>(nullptr, 0), ArrayRef<>::NIL.slice(0, 0));
  ASSERT_EQ(ArrayRef<>(data + 0, 0), ArrayRef<>(data).slice(0, 0));
  ASSERT_EQ(ArrayRef<>(data + 0, 2), ArrayRef<>(data).slice(0, 2));
  ASSERT_EQ(ArrayRef<>(data + 0, 4), ArrayRef<>(data).slice(0, 4));
  ASSERT_EQ(ArrayRef<>(data + 2, 0), ArrayRef<>(data).slice(2, 0));
  ASSERT_EQ(ArrayRef<>(data + 2, 2), ArrayRef<>(data).slice(2, 2));
  ASSERT_EQ(ArrayRef<>(data + 4, 0), ArrayRef<>(data).slice(4, 0));
}

TEST(ArrayRefTests, fill) {
  uint8_t data[4] = {2, 3, 5, 7};
  const ArrayRef<> ref(data);
  ASSERT_EQ(ArrayRef<>(data), ref.fill(11));
  ASSERT_EQ(11, data[0]);
  ASSERT_EQ(11, data[1]);
  ASSERT_EQ(11, data[2]);
  ASSERT_EQ(11, data[3]);
}

TEST(ArrayRefTests, reverse) {
  uint8_t data[4] = {2, 3, 5, 7};
  const ArrayRef<> ref(data);
  ASSERT_EQ(ArrayRef<>(data), ref.reverse());
  ASSERT_EQ(7, data[0]);
  ASSERT_EQ(5, data[1]);
  ASSERT_EQ(3, data[2]);
  ASSERT_EQ(2, data[3]);
}
