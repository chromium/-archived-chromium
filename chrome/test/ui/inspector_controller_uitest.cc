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

#include "base/command_line.h"
#include "base/no_windows2000_unittest.h"
#include "base/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

// This test does not work on win2k. See http://b/1070036
class InspectorControllerTest : public NoWindows2000Test<UITest> {
 protected:
  TabProxy* GetActiveTabProxy() {
    scoped_ptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(window_proxy.get());

    int active_tab_index = 0;
    EXPECT_TRUE(window_proxy->GetActiveTabIndex(&active_tab_index));
    return window_proxy->GetTab(active_tab_index);
  }

  void NavigateTab(TabProxy* tab_proxy, const GURL& url) {
    ASSERT_TRUE(tab_proxy->NavigateToURL(url));
  }
};

// This test also does not work in single process. See http://b/1214920
TEST_F(InspectorControllerTest, InspectElement) {
  if (IsTestCaseDisabled())
    return;

  if (CommandLine().HasSwitch(switches::kSingleProcess))
    return;

  TestServer server(L"chrome/test/data");
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  // We don't track resources until we've opened the inspector.
  NavigateTab(tab.get(), server.TestServerPageW(L"files/inspector/test1.html"));
  tab->InspectElement(0, 0);
  NavigateTab(tab.get(), server.TestServerPageW(L"files/inspector/test1.html"));
  EXPECT_EQ(1, tab->InspectElement(0, 0));
  NavigateTab(tab.get(), server.TestServerPageW(L"files/inspector/test2.html"));
  EXPECT_EQ(2, tab->InspectElement(0, 0));
}
