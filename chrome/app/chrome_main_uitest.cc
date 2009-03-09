// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "net/base/net_util.h"

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
// This test is disabled. See bug 5671.
TEST_F(ChromeMainTest, DISABLED_SecondLaunch) {
  include_testing_id_ = false;
  use_existing_browser_ = true;

  LaunchBrowser(CommandLine(L""), false);

  int window_count;
  ASSERT_TRUE(automation()->WaitForWindowCountToChange(1, &window_count,
                                                       action_timeout_ms()));
  ASSERT_EQ(2, window_count);
}

TEST_F(ChromeMainTest, ReuseBrowserInstanceWhenOpeningFile) {
  include_testing_id_ = false;
  use_existing_browser_ = true;

  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"empty.html");

  CommandLine command_line(L"");
  command_line.AppendLooseValue(test_file);

  LaunchBrowser(command_line, false);

  FilePath test_file_path(FilePath::FromWStringHack(test_file));

  ASSERT_TRUE(automation()->WaitForURLDisplayed(
      net::FilePathToFileURL(test_file_path), action_timeout_ms()));
}
