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

#include <Common/StringBuffer.h>
#include <gtest/gtest.h>

using namespace Common;

TEST(StringBufferTests, defaultConstructor) {
  const StringBuffer<16> buffer;
  static_assert(buffer.MAXIMUM_SIZE == 16, "Wrong MAXIMUM_SIZE");
  ASSERT_LE(static_cast<const void*>(&buffer), static_cast<const void*>(buffer.getData()));
  ASSERT_GE(static_cast<const void*>(&buffer + 1), static_cast<const void*>(buffer.getData() + 16));
  ASSERT_EQ(0, buffer.getSize());
}

TEST(StringBufferTests, directConstructor) {
  const StringView view("ABCD");
  const StringBuffer<16> buffer(view.getData(), 4);
  ASSERT_LE(static_cast<const void*>(&buffer), static_cast<const void*>(buffer.getData()));
  ASSERT_GE(static_cast<const void*>(&buffer + 1), static_cast<const void*>(buffer.getData() + 16));
  ASSERT_EQ(0, memcmp(buffer.getData(), view.getData(), 4));
  ASSERT_EQ(4, buffer.getSize());
}

TEST(StringBufferTests, arrayConstructor) {
  const char data[] = "ABCD";
  const StringBuffer<16> buffer(data);
  ASSERT_LE(static_cast<const void*>(&buffer), static_cast<const void*>(buffer.getData()));
  ASSERT_GE(static_cast<const void*>(&buffer + 1), static_cast<const void*>(buffer.getData() + 16));
  ASSERT_EQ(0, memcmp(buffer.getData(), data, 4));
  ASSERT_EQ(4, buffer.getSize());
}

TEST(StringBufferTests, viewConstructor) {
  const StringView view("ABCD");
  const StringBuffer<16> buffer(view);
  ASSERT_LE(static_cast<const void*>(&buffer), static_cast<const void*>(buffer.getData()));
  ASSERT_GE(static_cast<const void*>(&buffer + 1), static_cast<const void*>(buffer.getData() + 16));
  ASSERT_EQ(0, memcmp(buffer.getData(), view.getData(), 4));
  ASSERT_EQ(4, buffer.getSize());
}

TEST(StringBufferTests, copyConstructor) {
  const StringBuffer<16> buffer1("ABCD");
  const StringBuffer<16> buffer2 = buffer1;
  ASSERT_LE(static_cast<const void*>(&buffer2), static_cast<const void*>(buffer2.getData()));
  ASSERT_GE(static_cast<const void*>(&buffer2 + 1), static_cast<const void*>(buffer2.getData() + 16));
  ASSERT_EQ(0, memcmp(buffer2.getData(), buffer1.getData(), 4));
  ASSERT_EQ(4, buffer2.getSize());
}

TEST(StringBufferTests, copyAssignment) {
  const StringBuffer<16> buffer1("ABCD");
  StringBuffer<16> buffer2;
  buffer2 = buffer1;
  ASSERT_LE(static_cast<const void*>(&buffer2), static_cast<const void*>(buffer2.getData()));
  ASSERT_GE(static_cast<const void*>(&buffer2 + 1), static_cast<const void*>(buffer2.getData() + 16));
  ASSERT_EQ(0, memcmp(buffer2.getData(), buffer1.getData(), 4));
  ASSERT_EQ(4, buffer2.getSize());
}

TEST(StringBufferTests, viewAssignment) {
  const StringView view("ABCD");
  StringBuffer<16> buffer;
  buffer = view;
  ASSERT_LE(static_cast<const void*>(&buffer), static_cast<const void*>(buffer.getData()));
  ASSERT_GE(static_cast<const void*>(&buffer + 1), static_cast<const void*>(buffer.getData() + 16));
  ASSERT_EQ(0, memcmp(buffer.getData(), view.getData(), 4));
  ASSERT_EQ(4, view.getSize());
}

TEST(StringBufferTests, view) {
  const StringBuffer<16> buffer("ABCD");
  const StringView view = buffer;
  ASSERT_EQ(buffer.getData(), view.getData());
  ASSERT_EQ(4, view.getSize());
}

TEST(StringBufferTests, getData) {
  StringBuffer<16> buffer1("ABCD");
  static_assert(std::is_same<decltype(buffer1.getData()), char*>::value, "Wrong operation result type");
  const StringBuffer<16> buffer2("ABCD");
  static_assert(std::is_same<decltype(buffer2.getData()), const char*>::value, "Wrong operation result type");
}

TEST(StringBufferTests, empty) {
  const StringBuffer<16> buffer;
  static_assert(std::is_same<decltype(buffer.isEmpty()), bool>::value, "Wrong operation result type");
  ASSERT_TRUE(buffer.isEmpty());
  ASSERT_FALSE(StringBuffer<16>("ABCD").isEmpty());
}

TEST(StringBufferTests, squareBrackets) {
  StringBuffer<16> buffer1("ABCD");
  static_assert(std::is_same<decltype(buffer1[0]), char&>::value, "Wrong operation result type");
  ASSERT_EQ(buffer1.getData(), &buffer1[0]);
  ASSERT_EQ(buffer1.getData() + 3, &buffer1[3]);
  const StringBuffer<16> buffer2("ABCD");
  static_assert(std::is_same<decltype(buffer2[0]), const char&>::value, "Wrong operation result type");
  ASSERT_EQ(buffer2.getData(), &buffer2[0]);
  ASSERT_EQ(buffer2.getData() + 3, &buffer2[3]);
}

TEST(StringBufferTests, firstLast) {
  StringBuffer<16> buffer1("ABCD");
  static_assert(std::is_same<decltype(buffer1.first()), char&>::value, "Wrong operation result type");
  static_assert(std::is_same<decltype(buffer1.last()), char&>::value, "Wrong operation result type");
  ASSERT_EQ(buffer1.getData(), &buffer1.first());
  ASSERT_EQ(buffer1.getData() + 3, &buffer1.last());
  const StringBuffer<16> buffer2("ABCD");
  static_assert(std::is_same<decltype(buffer2.first()), const char&>::value, "Wrong operation result type");
  static_assert(std::is_same<decltype(buffer2.last()), const char&>::value, "Wrong operation result type");
  ASSERT_EQ(buffer2.getData(), &buffer2.first());
  ASSERT_EQ(buffer2.getData() + 3, &buffer2.last());
}

TEST(StringBufferTests, beginEnd) {
  StringBuffer<16> buffer1("ABCD");
  static_assert(std::is_same<decltype(buffer1.begin()), char*>::value, "Wrong operation result type");
  static_assert(std::is_same<decltype(buffer1.end()), char*>::value, "Wrong operation result type");
  ASSERT_EQ(buffer1.getData(), buffer1.begin());
  ASSERT_EQ(buffer1.getData() + 4, buffer1.end());
  const StringBuffer<16> buffer2("ABCD");
  static_assert(std::is_same<decltype(buffer2.begin()), const char*>::value, "Wrong operation result type");
  static_assert(std::is_same<decltype(buffer2.end()), const char*>::value, "Wrong operation result type");
  ASSERT_EQ(buffer2.getData(), buffer2.begin());
  ASSERT_EQ(buffer2.getData() + 4, buffer2.end());
}

TEST(StringBufferTests, comparisons) {
  const StringView view1("ABC");
  const StringView view2("ABCD");
  const StringView view3("ABCD");
  const StringView view4("ABCDE");
  const StringView view5("FGHI");
  ASSERT_FALSE(StringBuffer<16>(view2) == StringView::EMPTY);
  ASSERT_FALSE(StringBuffer<16>(view2) == StringView::NIL);
  ASSERT_FALSE(StringBuffer<16>(view2) == view1);
  ASSERT_TRUE(StringBuffer<16>(view2) == view2);
  ASSERT_TRUE(StringBuffer<16>(view2) == view3);
  ASSERT_FALSE(StringBuffer<16>(view2) == view4);
  ASSERT_FALSE(StringBuffer<16>(view2) == view5);
  ASSERT_TRUE(StringBuffer<16>(view2) != StringView::EMPTY);
  ASSERT_TRUE(StringBuffer<16>(view2) != StringView::NIL);
  ASSERT_TRUE(StringBuffer<16>(view2) != view1);
  ASSERT_FALSE(StringBuffer<16>(view2) != view2);
  ASSERT_FALSE(StringBuffer<16>(view2) != view3);
  ASSERT_TRUE(StringBuffer<16>(view2) != view4);
  ASSERT_TRUE(StringBuffer<16>(view2) != view5);
  ASSERT_FALSE(StringBuffer<16>(view2) < StringView::EMPTY);
  ASSERT_FALSE(StringBuffer<16>(view2) < StringView::NIL);
  ASSERT_FALSE(StringBuffer<16>(view2) < view1);
  ASSERT_FALSE(StringBuffer<16>(view2) < view2);
  ASSERT_FALSE(StringBuffer<16>(view2) < view3);
  ASSERT_TRUE(StringBuffer<16>(view2) < view4);
  ASSERT_TRUE(StringBuffer<16>(view2) < view5);
  ASSERT_FALSE(StringBuffer<16>(view2) <= StringView::EMPTY);
  ASSERT_FALSE(StringBuffer<16>(view2) <= StringView::NIL);
  ASSERT_FALSE(StringBuffer<16>(view2) <= view1);
  ASSERT_TRUE(StringBuffer<16>(view2) <= view2);
  ASSERT_TRUE(StringBuffer<16>(view2) <= view3);
  ASSERT_TRUE(StringBuffer<16>(view2) <= view4);
  ASSERT_TRUE(StringBuffer<16>(view2) <= view5);
  ASSERT_TRUE(StringBuffer<16>(view2) > StringView::EMPTY);
  ASSERT_TRUE(StringBuffer<16>(view2) > StringView::NIL);
  ASSERT_TRUE(StringBuffer<16>(view2) > view1);
  ASSERT_FALSE(StringBuffer<16>(view2) > view2);
  ASSERT_FALSE(StringBuffer<16>(view2) > view3);
  ASSERT_FALSE(StringBuffer<16>(view2) > view4);
  ASSERT_FALSE(StringBuffer<16>(view2) > view5);
  ASSERT_TRUE(StringBuffer<16>(view2) >= StringView::EMPTY);
  ASSERT_TRUE(StringBuffer<16>(view2) >= StringView::NIL);
  ASSERT_TRUE(StringBuffer<16>(view2) >= view1);
  ASSERT_TRUE(StringBuffer<16>(view2) >= view2);
  ASSERT_TRUE(StringBuffer<16>(view2) >= view3);
  ASSERT_FALSE(StringBuffer<16>(view2) >= view4);
  ASSERT_FALSE(StringBuffer<16>(view2) >= view5);
}

TEST(StringBufferTests, beginsWith) {
  const StringView view1("ABC");
  const StringView view2("ABCD");
  const StringView view3("ABCD");
  const StringView view4("ABCDE");
  const StringView view5("FGHI");
  ASSERT_TRUE(StringBuffer<16>(view2).beginsWith(view1[0]));
  ASSERT_FALSE(StringBuffer<16>(view2).beginsWith(view5[0]));
  ASSERT_TRUE(StringBuffer<16>(view2).beginsWith(StringView::EMPTY));
  ASSERT_TRUE(StringBuffer<16>(view2).beginsWith(StringView::NIL));
  ASSERT_TRUE(StringBuffer<16>(view2).beginsWith(view1));
  ASSERT_TRUE(StringBuffer<16>(view2).beginsWith(view2));
  ASSERT_TRUE(StringBuffer<16>(view2).beginsWith(view3));
  ASSERT_FALSE(StringBuffer<16>(view2).beginsWith(view4));
  ASSERT_FALSE(StringBuffer<16>(view2).beginsWith(view5));
}

TEST(StringBufferTests, contains) {
  const StringView view1("BC");
  const StringView view2("ABCD");
  const StringView view3("ABCD");
  const StringView view4("ABCDE");
  const StringView view5("FGHI");
  ASSERT_TRUE(StringBuffer<16>(view2).contains(view1[1]));
  ASSERT_FALSE(StringBuffer<16>(view2).contains(view5[1]));
  ASSERT_TRUE(StringBuffer<16>(view2).contains(StringView::EMPTY));
  ASSERT_TRUE(StringBuffer<16>(view2).contains(StringView::NIL));
  ASSERT_TRUE(StringBuffer<16>(view2).contains(view1));
  ASSERT_TRUE(StringBuffer<16>(view2).contains(view2));
  ASSERT_TRUE(StringBuffer<16>(view2).contains(view3));
  ASSERT_FALSE(StringBuffer<16>(view2).contains(view4));
  ASSERT_FALSE(StringBuffer<16>(view2).contains(view5));
}

TEST(StringBufferTests, endsWith) {
  const StringView view1("BCD");
  const StringView view2("ABCD");
  const StringView view3("ABCD");
  const StringView view4("ABCDE");
  const StringView view5("FGHI");
  ASSERT_TRUE(StringBuffer<16>(view2).endsWith(view1[2]));
  ASSERT_FALSE(StringBuffer<16>(view2).endsWith(view5[3]));
  ASSERT_TRUE(StringBuffer<16>(view2).endsWith(StringView::EMPTY));
  ASSERT_TRUE(StringBuffer<16>(view2).endsWith(StringView::NIL));
  ASSERT_TRUE(StringBuffer<16>(view2).endsWith(view1));
  ASSERT_TRUE(StringBuffer<16>(view2).endsWith(view2));
  ASSERT_TRUE(StringBuffer<16>(view2).endsWith(view3));
  ASSERT_FALSE(StringBuffer<16>(view2).endsWith(view4));
  ASSERT_FALSE(StringBuffer<16>(view2).endsWith(view5));
}

TEST(StringBufferTests, find) {
  const StringView view1("BC");
  const StringView view2("ABCBCD");
  const StringView view3("ABCBCD");
  const StringView view4("ABCBCDE");
  const StringView view5("FGHI");
  ASSERT_EQ(1, StringBuffer<16>(view2).find(view1[0]));
  ASSERT_EQ(StringBuffer<16>::INVALID, StringBuffer<16>(view2).find(view5[1]));
  ASSERT_EQ(0, StringBuffer<16>(view2).find(StringView::EMPTY));
  ASSERT_EQ(0, StringBuffer<16>(view2).find(StringView::NIL));
  ASSERT_EQ(1, StringBuffer<16>(view2).find(view1));
  ASSERT_EQ(0, StringBuffer<16>(view2).find(view2));
  ASSERT_EQ(0, StringBuffer<16>(view2).find(view3));
  ASSERT_EQ(StringBuffer<16>::INVALID, StringBuffer<16>(view2).find(view4));
  ASSERT_EQ(StringBuffer<16>::INVALID, StringBuffer<16>(view2).find(view5));
}

TEST(StringBufferTests, findLast) {
  const StringView view1("BC");
  const StringView view2("ABCBCD");
  const StringView view3("ABCBCD");
  const StringView view4("ABCBCDE");
  const StringView view5("FGHI");
  ASSERT_EQ(3, StringBuffer<16>(view2).findLast(view1[0]));
  ASSERT_EQ(StringBuffer<16>::INVALID, StringBuffer<16>(view2).findLast(view5[1]));
  ASSERT_EQ(6, StringBuffer<16>(view2).findLast(StringView::EMPTY));
  ASSERT_EQ(6, StringBuffer<16>(view2).findLast(StringView::NIL));
  ASSERT_EQ(3, StringBuffer<16>(view2).findLast(view1));
  ASSERT_EQ(0, StringBuffer<16>(view2).findLast(view2));
  ASSERT_EQ(0, StringBuffer<16>(view2).findLast(view3));
  ASSERT_EQ(StringBuffer<16>::INVALID, StringBuffer<16>(view2).findLast(view4));
  ASSERT_EQ(StringBuffer<16>::INVALID, StringBuffer<16>(view2).findLast(view5));
}

TEST(StringBufferTests, head) {
  const StringBuffer<16> buffer("ABCD");
  ASSERT_EQ(StringView(buffer.getData(), 0), buffer.head(0));
  ASSERT_EQ(StringView(buffer.getData(), 2), buffer.head(2));
  ASSERT_EQ(StringView(buffer.getData(), 4), buffer.head(4));
}

TEST(StringBufferTests, tail) {
  const StringBuffer<16> buffer("ABCD");
  ASSERT_EQ(StringView(buffer.getData() + 4, 0), buffer.tail(0));
  ASSERT_EQ(StringView(buffer.getData() + 2, 2), buffer.tail(2));
  ASSERT_EQ(StringView(buffer.getData(), 4), buffer.tail(4));
}

TEST(StringBufferTests, unhead) {
  const StringBuffer<16> buffer("ABCD");
  ASSERT_EQ(StringView(buffer.getData(), 4), buffer.unhead(0));
  ASSERT_EQ(StringView(buffer.getData() + 2, 2), buffer.unhead(2));
  ASSERT_EQ(StringView(buffer.getData() + 4, 0), buffer.unhead(4));
}

TEST(StringBufferTests, untail) {
  const StringBuffer<16> buffer("ABCD");
  ASSERT_EQ(StringView(buffer.getData(), 4), buffer.untail(0));
  ASSERT_EQ(StringView(buffer.getData(), 2), buffer.untail(2));
  ASSERT_EQ(StringView(buffer.getData(), 0), buffer.untail(4));
}

TEST(StringBufferTests, range) {
  const StringBuffer<16> buffer("ABCD");
  ASSERT_EQ(StringView(buffer.getData() + 0, 0), buffer.range(0, 0));
  ASSERT_EQ(StringView(buffer.getData() + 0, 2), buffer.range(0, 2));
  ASSERT_EQ(StringView(buffer.getData() + 0, 4), buffer.range(0, 4));
  ASSERT_EQ(StringView(buffer.getData() + 2, 0), buffer.range(2, 2));
  ASSERT_EQ(StringView(buffer.getData() + 2, 2), buffer.range(2, 4));
  ASSERT_EQ(StringView(buffer.getData() + 4, 0), buffer.range(4, 4));
}

TEST(StringBufferTests, slice) {
  const StringBuffer<16> buffer("ABCD");
  ASSERT_EQ(StringView(buffer.getData() + 0, 0), buffer.slice(0, 0));
  ASSERT_EQ(StringView(buffer.getData() + 0, 2), buffer.slice(0, 2));
  ASSERT_EQ(StringView(buffer.getData() + 0, 4), buffer.slice(0, 4));
  ASSERT_EQ(StringView(buffer.getData() + 2, 0), buffer.slice(2, 0));
  ASSERT_EQ(StringView(buffer.getData() + 2, 2), buffer.slice(2, 2));
  ASSERT_EQ(StringView(buffer.getData() + 4, 0), buffer.slice(4, 0));
}

TEST(StringBufferTests, append) {
  ASSERT_EQ(StringView("E"), StringBuffer<16>("").append('E'));
  ASSERT_EQ(StringView(""), StringBuffer<16>("").append(""));
  ASSERT_EQ(StringView("EF"), StringBuffer<16>("").append("EF"));
  ASSERT_EQ(StringView("EFGH"), StringBuffer<16>("").append("EFGH"));
  ASSERT_EQ(StringView("ABE"), StringBuffer<16>("AB").append('E'));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").append(""));
  ASSERT_EQ(StringView("ABEF"), StringBuffer<16>("AB").append("EF"));
  ASSERT_EQ(StringView("ABEFGH"), StringBuffer<16>("AB").append("EFGH"));
  ASSERT_EQ(StringView("ABCDE"), StringBuffer<16>("ABCD").append('E'));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").append(""));
  ASSERT_EQ(StringView("ABCDEF"), StringBuffer<16>("ABCD").append("EF"));
  ASSERT_EQ(StringView("ABCDEFGH"), StringBuffer<16>("ABCD").append("EFGH"));
}

TEST(StringBufferTests, clear) {
  ASSERT_EQ(StringView(""), StringBuffer<16>("").clear());
  ASSERT_EQ(StringView(""), StringBuffer<16>("AB").clear());
  ASSERT_EQ(StringView(""), StringBuffer<16>("ABCD").clear());
}

TEST(StringBufferTests, cut) {
  ASSERT_EQ(StringView(""), StringBuffer<16>("").cut(0, 0));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").cut(0, 0));
  ASSERT_EQ(StringView(""), StringBuffer<16>("AB").cut(0, 2));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").cut(2, 0));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").cut(0, 0));
  ASSERT_EQ(StringView("CD"), StringBuffer<16>("ABCD").cut(0, 2));
  ASSERT_EQ(StringView(""), StringBuffer<16>("ABCD").cut(0, 4));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").cut(2, 0));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("ABCD").cut(2, 2));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").cut(4, 0));
}

TEST(StringBufferTests, fill) {
  ASSERT_EQ(StringView(""), StringBuffer<16>("").fill('E'));
  ASSERT_EQ(StringView("EE"), StringBuffer<16>("AB").fill('E'));
  ASSERT_EQ(StringView("EEEE"), StringBuffer<16>("ABCD").fill('E'));
}

TEST(StringBufferTests, insert) {
  ASSERT_EQ(StringView("E"), StringBuffer<16>("").insert(0, 'E'));
  ASSERT_EQ(StringView(""), StringBuffer<16>("").insert(0, ""));
  ASSERT_EQ(StringView("EF"), StringBuffer<16>("").insert(0, "EF"));
  ASSERT_EQ(StringView("EFGH"), StringBuffer<16>("").insert(0, "EFGH"));
  ASSERT_EQ(StringView("EAB"), StringBuffer<16>("AB").insert(0, 'E'));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").insert(0, ""));
  ASSERT_EQ(StringView("EFAB"), StringBuffer<16>("AB").insert(0, "EF"));
  ASSERT_EQ(StringView("EFGHAB"), StringBuffer<16>("AB").insert(0, "EFGH"));
  ASSERT_EQ(StringView("ABE"), StringBuffer<16>("AB").insert(2, 'E'));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").insert(2, ""));
  ASSERT_EQ(StringView("ABEF"), StringBuffer<16>("AB").insert(2, "EF"));
  ASSERT_EQ(StringView("ABEFGH"), StringBuffer<16>("AB").insert(2, "EFGH"));
  ASSERT_EQ(StringView("EABCD"), StringBuffer<16>("ABCD").insert(0, 'E'));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").insert(0, ""));
  ASSERT_EQ(StringView("EFABCD"), StringBuffer<16>("ABCD").insert(0, "EF"));
  ASSERT_EQ(StringView("EFGHABCD"), StringBuffer<16>("ABCD").insert(0, "EFGH"));
  ASSERT_EQ(StringView("ABECD"), StringBuffer<16>("ABCD").insert(2, 'E'));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").insert(2, ""));
  ASSERT_EQ(StringView("ABEFCD"), StringBuffer<16>("ABCD").insert(2, "EF"));
  ASSERT_EQ(StringView("ABEFGHCD"), StringBuffer<16>("ABCD").insert(2, "EFGH"));
  ASSERT_EQ(StringView("ABCDE"), StringBuffer<16>("ABCD").insert(4, 'E'));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").insert(4, ""));
  ASSERT_EQ(StringView("ABCDEF"), StringBuffer<16>("ABCD").insert(4, "EF"));
  ASSERT_EQ(StringView("ABCDEFGH"), StringBuffer<16>("ABCD").insert(4, "EFGH"));
}

TEST(StringBufferTests, overwrite) {
  ASSERT_EQ(StringView(""), StringBuffer<16>("").overwrite(0, ""));
  ASSERT_EQ(StringView("EF"), StringBuffer<16>("").overwrite(0, "EF"));
  ASSERT_EQ(StringView("EFGH"), StringBuffer<16>("").overwrite(0, "EFGH"));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").overwrite(0, ""));
  ASSERT_EQ(StringView("EF"), StringBuffer<16>("AB").overwrite(0, "EF"));
  ASSERT_EQ(StringView("EFGH"), StringBuffer<16>("AB").overwrite(0, "EFGH"));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").overwrite(2, ""));
  ASSERT_EQ(StringView("ABEF"), StringBuffer<16>("AB").overwrite(2, "EF"));
  ASSERT_EQ(StringView("ABEFGH"), StringBuffer<16>("AB").overwrite(2, "EFGH"));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").overwrite(0, ""));
  ASSERT_EQ(StringView("EFCD"), StringBuffer<16>("ABCD").overwrite(0, "EF"));
  ASSERT_EQ(StringView("EFGH"), StringBuffer<16>("ABCD").overwrite(0, "EFGH"));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").overwrite(2, ""));
  ASSERT_EQ(StringView("ABEF"), StringBuffer<16>("ABCD").overwrite(2, "EF"));
  ASSERT_EQ(StringView("ABEFGH"), StringBuffer<16>("ABCD").overwrite(2, "EFGH"));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").overwrite(4, ""));
  ASSERT_EQ(StringView("ABCDEF"), StringBuffer<16>("ABCD").overwrite(4, "EF"));
  ASSERT_EQ(StringView("ABCDEFGH"), StringBuffer<16>("ABCD").overwrite(4, "EFGH"));
}

TEST(StringBufferTests, resize) {
  ASSERT_EQ(StringView(""), StringBuffer<16>("").resize(0));
  ASSERT_EQ(StringView("\0\0"), StringBuffer<16>("").resize(2));
  ASSERT_EQ(StringView("\0\0\0\0"), StringBuffer<16>("").resize(4));
  ASSERT_EQ(StringView(""), StringBuffer<16>("AB").resize(0));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").resize(2));
  ASSERT_EQ(StringView("AB\0\0"), StringBuffer<16>("AB").resize(4));
  ASSERT_EQ(StringView(""), StringBuffer<16>("ABCD").resize(0));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("ABCD").resize(2));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").resize(4));
}

TEST(StringBufferTests, reverse) {
  ASSERT_EQ(StringView(""), StringBuffer<16>("").reverse());
  ASSERT_EQ(StringView("BA"), StringBuffer<16>("AB").reverse());
  ASSERT_EQ(StringView("DCBA"), StringBuffer<16>("ABCD").reverse());
}

TEST(StringBufferTests, shrink) {
  ASSERT_EQ(StringView(""), StringBuffer<16>("").shrink(0));
  ASSERT_EQ(StringView(""), StringBuffer<16>("AB").shrink(0));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("AB").shrink(2));
  ASSERT_EQ(StringView(""), StringBuffer<16>("ABCD").shrink(0));
  ASSERT_EQ(StringView("AB"), StringBuffer<16>("ABCD").shrink(2));
  ASSERT_EQ(StringView("ABCD"), StringBuffer<16>("ABCD").shrink(4));
}
