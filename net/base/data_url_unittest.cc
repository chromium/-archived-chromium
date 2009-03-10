// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

  struct ParseTestData {
    const char* url;
    bool is_valid;
    const char* mime_type;
    const char* charset;
    const char* data;
  };

class DataURLTest : public testing::Test {
};

}

TEST(DataURLTest, Parse) {
  const ParseTestData tests[] = {
    { "data:",
       false,
       "",
       "",
       "" },

    { "data:,",
      true,
      "text/plain",
      "US-ASCII",
      "" },

    { "data:;base64,",
      true,
      "text/plain",
      "US-ASCII",
      "" },

    { "data:;charset=,test",
      true,
      "text/plain",
      "US-ASCII",
      "test" },

    { "data:TeXt/HtMl,<b>x</b>",
      true,
      "text/html",
      "US-ASCII",
      "<b>x</b>" },

    { "data:,foo",
      true,
      "text/plain",
      "US-ASCII",
      "foo" },

    { "data:;base64,aGVsbG8gd29ybGQ=",
      true,
      "text/plain",
      "US-ASCII",
      "hello world" },

    { "data:foo/bar;baz=1;charset=kk,boo",
      true,
      "foo/bar",
      "kk",
      "boo" },

    { "data:text/html,%3Chtml%3E%3Cbody%3E%3Cb%3Ehello%20world"
          "%3C%2Fb%3E%3C%2Fbody%3E%3C%2Fhtml%3E",
      true,
      "text/html",
      "US-ASCII",
      "<html><body><b>hello world</b></body></html>" },

    { "data:text/html,<html><body><b>hello world</b></body></html>",
      true,
      "text/html",
      "US-ASCII",
      "<html><body><b>hello world</b></body></html>" },

    // the comma cannot be url-escaped!
    { "data:%2Cblah",
      false,
      "",
      "",
      "" },

    // invalid base64 content
    { "data:;base64,aGVs_-_-",
      false,
      "",
      "",
      "" },

    // Spaces should be removed from non-text data URLs (we already tested
    // spaces above).
    { "data:image/fractal,a b c d e f g",
      true,
      "image/fractal",
      "US-ASCII",
      "abcdefg" },

    // Spaces should also be removed from anything base-64 encoded
    { "data:;base64,aGVs bG8gd2  9ybGQ=",
      true,
      "text/plain",
      "US-ASCII",
      "hello world" },

    // Other whitespace should also be removed from anything base-64 encoded.
    { "data:;base64,aGVs bG8gd2  \n9ybGQ=",
      true,
      "text/plain",
      "US-ASCII",
      "hello world" },

    // In base64 encoding, escaped whitespace should be stripped.
    // (This test was taken from acid3)
    // http://b/1054495
    { "data:text/javascript;base64,%20ZD%20Qg%0D%0APS%20An%20Zm91cic%0D%0A%207"
          "%20",
      true,
      "text/javascript",
      "US-ASCII",
      "d4 = 'four';" },

    // Only unescaped whitespace should be stripped in non-base64.
    // http://b/1157796
    { "data:img/png,A  B  %20  %0A  C",
      true,
      "img/png",
      "US-ASCII",
      "AB \nC" },

    // TODO(darin): add more interesting tests
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string mime_type;
    std::string charset;
    std::string data;
    bool ok =
        net::DataURL::Parse(GURL(tests[i].url), &mime_type, &charset, &data);
    EXPECT_EQ(ok, tests[i].is_valid);
    if (tests[i].is_valid) {
      EXPECT_EQ(tests[i].mime_type, mime_type);
      EXPECT_EQ(tests[i].charset, charset);
      EXPECT_EQ(tests[i].data, data);
    }
  }
}
