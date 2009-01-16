// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

class IFrameTest : public UITest {
 protected:
  void NavigateAndVerifyTitle(const wchar_t* url, const wchar_t* page_title) {
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, url);

    NavigateToURL(net::FilePathToFileURL(test_file));
    Sleep(sleep_timeout_ms());  // The browser lazily updates the title.

    // Make sure the navigation succeeded.
    EXPECT_EQ(std::wstring(page_title), GetActiveTabTitle());

    // UITest will check if this crashed.
  }

};

TEST_F(IFrameTest, Crash) {
  NavigateAndVerifyTitle(L"iframe.html", L"iframe test");
}

// http://crbug.com/3035 : Triggers a DCHECK in web_contents.cc.  Need to
// investigate.
TEST_F(IFrameTest, InEmptyFrame) {
  NavigateAndVerifyTitle(L"iframe_in_empty_frame.html", L"iframe test");
}

