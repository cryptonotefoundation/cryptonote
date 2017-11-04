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

#include <Common/ArrayView.h>
#include <gtest/gtest.h>

using namespace Common;

TEST(ArrayViewTests, representations) {
  ASSERT_NE(nullptr, ArrayView<>::EMPTY.getData());
  ASSERT_EQ(0, ArrayView<>::EMPTY.getSize());
  ASSERT_EQ(nullptr, ArrayView<>::NIL.getData());
  ASSERT_EQ(0, ArrayView<>::NIL.getSize());
}

TEST(ArrayViewTests, directConstructor) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(data, ArrayView<>(data, 4).getData());
  ASSERT_EQ(4, ArrayView<>(data, 4).getSize());
}

TEST(ArrayViewTests, arrayConstructor) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(data, ArrayView<>(data).getData());
  ASSERT_EQ(4, ArrayView<>(data).getSize());
}

TEST(ArrayViewTests, copyConstructor) {
  const uint8_t data[4] = {2, 3, 5, 7};
  const ArrayView<> view(data);
  ASSERT_EQ(view.getData(), ArrayView<>(view).getData());
  ASSERT_EQ(view.getSize(), ArrayView<>(view).getSize());
}

TEST(ArrayViewTests, copyAssignment) {
  const uint8_t data[4] = {2, 3, 5, 7};
  const ArrayView<> view1(data);
  ArrayView<> view2;
  view2 = view1;
  ASSERT_EQ(view1.getData(), view2.getData());
  ASSERT_EQ(view1.getSize(), view2.getSize());
}

TEST(ArrayViewTests, emptyNil) {
  ASSERT_TRUE(ArrayView<>::EMPTY.isEmpty());
  ASSERT_FALSE(ArrayView<>::EMPTY.isNil());
  ASSERT_TRUE(ArrayView<>::NIL.isEmpty());
  ASSERT_TRUE(ArrayView<>::NIL.isNil());
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_TRUE(ArrayView<>(data, 0).isEmpty());
  ASSERT_FALSE(ArrayView<>(data, 0).isNil());
  ASSERT_FALSE(ArrayView<>(data).isEmpty());
  ASSERT_FALSE(ArrayView<>(data).isNil());
}

TEST(ArrayViewTests, squareBrackets) {
  const uint8_t data[4] = {2, 3, 5, 7};
  const ArrayView<> view(data);
  ASSERT_EQ(data + 0, &view[0]);
  ASSERT_EQ(data + 1, &view[1]);
  ASSERT_EQ(data + 2, &view[2]);
  ASSERT_EQ(data + 3, &view[3]);
}

TEST(ArrayViewTests, firstLast) {
  const uint8_t data[4] = {2, 3, 5, 7};
  const ArrayView<> view(data);
  ASSERT_EQ(data + 0, &view.first());
  ASSERT_EQ(data + 3, &view.last());
}

TEST(ArrayViewTests, beginEnd) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(nullptr, ArrayView<>::NIL.begin());
  ASSERT_EQ(nullptr, ArrayView<>::NIL.end());
  ASSERT_EQ(data, ArrayView<>(data).begin());
  ASSERT_EQ(data + 4, ArrayView<>(data).end());
  size_t offset = 0;
  for (const uint8_t& value : ArrayView<>(data)) {
    ASSERT_EQ(data[offset], value);
    ++offset;
  }
}

TEST(ArrayViewTests, comparisons) {
  const uint8_t data1[3] = {2, 3, 5};
  const uint8_t data2[4] = {2, 3, 5, 7};
  const uint8_t data3[4] = {2, 3, 5, 7};
  const uint8_t data4[5] = {2, 3, 5, 7, 11};
  const uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_TRUE(ArrayView<>::EMPTY == ArrayView<>::EMPTY);
  ASSERT_TRUE(ArrayView<>::EMPTY == ArrayView<>::NIL);
  ASSERT_FALSE(ArrayView<>::EMPTY == ArrayView<>(data1));
  ASSERT_TRUE(ArrayView<>::NIL == ArrayView<>::EMPTY);
  ASSERT_TRUE(ArrayView<>::NIL == ArrayView<>::NIL);
  ASSERT_FALSE(ArrayView<>::NIL == ArrayView<>(data1));
  ASSERT_FALSE(ArrayView<>(data2) == ArrayView<>::EMPTY);
  ASSERT_FALSE(ArrayView<>(data2) == ArrayView<>::NIL);
  ASSERT_FALSE(ArrayView<>(data2) == ArrayView<>(data1));
  ASSERT_TRUE(ArrayView<>(data2) == ArrayView<>(data2));
  ASSERT_TRUE(ArrayView<>(data2) == ArrayView<>(data3));
  ASSERT_FALSE(ArrayView<>(data2) == ArrayView<>(data4));
  ASSERT_FALSE(ArrayView<>(data2) == ArrayView<>(data5));
  ASSERT_FALSE(ArrayView<>::EMPTY != ArrayView<>::EMPTY);
  ASSERT_FALSE(ArrayView<>::EMPTY != ArrayView<>::NIL);
  ASSERT_TRUE(ArrayView<>::EMPTY != ArrayView<>(data1));
  ASSERT_FALSE(ArrayView<>::NIL != ArrayView<>::EMPTY);
  ASSERT_FALSE(ArrayView<>::NIL != ArrayView<>::NIL);
  ASSERT_TRUE(ArrayView<>::NIL != ArrayView<>(data1));
  ASSERT_TRUE(ArrayView<>(data2) != ArrayView<>::EMPTY);
  ASSERT_TRUE(ArrayView<>(data2) != ArrayView<>::NIL);
  ASSERT_TRUE(ArrayView<>(data2) != ArrayView<>(data1));
  ASSERT_FALSE(ArrayView<>(data2) != ArrayView<>(data2));
  ASSERT_FALSE(ArrayView<>(data2) != ArrayView<>(data3));
  ASSERT_TRUE(ArrayView<>(data2) != ArrayView<>(data4));
  ASSERT_TRUE(ArrayView<>(data2) != ArrayView<>(data5));
}

TEST(ArrayViewTests, beginsWith) {
  const uint8_t data1[3] = {2, 3, 5};
  const uint8_t data2[4] = {2, 3, 5, 7};
  const uint8_t data3[4] = {2, 3, 5, 7};
  const uint8_t data4[5] = {2, 3, 5, 7, 11};
  const uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_FALSE(ArrayView<>::EMPTY.beginsWith(data1[0]));
  ASSERT_TRUE(ArrayView<>::EMPTY.beginsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>::EMPTY.beginsWith(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayView<>::EMPTY.beginsWith(ArrayView<>(data1)));
  ASSERT_FALSE(ArrayView<>::NIL.beginsWith(data1[0]));
  ASSERT_TRUE(ArrayView<>::NIL.beginsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>::NIL.beginsWith(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayView<>::NIL.beginsWith(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayView<>(data2).beginsWith(data1[0]));
  ASSERT_FALSE(ArrayView<>(data2).beginsWith(data5[0]));
  ASSERT_TRUE(ArrayView<>(data2).beginsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>(data2).beginsWith(ArrayView<>::NIL));
  ASSERT_TRUE(ArrayView<>(data2).beginsWith(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayView<>(data2).beginsWith(ArrayView<>(data2)));
  ASSERT_TRUE(ArrayView<>(data2).beginsWith(ArrayView<>(data3)));
  ASSERT_FALSE(ArrayView<>(data2).beginsWith(ArrayView<>(data4)));
  ASSERT_FALSE(ArrayView<>(data2).beginsWith(ArrayView<>(data5)));
}

TEST(ArrayViewTests, contains) {
  const uint8_t data1[2] = {3, 5};
  const uint8_t data2[4] = {2, 3, 5, 7};
  const uint8_t data3[4] = {2, 3, 5, 7};
  const uint8_t data4[5] = {2, 3, 5, 7, 11};
  const uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_FALSE(ArrayView<>::EMPTY.contains(data1[1]));
  ASSERT_TRUE(ArrayView<>::EMPTY.contains(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>::EMPTY.contains(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayView<>::EMPTY.contains(ArrayView<>(data1)));
  ASSERT_FALSE(ArrayView<>::NIL.contains(data1[1]));
  ASSERT_TRUE(ArrayView<>::NIL.contains(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>::NIL.contains(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayView<>::NIL.contains(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayView<>(data2).contains(data1[1]));
  ASSERT_FALSE(ArrayView<>(data2).contains(data5[1]));
  ASSERT_TRUE(ArrayView<>(data2).contains(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>(data2).contains(ArrayView<>::NIL));
  ASSERT_TRUE(ArrayView<>(data2).contains(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayView<>(data2).contains(ArrayView<>(data2)));
  ASSERT_TRUE(ArrayView<>(data2).contains(ArrayView<>(data3)));
  ASSERT_FALSE(ArrayView<>(data2).contains(ArrayView<>(data4)));
  ASSERT_FALSE(ArrayView<>(data2).contains(ArrayView<>(data5)));
}

TEST(ArrayViewTests, endsWith) {
  const uint8_t data1[3] = {3, 5, 7};
  const uint8_t data2[4] = {2, 3, 5, 7};
  const uint8_t data3[4] = {2, 3, 5, 7};
  const uint8_t data4[5] = {2, 3, 5, 7, 11};
  const uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_FALSE(ArrayView<>::EMPTY.endsWith(data1[2]));
  ASSERT_TRUE(ArrayView<>::EMPTY.endsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>::EMPTY.endsWith(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayView<>::EMPTY.endsWith(ArrayView<>(data1)));
  ASSERT_FALSE(ArrayView<>::NIL.endsWith(data1[2]));
  ASSERT_TRUE(ArrayView<>::NIL.endsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>::NIL.endsWith(ArrayView<>::NIL));
  ASSERT_FALSE(ArrayView<>::NIL.endsWith(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayView<>(data2).endsWith(data1[2]));
  ASSERT_FALSE(ArrayView<>(data2).endsWith(data5[3]));
  ASSERT_TRUE(ArrayView<>(data2).endsWith(ArrayView<>::EMPTY));
  ASSERT_TRUE(ArrayView<>(data2).endsWith(ArrayView<>::NIL));
  ASSERT_TRUE(ArrayView<>(data2).endsWith(ArrayView<>(data1)));
  ASSERT_TRUE(ArrayView<>(data2).endsWith(ArrayView<>(data2)));
  ASSERT_TRUE(ArrayView<>(data2).endsWith(ArrayView<>(data3)));
  ASSERT_FALSE(ArrayView<>(data2).endsWith(ArrayView<>(data4)));
  ASSERT_FALSE(ArrayView<>(data2).endsWith(ArrayView<>(data5)));
}

TEST(ArrayViewTests, find) {
  const uint8_t data1[2] = {3, 5};
  const uint8_t data2[6] = {2, 3, 5, 3, 5, 7};
  const uint8_t data3[6] = {2, 3, 5, 3, 5, 7};
  const uint8_t data4[7] = {2, 3, 5, 3, 5, 7, 11};
  const uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>::EMPTY.find(data1[0]));
  ASSERT_EQ(0, ArrayView<>::EMPTY.find(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayView<>::EMPTY.find(ArrayView<>::NIL));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>::EMPTY.find(ArrayView<>(data1)));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>::NIL.find(data1[0]));
  ASSERT_EQ(0, ArrayView<>::NIL.find(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayView<>::NIL.find(ArrayView<>::NIL));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>::NIL.find(ArrayView<>(data1)));
  ASSERT_EQ(1, ArrayView<>(data2).find(data1[0]));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>(data2).find(data5[1]));
  ASSERT_EQ(0, ArrayView<>(data2).find(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayView<>(data2).find(ArrayView<>::NIL));
  ASSERT_EQ(1, ArrayView<>(data2).find(ArrayView<>(data1)));
  ASSERT_EQ(0, ArrayView<>(data2).find(ArrayView<>(data2)));
  ASSERT_EQ(0, ArrayView<>(data2).find(ArrayView<>(data3)));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>(data2).find(ArrayView<>(data4)));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>(data2).find(ArrayView<>(data5)));
}

TEST(ArrayViewTests, findLast) {
  const uint8_t data1[2] = {3, 5};
  const uint8_t data2[6] = {2, 3, 5, 3, 5, 7};
  const uint8_t data3[6] = {2, 3, 5, 3, 5, 7};
  const uint8_t data4[7] = {2, 3, 5, 3, 5, 7, 11};
  const uint8_t data5[4] = {13, 17, 19, 23};
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>::EMPTY.findLast(data1[0]));
  ASSERT_EQ(0, ArrayView<>::EMPTY.findLast(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayView<>::EMPTY.findLast(ArrayView<>::NIL));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>::EMPTY.findLast(ArrayView<>(data1)));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>::NIL.findLast(data1[0]));
  ASSERT_EQ(0, ArrayView<>::NIL.findLast(ArrayView<>::EMPTY));
  ASSERT_EQ(0, ArrayView<>::NIL.findLast(ArrayView<>::NIL));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>::NIL.findLast(ArrayView<>(data1)));
  ASSERT_EQ(3, ArrayView<>(data2).findLast(data1[0]));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>(data2).findLast(data5[1]));
  ASSERT_EQ(6, ArrayView<>(data2).findLast(ArrayView<>::EMPTY));
  ASSERT_EQ(6, ArrayView<>(data2).findLast(ArrayView<>::NIL));
  ASSERT_EQ(3, ArrayView<>(data2).findLast(ArrayView<>(data1)));
  ASSERT_EQ(0, ArrayView<>(data2).findLast(ArrayView<>(data2)));
  ASSERT_EQ(0, ArrayView<>(data2).findLast(ArrayView<>(data3)));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>(data2).findLast(ArrayView<>(data4)));
  ASSERT_EQ(ArrayView<>::INVALID, ArrayView<>(data2).findLast(ArrayView<>(data5)));
}

TEST(ArrayViewTests, head) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayView<>::EMPTY.head(0).getSize());
  ASSERT_EQ(ArrayView<>(nullptr, 0), ArrayView<>::NIL.head(0));
  ASSERT_EQ(ArrayView<>(data, 0), ArrayView<>(data).head(0));
  ASSERT_EQ(ArrayView<>(data, 2), ArrayView<>(data).head(2));
  ASSERT_EQ(ArrayView<>(data, 4), ArrayView<>(data).head(4));
}

TEST(ArrayViewTests, tail) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayView<>::EMPTY.tail(0).getSize());
  ASSERT_EQ(ArrayView<>(nullptr, 0), ArrayView<>::NIL.tail(0));
  ASSERT_EQ(ArrayView<>(data + 4, 0), ArrayView<>(data).tail(0));
  ASSERT_EQ(ArrayView<>(data + 2, 2), ArrayView<>(data).tail(2));
  ASSERT_EQ(ArrayView<>(data, 4), ArrayView<>(data).tail(4));
}

TEST(ArrayViewTests, unhead) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayView<>::EMPTY.unhead(0).getSize());
  ASSERT_EQ(ArrayView<>(nullptr, 0), ArrayView<>::NIL.unhead(0));
  ASSERT_EQ(ArrayView<>(data, 4), ArrayView<>(data).unhead(0));
  ASSERT_EQ(ArrayView<>(data + 2, 2), ArrayView<>(data).unhead(2));
  ASSERT_EQ(ArrayView<>(data + 4, 0), ArrayView<>(data).unhead(4));
}

TEST(ArrayViewTests, untail) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayView<>::EMPTY.untail(0).getSize());
  ASSERT_EQ(ArrayView<>(nullptr, 0), ArrayView<>::NIL.untail(0));
  ASSERT_EQ(ArrayView<>(data, 4), ArrayView<>(data).untail(0));
  ASSERT_EQ(ArrayView<>(data, 2), ArrayView<>(data).untail(2));
  ASSERT_EQ(ArrayView<>(data, 0), ArrayView<>(data).untail(4));
}

TEST(ArrayViewTests, range) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayView<>::EMPTY.range(0, 0).getSize());
  ASSERT_EQ(ArrayView<>(nullptr, 0), ArrayView<>::NIL.range(0, 0));
  ASSERT_EQ(ArrayView<>(data + 0, 0), ArrayView<>(data).range(0, 0));
  ASSERT_EQ(ArrayView<>(data + 0, 2), ArrayView<>(data).range(0, 2));
  ASSERT_EQ(ArrayView<>(data + 0, 4), ArrayView<>(data).range(0, 4));
  ASSERT_EQ(ArrayView<>(data + 2, 0), ArrayView<>(data).range(2, 2));
  ASSERT_EQ(ArrayView<>(data + 2, 2), ArrayView<>(data).range(2, 4));
  ASSERT_EQ(ArrayView<>(data + 4, 0), ArrayView<>(data).range(4, 4));
}

TEST(ArrayViewTests, slice) {
  const uint8_t data[4] = {2, 3, 5, 7};
  ASSERT_EQ(0, ArrayView<>::EMPTY.slice(0, 0).getSize());
  ASSERT_EQ(ArrayView<>(nullptr, 0), ArrayView<>::NIL.slice(0, 0));
  ASSERT_EQ(ArrayView<>(data + 0, 0), ArrayView<>(data).slice(0, 0));
  ASSERT_EQ(ArrayView<>(data + 0, 2), ArrayView<>(data).slice(0, 2));
  ASSERT_EQ(ArrayView<>(data + 0, 4), ArrayView<>(data).slice(0, 4));
  ASSERT_EQ(ArrayView<>(data + 2, 0), ArrayView<>(data).slice(2, 0));
  ASSERT_EQ(ArrayView<>(data + 2, 2), ArrayView<>(data).slice(2, 2));
  ASSERT_EQ(ArrayView<>(data + 4, 0), ArrayView<>(data).slice(4, 0));
}
