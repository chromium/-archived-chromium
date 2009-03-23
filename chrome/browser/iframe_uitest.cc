// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/platform_thread.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

class IFrameTest : public UITest {
 protected:
  void NavigateAndVerifyTitle(const wchar_t* url, const wchar_t* page_title) {
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, url);

    NavigateToURL(net::FilePathToFileURL(test_file));
    // The browser lazily updates the title.
    PlatformThread::Sleep(sleep_timeout_ms());

    // Make sure the navigation succeeded.
    EXPECT_EQ(std::wstring(page_title), GetActiveTabTitle());

    // UITest will check if this crashed.
  }

};

TEST_F(IFrameTest, Crash) {
  NavigateAndVerifyTitle(L"iframe.html", L"iframe test");
}

TEST_F(IFrameTest, InEmptyFrame) {
  NavigateAndVerifyTitle(L"iframe_in_empty_frame.html", L"iframe test");
}
