// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a file for random testcases that we run into that at one point or
// another have crashed the program.

#include "chrome/test/ui/ui_test.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/platform_thread.h"

class GoogleTest : public UITest {
 protected:
  GoogleTest() : UITest() {
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, L"google");
    file_util::AppendToPath(&test_file, L"google.html");
    homepage_ = test_file;
  }
};

TEST_F(GoogleTest, Crash) {
  std::wstring page_title = L"Google";

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UITest will check if this crashed.
}

class ColumnLayout : public UITest {
 protected:
  ColumnLayout() : UITest() {
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, L"columns.html");
    homepage_ = test_file;
  }
};

TEST_F(ColumnLayout, Crash) {
  std::wstring page_title = L"Column test";

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UIText will check if this crashed.
}
