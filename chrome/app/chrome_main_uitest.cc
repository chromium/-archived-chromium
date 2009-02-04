// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"
#include "chrome/test/ui/ui_test.h"

typedef UITest ChromeMainTest;

// Launch the app, then close the app.
TEST_F(ChromeMainTest, AppLaunch) {
  // If we make it here at all, we've succeeded in retrieving the app window
  // in UITest::SetUp()--otherwise we'd fail with an exception in SetUp().

  if (UITest::in_process_renderer()) {
    EXPECT_EQ(1, UITest::GetBrowserProcessCount());
  } else {
    // We should have two instances of the browser process alive -
    // one is the Browser and the other is the Renderer.
    EXPECT_EQ(2, UITest::GetBrowserProcessCount());
  }
}

// Make sure that the testing interface is there and giving reasonable answers.
TEST_F(ChromeMainTest, AppTestingInterface) {
  int window_count = 0;
  EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);

  EXPECT_EQ(1, GetTabCount());
}

// Make sure that the second invocation creates a new window.
TEST_F(ChromeMainTest, DISABLED_SecondLaunch) {
  include_testing_id_ = false;
  use_existing_browser_ = true;

  LaunchBrowser(CommandLine(L""), false);

  int window_count;
  ASSERT_TRUE(automation()->WaitForWindowCountToChange(1, &window_count,
                                                       action_timeout_ms()));
  ASSERT_EQ(2, window_count);
}

