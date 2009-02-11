// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/query_parser.h"
#include "base/logging.h"
#include "chrome/common/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  for (size_t i = 0; i < arraysize(data); ++i) {
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
    const size_t m1_start;
    const size_t m1_end;
    const size_t m2_start;
    const size_t m2_end;
  } data[] = {
    { L"foo foo",       L"foo",              true,  0, 3, 0, 0 },
    { L"foo fooey",     L"fooey",            true,  0, 5, 0, 0 },
    { L"foo fooey bar", L"bar fooey",        true,  0, 3, 4, 9 },
    { L"blah",          L"blah",             true,  0, 4, 0, 0 },
    { L"blah",          L"foo",              false, 0, 0, 0, 0 },
    { L"blah",          L"blahblah",         true,  0, 4, 0, 0 },
    { L"blah",          L"foo blah",         true,  4, 8, 0, 0 },
    { L"foo blah",      L"blah",             false, 0, 0, 0, 0 },
    { L"foo blah",      L"blahx foobar",     true,  0, 4, 6, 9 },
    { L"\"foo blah\"",  L"foo blah",         true,  0, 8, 0, 0 },
    { L"\"foo blah\"",  L"foox blahx",       false, 0, 0, 0, 0 },
    { L"\"foo blah\"",  L"foo blah",         true,  0, 8, 0, 0 },
    { L"\"foo blah\"",  L"\"foo blah\"",     true,  1, 9, 0, 0 },
    { L"foo blah",      L"\"foo bar blah\"", true,  1, 4, 9, 13 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::vector<std::wstring> results;
    QueryParser parser;
    ScopedVector<QueryNode> query_nodes;
    parser.ParseQuery(data[i].query, &query_nodes.get());
    Snippet::MatchPositions match_positions;
    ASSERT_EQ(data[i].matches,
              parser.DoesQueryMatch(data[i].text, query_nodes.get(),
                                    &match_positions));
    size_t offset = 0;
    if (data[i].m1_start != 0 || data[i].m1_end != 0) {
      ASSERT_TRUE(match_positions.size() >= 1);
      EXPECT_EQ(data[i].m1_start, match_positions[0].first);
      EXPECT_EQ(data[i].m1_end, match_positions[0].second);
      offset++;
    }
    if (data[i].m2_start != 0 || data[i].m2_end != 0) {
      ASSERT_TRUE(match_positions.size() == 1 + offset);
      EXPECT_EQ(data[i].m2_start, match_positions[offset].first);
      EXPECT_EQ(data[i].m2_end, match_positions[offset].second);
    }
  }
}

TEST_F(QueryParserTest, ExtractQueryWords) {
  struct TestData2 {
    const std::wstring text;
    const std::wstring w1;
    const std::wstring w2;
    const std::wstring w3;
    const size_t word_count;
  } data[] = {
    { L"foo",           L"foo", L"",    L"",  1 },
    { L"foo bar",       L"foo", L"bar", L"",  2 },
    { L"\"foo bar\"",   L"foo", L"bar", L"",  2 },
    { L"\"foo bar\" a", L"foo", L"bar", L"a", 3 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::vector<std::wstring> results;
    QueryParser parser;
    parser.ExtractQueryWords(data[i].text, &results);
    ASSERT_EQ(data[i].word_count, results.size());
    EXPECT_EQ(data[i].w1, results[0]);
    if (results.size() == 2)
      EXPECT_EQ(data[i].w2, results[1]);
    if (results.size() == 3)
      EXPECT_EQ(data[i].w3, results[2]);
  }
}
