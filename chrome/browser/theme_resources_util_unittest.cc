// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/theme_resources_util.h"

#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestCase {
  const char* name;
  int id;
};

}  // namespace

TEST(ThemeResourcesUtil, SpotCheckIds) {
  const TestCase kTestCases[] = {
    {"back", IDR_BACK},
    {"go", IDR_GO},
    {"star", IDR_STAR},
    {"sad_tab", IDR_SAD_TAB},
  };
  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    EXPECT_EQ(kTestCases[i].id, ThemeResourcesUtil::GetId(kTestCases[i].name));
  }

  // Should return -1 of unknown names.
  EXPECT_EQ(-1, ThemeResourcesUtil::GetId("foobar"));
  EXPECT_EQ(-1, ThemeResourcesUtil::GetId("backstar"));
}
