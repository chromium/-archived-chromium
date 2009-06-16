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

// History UI tests

#include "base/file_util.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "views/event.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";

class HistoryTester : public UITest {
 protected:
  HistoryTester() : UITest() { }

  virtual void SetUp() {
    show_window_ = true;
    UITest::SetUp();
  }
};

// TODO(yuzo): Fix the following flaky (hence disabled) tests.
// These tests are flaky because automatic and user-initiated transitions are
// distinguished based on the interval between page load and redirect.

TEST_F(HistoryTester, DISABLED_VerifyHistoryLength) {
  // Test the history length for the following page transitions.
  //
  // Test case 1:
  //   -open-> Page 1.
  // Test case 2:
  //   -open-> Page 2 -redirect-> Page 3.
  // Test case 3:
  //   -open-> Page 4 -navigate_backward-> Page 3 -navigate_backward->Page 1
  //   -navigate_forward-> Page 3 -navigate_forward-> Page 4
  //
  // Note that Page 2 is not visited on navigating backward/forward.

  // Test case 1
  std::wstring test_case_1 = L"history_length_test_page_1.html";
  GURL url_1 = GetTestUrl(L"History", test_case_1);
  NavigateToURL(url_1);
  WaitForFinish("History_Length_Test_1", "1", url_1, kTestCompleteCookie,
                  kTestCompleteSuccess, action_max_timeout_ms());

  // Test case 2
  std::wstring test_case_2 = L"history_length_test_page_2.html";
  GURL url_2 = GetTestUrl(L"History", test_case_2);
  NavigateToURL(url_2);
  WaitForFinish("History_Length_Test_2", "1", url_2, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());

  // Test case 3
  std::wstring test_case_3 = L"history_length_test_page_4.html";
  GURL url_3 = GetTestUrl(L"History", test_case_3);
  NavigateToURL(url_3);
  WaitForFinish("History_Length_Test_3", "1", url_3, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}

#if defined(OS_WIN)
// This test requires simulated mouse click, which is possible only for Windows.
TEST_F(HistoryTester, DISABLED_ConsiderRedirectAfterGestureAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 11 -slow_redirect-> Page 12.
  //
  // If redirect occurs after a user gesture, e.g., mouse click, the
  // redirect is more likely to be user-initiated rather than automatic.
  // Therefore, Page 11 should be in the history in addition to Page 12.

  std::wstring test_case = L"history_length_test_page_11.html";
  GURL url = GetTestUrl(L"History", test_case);
  NavigateToURL(url);
  WaitForFinish("History_Length_Test_11", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());

  // Simulate click. This only works for Windows.
  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(0);
  scoped_refptr<WindowProxy> window = browser->GetWindow();
  gfx::Rect tab_view_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &tab_view_bounds,
                                    true));
  POINT point(tab_view_bounds.CenterPoint().ToPOINT());
  ASSERT_TRUE(
      window->SimulateOSClick(point, views::Event::EF_LEFT_BUTTON_DOWN));

    NavigateToURL(GURL("javascript:redirectToPage12()"));
  WaitForFinish("History_Length_Test_12", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}
#endif  // defined(OS_WIN)

TEST_F(HistoryTester, DISABLED_ConsiderSlowRedirectAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 21 -redirect-> Page 22.
  //
  // If redirect occurs more than 5 seconds later after the page is loaded,
  // the redirect is likely to be user-initiated.
  // Therefore, Page 21 should be in the history in addition to Page 22.

  std::wstring test_case = L"history_length_test_page_21.html";
  GURL url = GetTestUrl(L"History", test_case);
  NavigateToURL(url);
  WaitForFinish("History_Length_Test_21", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, action_max_timeout_ms());
}
