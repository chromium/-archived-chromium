// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "PlatformString.h"
#include "RegularExpression.h"
MSVC_POP_WARNING();

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/glue_util.h"

using std::wstring;
using webkit_glue::StdWStringToString;
using WebCore::RegularExpression;
using WebCore::String;

namespace {

class RegexTest : public testing::Test {
};

struct Match {
  const std::wstring text;
  const int position;
  const int length;
};

}  // namespace

TEST(RegexTest, Basic) {
  // Just make sure we're not completely broken.
  const String pattern("the quick brown fox");
  RegularExpression regex(pattern, WebCore::TextCaseSensitive);
  EXPECT_EQ(0, regex.match("the quick brown fox"));
  EXPECT_EQ(1, regex.match(" the quick brown fox"));
  EXPECT_EQ(3, regex.match("foothe quick brown foxbar"));

  EXPECT_EQ(-1, regex.match("The quick brown FOX"));
  EXPECT_EQ(-1, regex.match("the quick brown fo"));
}

TEST(RegexTest, Unicode) {
  // Make sure we get the right offsets for unicode strings.

  // Test 1
  wstring wstr_pattern(L"\x6240\x6709\x7f51\x9875");
  String pattern = StdWStringToString(wstr_pattern);
  RegularExpression regex(pattern, WebCore::TextCaseInsensitive);

  EXPECT_EQ(0, regex.match(StdWStringToString(wstr_pattern)));
  EXPECT_EQ(1, regex.match(StdWStringToString(
      wstring(L" ") + wstr_pattern)));
  EXPECT_EQ(3, regex.match(StdWStringToString(
      wstring(L"foo") + wstr_pattern + wstring(L"bar"))));
  EXPECT_EQ(4, regex.match(StdWStringToString(
      wstring(L"\x4e2d\x6587\x7f51\x9875") + wstr_pattern)));

  // Test 2, mixed length
  wstr_pattern = L":[ \x2000]+:";
  pattern = StdWStringToString(wstr_pattern);
  regex = RegularExpression(pattern, WebCore::TextCaseInsensitive);

  const Match matches[] = {
    { L":  :", 0, 4 },
    { L"  :    :  ", 2, 6 },
    { L" : \x2000 : ", 1, 5 },
    { L"\x6240\x6709\x7f51\x9875 : \x2000 \x2000 : ", 5, 7 },
    { L"", -1, -1 },
    { L"::", -1, -1 },
  };
  for (size_t i = 0; i < arraysize(matches); ++i) {
    EXPECT_EQ(matches[i].position, regex.match(StdWStringToString(
        wstring(matches[i].text))));
    EXPECT_EQ(matches[i].length, regex.matchedLength());
  }

  // Test 3, empty match
  wstr_pattern = L"|x";
  pattern = StdWStringToString(wstr_pattern);
  regex = RegularExpression(pattern, WebCore::TextCaseInsensitive);

  const Match matches2[] = {
    { L"", 0, 0 },
  };
  for (size_t i = 0; i < arraysize(matches2); ++i) {
    EXPECT_EQ(matches2[i].position, regex.match(StdWStringToString(
        wstring(matches2[i].text))));
    EXPECT_EQ(matches2[i].length, regex.matchedLength());
  }
}

