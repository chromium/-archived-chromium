// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "base/win_util.h"

// The test is somewhat silly, because the Vista bots some have UAC enabled
// and some have it disabled. At least we check that it does not crash.
TEST(BaseWinUtilTest, TestIsUACEnabled) {
  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
    win_util::UserAccountControlIsEnabled();
  } else {
    EXPECT_TRUE(win_util::UserAccountControlIsEnabled());
  }
}

TEST(BaseWinUtilTest, TestGetUserSidString) {
  std::wstring user_sid;
  EXPECT_TRUE(win_util::GetUserSidString(&user_sid));
  EXPECT_TRUE(!user_sid.empty());
}

TEST(BaseWinUtilTest, TestGetNonClientMetrics) {
  NONCLIENTMETRICS metrics = {0};
  win_util::GetNonClientMetrics(&metrics);
  EXPECT_TRUE(metrics.cbSize > 0);
  EXPECT_TRUE(metrics.iScrollWidth > 0);
  EXPECT_TRUE(metrics.iScrollHeight > 0);
}

