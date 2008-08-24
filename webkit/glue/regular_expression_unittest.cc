// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#pragma warning(push, 0)
#include "PlatformString.h"
#include "RegularExpression.h"
#pragma warning(pop)

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/glue_util.h"

using std::wstring;
using webkit_glue::StdWStringToDeprecatedString;
using WebCore::DeprecatedString;
using WebCore::RegularExpression;

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
  const DeprecatedString pattern("the quick brown fox");
  RegularExpression regex(pattern, /* case sensitive */ true);
  EXPECT_EQ(0, regex.match(DeprecatedString("the quick brown fox")));
  EXPECT_EQ(1, regex.match(DeprecatedString(" the quick brown fox")));
  EXPECT_EQ(3, regex.match(DeprecatedString("foothe quick brown foxbar")));

  EXPECT_EQ(-1, regex.match(DeprecatedString("The quick brown FOX")));
  EXPECT_EQ(-1, regex.match(DeprecatedString("the quick brown fo")));
}

TEST(RegexTest, Unicode) {
  // Make sure we get the right offsets for unicode strings.

  // Test 1
  wstring wstr_pattern(L"\x6240\x6709\x7f51\x9875");
  DeprecatedString pattern = StdWStringToDeprecatedString(wstr_pattern);
  RegularExpression regex(pattern, /* case sensitive */ false);

  EXPECT_EQ(0, regex.match(StdWStringToDeprecatedString(wstr_pattern)));
  EXPECT_EQ(1, regex.match(StdWStringToDeprecatedString(
      wstring(L" ") + wstr_pattern)));
  EXPECT_EQ(3, regex.match(StdWStringToDeprecatedString(
      wstring(L"foo") + wstr_pattern + wstring(L"bar"))));
  EXPECT_EQ(4, regex.match(StdWStringToDeprecatedString(
      wstring(L"\x4e2d\x6587\x7f51\x9875") + wstr_pattern)));

  // Test 2, mixed length
  wstr_pattern = L":[ \x2000]+:";
  pattern = StdWStringToDeprecatedString(wstr_pattern);
  regex = RegularExpression(pattern, /* case sensitive */ false);
  
  const Match matches[] = {
    { L":  :", 0, 4 },
    { L"  :    :  ", 2, 6 },
    { L" : \x2000 : ", 1, 5 },
    { L"\x6240\x6709\x7f51\x9875 : \x2000 \x2000 : ", 5, 7 },
    { L"", -1, -1 },
    { L"::", -1, -1 },
  };
  for (size_t i = 0; i < arraysize(matches); ++i) {
    EXPECT_EQ(matches[i].position, regex.match(StdWStringToDeprecatedString(
        wstring(matches[i].text))));
    EXPECT_EQ(matches[i].length, regex.matchedLength());
  }

  // Test 3, empty match
  wstr_pattern = L"|x";
  pattern = StdWStringToDeprecatedString(wstr_pattern);
  regex = RegularExpression(pattern, /* case sensitive */ false);
  
  const Match matches2[] = {
    { L"", 0, 0 },
  };
  for (size_t i = 0; i < arraysize(matches2); ++i) {
    EXPECT_EQ(matches2[i].position, regex.match(StdWStringToDeprecatedString(
        wstring(matches2[i].text))));
    EXPECT_EQ(matches2[i].length, regex.matchedLength());
  }
}

