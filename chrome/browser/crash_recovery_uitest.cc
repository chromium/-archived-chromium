// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/file_util.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef UITest CrashRecoveryUITest;

TEST_F(CrashRecoveryUITest, Reload) {
  // Test that reload works after a crash.

  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  // The title of the active tab should change each time this URL is loaded.
  GURL url(
      "data:text/html,<script>document.title=new Date().valueOf()</script>");

  NavigateToURL(url);

  std::wstring title1 = GetActiveTabTitle();

  scoped_ptr<TabProxy> tab(GetActiveTab());

  // Cause the renderer to crash.
  expected_crashes_ = 1;
  tab->NavigateToURLAsync(GURL("about:crash"));

  Sleep(1000);  // Wait for the browser to notice the renderer crash.

  tab->Reload();

  std::wstring title2 = GetActiveTabTitle();
  EXPECT_NE(title1, title2);
}

// Tests that loading a crashed page in a new tab correctly updates the title.
// There was an earlier bug (1270510) in process-per-site in which the max page
// ID of the RenderProcessHost was stale, so the NavigationEntry in the new tab
// was not committed.  This prevents regression of that bug.
TEST_F(CrashRecoveryUITest, LoadInNewTab) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  // The title of the active tab should change each time this URL is loaded.
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"title2.html");
  GURL url(net_util::FilePathToFileURL(test_file));

  NavigateToURL(url);

  const std::wstring title(L"Title Of Awesomeness");
  EXPECT_EQ(title, GetActiveTabTitle());

  scoped_ptr<TabProxy> tab(GetActiveTab());

  // Cause the renderer to crash.
  expected_crashes_ = 1;
  tab->NavigateToURLAsync(GURL("about:crash"));

  Sleep(1000);  // Wait for the browser to notice the renderer crash.

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy->AppendTab(url));

  // Ensure the title of the new tab is updated, indicating that the navigation
  // entry was properly committed.
  EXPECT_EQ(title, GetActiveTabTitle());
}
