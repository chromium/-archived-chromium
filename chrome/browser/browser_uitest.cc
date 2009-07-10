// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/gfx/native_widget_types.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/platform_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

class BrowserTest : public UITest {
 protected:
#if defined(OS_WIN)
  HWND GetMainWindow() {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    scoped_refptr<WindowProxy> window(browser->GetWindow());

    HWND window_handle;
    EXPECT_TRUE(window->GetHWND(&window_handle));
    return window_handle;
  }
#endif
};

class VisibleBrowserTest : public UITest {
 protected:
  VisibleBrowserTest() : UITest() {
    show_window_ = true;
  }
};

// Create 34 tabs and verify that a lot of processes have been created. The
// exact number of processes depends on the amount of memory. Previously we
// had a hard limit of 31 processes and this test is mainly directed at
// verifying that we don't crash when we pass this limit.
TEST_F(BrowserTest, ThirtyFourTabs) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title2.html");
  GURL url(net::FilePathToFileURL(test_file));
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  // There is one initial tab.
  for (int ix = 0; ix != 33; ++ix) {
    EXPECT_TRUE(window->AppendTab(url));
  }
  int tab_count = 0;
  EXPECT_TRUE(window->GetTabCount(&tab_count));
  EXPECT_EQ(34, tab_count);
  // Do not test the rest in single process mode.
  if (in_process_renderer())
    return;
  // See browser\renderer_host\render_process_host.cc for the algorithm to
  // decide how many processes to create.
  int process_count = GetBrowserProcessCount();
  if (base::SysInfo::AmountOfPhysicalMemoryMB() >= 2048) {
    EXPECT_GE(process_count, 24);
  } else {
    EXPECT_LE(process_count, 23);
  }
}

#if defined(OS_WIN)
// The browser should quit quickly if it receives a WM_ENDSESSION message.
TEST_F(BrowserTest, WindowsSessionEnd) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title1.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  PlatformThread::Sleep(action_timeout_ms());

  // Simulate an end of session. Normally this happens when the user
  // shuts down the pc or logs off.
  HWND window_handle = GetMainWindow();
  ASSERT_TRUE(::PostMessageW(window_handle, WM_ENDSESSION, 0, 0));

  PlatformThread::Sleep(action_timeout_ms());
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
#endif

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
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  FilePath test_file(test_data_directory_);
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  scoped_refptr<TabProxy> tab(window->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with a file:// url
  test_file = test_file.AppendASCII("title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_GE(orig_process_count, 1);

  // Use JavaScript URL to "fork" a new tab, just like Gmail.  (Open tab to a
  // blank page, set its opener to null, and redirect it cross-site.)
  std::wstring url_prefix(L"javascript:(function(){w=window.open();");
  GURL fork_url(url_prefix +
      L"w.opener=null;w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab has been created and that we have a new renderer
  // process for it.
  ASSERT_TRUE(tab->NavigateToURLAsync(fork_url));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count + 1, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);
}
#endif

#if !defined(OS_LINUX)
// TODO(port): This passes on linux locally, but fails on the try bot.
// Tests that non-Gmail-like script redirects (i.e., non-null window.opener) or
// a same-page-redirect) will not fork a new process.
TEST_F(BrowserTest, OtherRedirectsDontForkProcess) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  FilePath test_file(test_data_directory_);
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  scoped_refptr<TabProxy> tab(window->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with a file:// url
  test_file = test_file.AppendASCII("title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_GE(orig_process_count, 1);

  // Use JavaScript URL to almost fork a new tab, but not quite.  (Leave the
  // opener non-null.)  Should not fork a process.
  std::string url_prefix("javascript:(function(){w=window.open();");
  GURL dont_fork_url(url_prefix +
      "w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab but not new process has been created.
  ASSERT_TRUE(tab->NavigateToURLAsync(dont_fork_url));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);

  // Same thing if the current tab tries to redirect itself.
  GURL dont_fork_url2(url_prefix +
      "document.location=\"http://localhost:1337\";})()");

  // Make sure that no new process has been created.
  ASSERT_TRUE(tab->NavigateToURLAsync(dont_fork_url2));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
}
#endif

#if defined(OS_WIN)
// TODO(estade): need to port GetActiveTabTitle().
TEST_F(VisibleBrowserTest, WindowOpenClose) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("window.close.html");

  NavigateToURL(net::FilePathToFileURL(test_file));

  int i;
  for (i = 0; i < 10; ++i) {
    PlatformThread::Sleep(action_max_timeout_ms() / 10);
    std::wstring title = GetActiveTabTitle();
    if (title == L"PASSED") {
      // Success, bail out.
      break;
    }
  }

  if (i == 10)
    FAIL() << "failed to get error page title";
}
#endif

#if defined(OS_WIN)  // only works on Windows for now: http:://crbug.com/15891
class ShowModalDialogTest : public UITest {
 public:
  ShowModalDialogTest() {
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

TEST_F(ShowModalDialogTest, BasicTest) {
  // Test that a modal dialog is shown.
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("showmodaldialog.html");
  NavigateToURL(net::FilePathToFileURL(test_file));

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, action_timeout_ms()));

  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(1);
  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_EQ(title, L"ModalDialogTitle");

  // Test that window.close() works.  Since we don't have a way of executing a
  // JS function on the page through TabProxy, reload it and use an unload
  // handler that closes the page.
  ASSERT_EQ(tab->Reload(), AUTOMATION_MSG_NAVIGATION_SUCCESS);
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(1, action_timeout_ms()));
}
#endif

}  // namespace
