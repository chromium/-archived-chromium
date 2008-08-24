// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/basictypes.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::HttpUtil;

namespace {
class HttpUtilTest : public testing::Test {};
}

TEST(HttpUtilTest, HasHeader) {
  static const struct {
    const char* headers;
    const char* name;
    bool expected_result;
  } tests[] = {
    { "", "foo", false },
    { "foo\r\nbar", "foo", false },
    { "ffoo: 1", "foo", false },
    { "foo: 1", "foo", true },
    { "foo: 1\r\nbar: 2", "foo", true },
    { "fOO: 1\r\nbar: 2", "foo", true },
    { "g: 0\r\nfoo: 1\r\nbar: 2", "foo", true },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    bool result = HttpUtil::HasHeader(tests[i].headers, tests[i].name);
    EXPECT_EQ(tests[i].expected_result, result);
  }
}

TEST(HttpUtilTest, HeadersIterator) {
  std::string headers = "foo: 1\t\r\nbar: hello world\r\nbaz: 3 \r\n";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("foo"), it.name());
  EXPECT_EQ(std::string("1"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("bar"), it.name());
  EXPECT_EQ(std::string("hello world"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("baz"), it.name());
  EXPECT_EQ(std::string("3"), it.values());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, HeadersIterator_MalformedLine) {
  std::string headers = "foo: 1\n: 2\n3\nbar: 4";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n");

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("foo"), it.name());
  EXPECT_EQ(std::string("1"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("bar"), it.name());
  EXPECT_EQ(std::string("4"), it.values());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, ValuesIterator) {
  std::string values = " must-revalidate,   no-cache=\"foo, bar\"\t, private ";

  HttpUtil::ValuesIterator it(values.begin(), values.end(), ',');

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("must-revalidate"), it.value());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("no-cache=\"foo, bar\""), it.value());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("private"), it.value());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, ValuesIterator_Blanks) {
  std::string values = " \t ";

  HttpUtil::ValuesIterator it(values.begin(), values.end(), ',');

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, LocateEndOfHeaders) {
  struct {
    const char* input;
    int expected_result;
  } tests[] = {
    { "foo\r\nbar\r\n\r\n", 12 },
    { "foo\nbar\n\n", 9 },
    { "foo\r\nbar\r\n\r\njunk", 12 },
    { "foo\nbar\n\njunk", 9 },
    { "foo\nbar\n\r\njunk", 10 },
    { "foo\nbar\r\n\njunk", 10 },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    int input_len = static_cast<int>(strlen(tests[i].input));
    int eoh = HttpUtil::LocateEndOfHeaders(tests[i].input, input_len);
    EXPECT_EQ(tests[i].expected_result, eoh);
  }
}

TEST(HttpUtilTest, AssembleRawHeaders) {
  struct {
    const char* input;
    const char* expected_result;  // with '\0' changed to '|'
  } tests[] = {
    { "HTTP/1.0 200 OK\r\nFoo: 1\r\nBar: 2\r\n\r\n",
      "HTTP/1.0 200 OK|Foo: 1|Bar: 2||" },

    { "HTTP/1.0 200 OK\nFoo: 1\nBar: 2\n\n",
      "HTTP/1.0 200 OK|Foo: 1|Bar: 2||" },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    int input_len = static_cast<int>(strlen(tests[i].input));
    std::string raw = HttpUtil::AssembleRawHeaders(tests[i].input, input_len);
    std::replace(raw.begin(), raw.end(), '\0', '|');
    EXPECT_TRUE(raw == tests[i].expected_result);
  }
}

