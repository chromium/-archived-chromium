// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include "chrome/test/ui/ui_test.h"

class ChromeProcessUtilTest : public UITest {
};

TEST_F(ChromeProcessUtilTest, SanityTest) {
  EXPECT_TRUE(IsBrowserRunning());
  EXPECT_NE(-1, ChromeBrowserProcessId(user_data_dir()));
  EXPECT_GE(GetRunningChromeProcesses(user_data_dir()).size(), 1U);
  QuitBrowser();
  EXPECT_FALSE(IsBrowserRunning());
  EXPECT_EQ(-1, ChromeBrowserProcessId(user_data_dir()));
  EXPECT_EQ(0U, GetRunningChromeProcesses(user_data_dir()).size());
}
