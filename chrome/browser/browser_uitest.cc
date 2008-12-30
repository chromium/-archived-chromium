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
     scoped_ptr<WindowProxy> window(
        automation()->GetWindowForBrowser(browser.get()));

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
  Sleep(kWaitForActionMsec);  // The browser lazily updates the title.

  EXPECT_EQ(WindowCaptionFromPageTitle(L"title1.html"), GetWindowTitle());
  EXPECT_EQ(L"title1.html", GetActiveTabTitle());
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
TEST_F(BrowserTest, Title) {
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"title2.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  Sleep(kWaitForActionMsec);  // The browser lazily updates the title.

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

// Tests the accelerators for tab navigation. Specifically IDC_SELECT_NEXT_TAB,
// IDC_SELECT_PREV_TAB, IDC_SELECT_TAB_0, and IDC_SELECT_LAST_TAB.
TEST_F(BrowserTest, TabNavigationAccelerators) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  // Create two new tabs. This way we'll have at least three tabs to navigate
  // to.
  int old_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&old_tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  int new_tab_count;
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
      5000));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  old_tab_count = new_tab_count;
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
      5000));
  ASSERT_GE(new_tab_count, 2);

  // Activate the second tab.
  ASSERT_TRUE(window->ActivateTab(1));

  // Navigate to the first tab using an accelerator.
  ASSERT_TRUE(window->ApplyAccelerator(IDC_SELECT_TAB_0));
  ASSERT_TRUE(window->WaitForTabToBecomeActive(0, 5000));

  // Navigate to the second tab using the next accelerators.
  ASSERT_TRUE(window->ApplyAccelerator(IDC_SELECT_NEXT_TAB));
  ASSERT_TRUE(window->WaitForTabToBecomeActive(1, 5000));

  // Navigate back to the first tab using the previous accelerators.
  ASSERT_TRUE(window->ApplyAccelerator(IDC_SELECT_PREVIOUS_TAB));
  ASSERT_TRUE(window->WaitForTabToBecomeActive(0, 5000));

  // Navigate to the last tab using the select last accelerator.
  ASSERT_TRUE(window->ApplyAccelerator(IDC_SELECT_LAST_TAB));
  ASSERT_TRUE(window->WaitForTabToBecomeActive(new_tab_count - 1, 5000));
}

TEST_F(BrowserTest, JavascriptAlertActivatesTab) {
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
                                               kWaitForActionMaxMsec));
}

TEST_F(BrowserTest, DuplicateTab) {
  std::wstring path_prefix = test_data_directory_;
  file_util::AppendToPath(&path_prefix, L"session_history");
  path_prefix += FilePath::kSeparators[0];
  GURL url1 = net::FilePathToFileURL(path_prefix + L"bot1.html");
  GURL url2 = net::FilePathToFileURL(path_prefix + L"bot2.html");
  GURL url3 = GURL("about:blank");

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));

  // Navigate to the three urls, then go back.
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  tab_proxy->NavigateToURL(url1);
  tab_proxy->NavigateToURL(url2);
  tab_proxy->NavigateToURL(url3);
  ASSERT_TRUE(tab_proxy->GoBack());

  int initial_window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&initial_window_count));

  // Duplicate the tab.
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_DUPLICATE_TAB));

  // The duplicated tab should not end up in a new window.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_TRUE(window_count == initial_window_count);

  tab_proxy.reset(browser_proxy->GetTab(1));
  ASSERT_TRUE(tab_proxy != NULL);
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));

  // Verify the stack of urls.
  GURL url;
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(url2, url);

  ASSERT_TRUE(tab_proxy->GoForward());
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(url3, url);

  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(url1, url);
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
  Sleep(kWaitForActionMsec);
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
  Sleep(kWaitForActionMsec);
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);

  // Same thing if the current tab tries to redirect itself.
  GURL dont_fork_url2(url_prefix +
      L"document.location=\"http://localhost:1337\";})()");

  // Make sure that no new process has been created.
  tab->NavigateToURLAsync(dont_fork_url2);
  Sleep(kWaitForActionMsec);
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
}

TEST_F(VisibleBrowserTest, WindowOpenClose) {
  std::wstring test_file(test_data_directory_);
  file_util::AppendToPath(&test_file, L"window.close.html");

  NavigateToURL(net::FilePathToFileURL(test_file));

  int i;
  for (i = 0; i < 10; ++i) {
    Sleep(kWaitForActionMaxMsec / 10);
    std::wstring title = GetActiveTabTitle();
    if (title == L"PASSED") {
      // Success, bail out.
      break;
    }
  }

  if (i == 10)
    FAIL() << "failed to get error page title";
}

