// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/query_parser.h"
#include "base/logging.h"
#include "chrome/common/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class QueryParserTest : public testing::Test {
 public:
  struct TestData {
    const std::wstring input;
    const int expected_word_count;
  };

  std::wstring QueryToString(const std::wstring& query);

 protected:
  QueryParser query_parser_;
};

};  // namespace

// Test helper: Convert a user query string to a SQLite query string.
std::wstring QueryParserTest::QueryToString(const std::wstring& query) {
  std::wstring sqlite_query;
  query_parser_.ParseQuery(query, &sqlite_query);
  return sqlite_query;
}

// Basic multi-word queries, including prefix matching.
TEST_F(QueryParserTest, SimpleQueries) {
  EXPECT_EQ(L"", QueryToString(L" "));
  EXPECT_EQ(L"singleword*", QueryToString(L"singleword"));
  EXPECT_EQ(L"spacedout*", QueryToString(L"  spacedout "));
  EXPECT_EQ(L"foo* bar*", QueryToString(L"foo bar"));
  // Short words aren't prefix matches. For Korean Hangul
  // the minimum is 2 while for other scripts, it's 3.
  EXPECT_EQ(L"f b", QueryToString(L" f b"));
  // KA JANG
  EXPECT_EQ(L"\xAC00 \xC7A5", QueryToString(L" \xAC00 \xC7A5"));
  EXPECT_EQ(L"foo* bar*", QueryToString(L" foo   bar "));
  // KA-JANG BICH-GO
  EXPECT_EQ(L"\xAC00\xC7A5* \xBE5B\xACE0*",
            QueryToString(L"\xAC00\xC7A5 \xBE5B\xACE0"));
}

// Quoted substring parsing.
TEST_F(QueryParserTest, Quoted) {
  EXPECT_EQ(L"\"Quoted\"", QueryToString(L"\"Quoted\""));    // ASCII quotes
  EXPECT_EQ(L"\"miss end\"", QueryToString(L"\"miss end"));  // Missing end quotes
  EXPECT_EQ(L"miss* beg*", QueryToString(L"miss beg\""));    // Missing begin quotes
  EXPECT_EQ(L"\"Many\" \"quotes\"", QueryToString(L"\"Many   \"\"quotes")); // Weird formatting
}

// Apostrophes within words should be preserved, but otherwise stripped.
TEST_F(QueryParserTest, Apostrophes) {
  EXPECT_EQ(L"foo* bar's*", QueryToString(L"foo bar's"));
  EXPECT_EQ(L"l'foo*", QueryToString(L"l'foo"));
  EXPECT_EQ(L"foo*", QueryToString(L"'foo"));
}

// Special characters.
TEST_F(QueryParserTest, SpecialChars) {
  EXPECT_EQ(L"foo* the* bar*", QueryToString(L"!#:/*foo#$*;'* the!#:/*bar"));
}

TEST_F(QueryParserTest, NumWords) {
  TestData data[] = {
    { L"blah",                  1 },
    { L"foo \"bar baz\"",       3 },
    { L"foo \"baz\"",           2 },
    { L"foo \"bar baz\"  blah", 4 },
  };

  for (int i = 0; i < arraysize(data); ++i) {
    std::wstring query_string;
    EXPECT_EQ(data[i].expected_word_count,
              query_parser_.ParseQuery(data[i].input, &query_string));
  }
}

TEST_F(QueryParserTest, ParseQueryNodesAndMatch) {
  struct TestData2 {
    const std::wstring query;
    const std::wstring text;
    const bool matches;
  } data[] = {
    { L"blah",                  L"blah",                          true  },
    { L"blah",                  L"foo",                           false },
    { L"blah",                  L"blahblah",                      true  },
    { L"blah",                  L"foo blah",                      true  },
    { L"foo blah",              L"blah",                          false },
    { L"foo blah",              L"blahx foobar",                  true  },
    { L"\"foo blah\"",          L"foo blah",                      true  },
    { L"\"foo blah\"",          L"foox blahx",                    false },
    { L"\"foo blah\"",          L"foo blah",                      true  },
    { L"\"foo blah\"",          L"\"foo blah\"",                  true },
    { L"foo blah",              L"\"foo bar blah\"",              true },
  };
  for (int i = 0; i < arraysize(data); ++i) {
    std::vector<std::wstring> results;
    QueryParser parser;
    ScopedVector<QueryNode> query_nodes;
    parser.ParseQuery(data[i].query, &query_nodes.get());
    ASSERT_EQ(data[i].matches,
              parser.DoesQueryMatch(data[i].text, query_nodes.get()));
  }
}
