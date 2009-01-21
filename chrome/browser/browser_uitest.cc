// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {

// Given a page title, returns the expected window caption string.
std::wstring WindowCaptionFromPageTitle(std::wstring page_title) {
  if (page_title.empty())
    return l10n_util::GetString(IDS_PRODUCT_NAME);

  return l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT, page_title);
}

class BrowserTest : public UITest {
 protected:
   HWND GetMainWindow() {
     scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
     scoped_ptr<WindowProxy> window(browser->GetWindow());

     HWND window_handle;
     EXPECT_TRUE(window->GetHWND(&window_handle));
     return window_handle;
   }

   std::wstring GetWindowTitle() {
     HWND window_handle = GetMainWindow();
     std::wstring result;
     int length = ::GetWindowTextLength(window_handle) + 1;
     ::GetWindowText(window_handle, WriteInto(&result, length), length);
     return result;
   }
};

class VisibleBrowserTest : public UITest {
 protected:
  VisibleBrowserTest() : UITest() {
    show_window_ = true;
  }
};

}  // namespace

// Launch the app on a page with no title, check that the app title was set
// correctly.
TEST_F(BrowserTest, NoTitle) {
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"title1.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  Sleep(sleep_timeout_ms());  // The browser lazily updates the title.

  EXPECT_EQ(WindowCaptionFromPageTitle(L"title1.html"), GetWindowTitle());
  EXPECT_EQ(L"title1.html", GetActiveTabTitle());
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
TEST_F(BrowserTest, Title) {
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"title2.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  Sleep(sleep_timeout_ms());  // The browser lazily updates the title.

  const std::wstring test_title(L"Title Of Awesomeness");
  EXPECT_EQ(WindowCaptionFromPageTitle(test_title), GetWindowTitle());
  EXPECT_EQ(test_title, GetActiveTabTitle());
}

// The browser should quit quickly if it receives a WM_ENDSESSION message.
TEST_F(BrowserTest, WindowsSessionEnd) {
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"title1.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  Sleep(action_timeout_ms());

  // Simulate an end of session. Normally this happens when the user
  // shuts down the pc or logs off.
  HWND window_handle = GetMainWindow();
  ASSERT_TRUE(::PostMessageW(window_handle, WM_ENDSESSION, 0, 0));

  Sleep(action_timeout_ms());
  ASSERT_FALSE(IsBrowserRunning());

  // Make sure the UMA metrics say we didn't crash.
  scoped_ptr<DictionaryValue> local_prefs(GetLocalState());
  bool exited_cleanly;
  ASSERT_TRUE(local_prefs.get());
  ASSERT_TRUE(local_prefs->GetBoolean(prefs::kStabilityExitedCleanly,
                                      &exited_cleanly));
  ASSERT_TRUE(exited_cleanly);

  // And that session end was successful.
  bool session_end_completed;
  ASSERT_TRUE(local_prefs->GetBoolean(prefs::kStabilitySessionEndCompleted,
                                      &session_end_completed));
  ASSERT_TRUE(session_end_completed);

  // Make sure session restore says we didn't crash.
  scoped_ptr<DictionaryValue> profile_prefs(GetDefaultProfilePreferences());
  ASSERT_TRUE(profile_prefs.get());
  ASSERT_TRUE(profile_prefs->GetBoolean(prefs::kSessionExitedCleanly,
                                        &exited_cleanly));
  ASSERT_TRUE(exited_cleanly);
}

// This test is flakey, see bug 5668 for details.
TEST_F(BrowserTest, DISABLED_JavascriptAlertActivatesTab) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  int start_index;
  ASSERT_TRUE(window->GetActiveTabIndex(&start_index));
  ASSERT_TRUE(window->AppendTab(GURL("about:blank")));
  int javascript_tab_index;
  ASSERT_TRUE(window->GetActiveTabIndex(&javascript_tab_index));
  TabProxy* javascript_tab = window->GetActiveTab();
  // Switch back to the starting tab, then send the second tab a javascript
  // alert, which should force it to become active.
  ASSERT_TRUE(window->ActivateTab(start_index));
  ASSERT_TRUE(
      javascript_tab->NavigateToURLAsync(GURL("javascript:alert('Alert!')")));
  ASSERT_TRUE(window->WaitForTabToBecomeActive(javascript_tab_index,
                                               action_max_timeout_ms()));
}

// Test that scripts can fork a new renderer process for a tab in a particular
// case (which matches following a link in Gmail).  The script must open a new
// tab, set its window.opener to null, and redirect it to a cross-site URL.
// (Bug 1115708)
// This test can only run if V8 is in use, and not KJS, because KJS will not
// set window.opener to null properly.
#ifdef CHROME_V8
TEST_F(BrowserTest, NullOpenerRedirectForksProcess) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());
  std::wstring test_file(test_data_directory_);
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab(window->GetActiveTab());

  // Start with a file:// url
  file_util::AppendToPath(&test_file, L"title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_TRUE(orig_process_count >= 1);

  // Use JavaScript URL to "fork" a new tab, just like Gmail.  (Open tab to a
  // blank page, set its opener to null, and redirect it cross-site.)
  std::wstring url_prefix(L"javascript:(function(){w=window.open();");
  GURL fork_url(url_prefix +
      L"w.opener=null;w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab has been created and that we have a new renderer
  // process for it.
  tab->NavigateToURLAsync(fork_url);
  Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count + 1, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);
}
#endif

// Tests that non-Gmail-like script redirects (i.e., non-null window.opener) or
// a same-page-redirect) will not fork a new process.
TEST_F(BrowserTest, OtherRedirectsDontForkProcess) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());
  std::wstring test_file(test_data_directory_);
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab(window->GetActiveTab());

  // Start with a file:// url
  file_util::AppendToPath(&test_file, L"title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_TRUE(orig_process_count >= 1);

  // Use JavaScript URL to almost fork a new tab, but not quite.  (Leave the
  // opener non-null.)  Should not fork a process.
  std::wstring url_prefix(L"javascript:(function(){w=window.open();");
  GURL dont_fork_url(url_prefix +
      L"w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab but not new process has been created.
  tab->NavigateToURLAsync(dont_fork_url);
  Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);

  // Same thing if the current tab tries to redirect itself.
  GURL dont_fork_url2(url_prefix +
      L"document.location=\"http://localhost:1337\";})()");

  // Make sure that no new process has been created.
  tab->NavigateToURLAsync(dont_fork_url2);
  Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
}

TEST_F(VisibleBrowserTest, WindowOpenClose) {
  std::wstring test_file(test_data_directory_);
  file_util::AppendToPath(&test_file, L"window.close.html");

  NavigateToURL(net::FilePathToFileURL(test_file));

  int i;
  for (i = 0; i < 10; ++i) {
    Sleep(action_max_timeout_ms() / 10);
    std::wstring title = GetActiveTabTitle();
    if (title == L"PASSED") {
      // Success, bail out.
      break;
    }
  }

  if (i == 10)
    FAIL() << "failed to get error page title";
}

