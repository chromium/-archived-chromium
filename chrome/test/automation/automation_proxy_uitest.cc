// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/app_switches.h"
#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/automation_proxy_uitest.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "views/event.h"

class AutomationProxyTest : public UITest {
 protected:
  AutomationProxyTest() {
    dom_automation_enabled_ = true;
    launch_arguments_.AppendSwitchWithValue(switches::kLang,
                                            L"en-US");
  }
};

TEST_F(AutomationProxyTest, GetBrowserWindowCount) {
  int window_count = 0;
  EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);
#ifdef NDEBUG
  ASSERT_FALSE(automation()->GetBrowserWindowCount(NULL));
#endif
}

TEST_F(AutomationProxyTest, GetBrowserWindow) {
  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(window.get());
  }

  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(-1));
    ASSERT_FALSE(window.get());
  }

  {
    scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(1));
    ASSERT_FALSE(window.get());
  }
};

// TODO(port): This test is for Chrome Views, which we only use on Windows.
// Maybe split this into a _win.cc file?
#if defined(OS_WIN)
TEST_F(AutomationProxyVisibleTest, WindowGetViewBounds) {
  {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());
    scoped_refptr<WindowProxy> window(browser->GetWindow());
    ASSERT_TRUE(window.get());

    scoped_refptr<TabProxy> tab1(browser->GetTab(0));
    ASSERT_TRUE(tab1.get());
    GURL tab1_url;
    ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

    // Add another tab so we can simulate dragging.
    ASSERT_TRUE(browser->AppendTab(GURL("about:")));

    scoped_refptr<TabProxy> tab2(browser->GetTab(1));
    ASSERT_TRUE(tab2.get());
    GURL tab2_url;
    ASSERT_TRUE(tab2->GetCurrentURL(&tab2_url));

    EXPECT_NE(tab1_url.spec(), tab2_url.spec());

    gfx::Rect bounds;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_0, &bounds, false));
    EXPECT_GT(bounds.x(), 0);
    EXPECT_GT(bounds.width(), 0);
    EXPECT_GT(bounds.height(), 0);

    gfx::Rect bounds2;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_LAST, &bounds2, false));
    EXPECT_GT(bounds2.width(), 0);
    EXPECT_GT(bounds2.height(), 0);

    // The tab logic is mirrored in RTL locales, so what is to the right in
    // LTR locales is now on the left with RTL ones.
    string16 browser_locale;

    EXPECT_TRUE(automation()->GetBrowserLocale(&browser_locale));

    const std::string& locale_utf8 = UTF16ToUTF8(browser_locale);
    if (l10n_util::GetTextDirectionForLocale(locale_utf8.c_str()) ==
        l10n_util::RIGHT_TO_LEFT) {
      EXPECT_LT(bounds2.x(), bounds.x());
    } else {
      EXPECT_GT(bounds2.x(), bounds.x());
    }
    EXPECT_EQ(bounds2.y(), bounds.y());

    gfx::Rect urlbar_bounds;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &urlbar_bounds,
                                      false));
    EXPECT_GT(urlbar_bounds.x(), 0);
    EXPECT_GT(urlbar_bounds.y(), 0);
    EXPECT_GT(urlbar_bounds.width(), 0);
    EXPECT_GT(urlbar_bounds.height(), 0);

    /*

    TODO(beng): uncomment this section or move to interactive_ui_tests post
    haste!

    // Now that we know where the tabs are, let's try dragging one.
    POINT start;
    POINT end;
    start.x = bounds.x() + bounds.width() / 2;
    start.y = bounds.y() + bounds.height() / 2;
    end.x = start.x + 2 * bounds.width() / 3;
    end.y = start.y;
    ASSERT_TRUE(browser->SimulateDrag(start, end,
                                      views::Event::EF_LEFT_BUTTON_DOWN));

    // Check to see that the drag event successfully swapped the two tabs.
    tab1 = browser->GetTab(0);
    ASSERT_TRUE(tab1.get());
    GURL tab1_new_url;
    ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

    tab2 = browser->GetTab(1);
    ASSERT_TRUE(tab2.get());
    GURL tab2_new_url;
    ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

    EXPECT_EQ(tab1_url.spec(), tab2_new_url.spec());
    EXPECT_EQ(tab2_url.spec(), tab1_new_url.spec());

    */
  }
}
#endif  // defined(OS_WIN)

TEST_F(AutomationProxyTest, GetTabCount) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int tab_count = 0;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);
}

TEST_F(AutomationProxyTest, GetActiveTabIndex) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);
}

TEST_F(AutomationProxyVisibleTest, AppendTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int original_tab_count;
  ASSERT_TRUE(window->GetTabCount(&original_tab_count));
  ASSERT_EQ(1, original_tab_count);  // By default there are 2 tabs opened.

  int original_active_tab_index;
  ASSERT_TRUE(window->GetActiveTabIndex(&original_active_tab_index));
  ASSERT_EQ(0, original_active_tab_index);  // By default 0-th tab is active

  ASSERT_TRUE(window->AppendTab(GURL("about:blank")));
  int tab_count;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(original_tab_count + 1, tab_count);

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(tab_count - 1, active_tab_index);
  ASSERT_NE(original_active_tab_index, active_tab_index);

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(filename)));

  int appended_tab_index;
  // Append tab will also be active tab
  ASSERT_TRUE(window->GetActiveTabIndex(&appended_tab_index));

  scoped_refptr<TabProxy> tab(window->GetTab(appended_tab_index));
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, ActivateTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  ASSERT_TRUE(window->AppendTab(GURL("about:blank")));

  int at_index = 1;
  ASSERT_TRUE(window->ActivateTab(at_index));
  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(at_index, active_tab_index);

  at_index = 0;
  ASSERT_TRUE(window->ActivateTab(at_index));
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(at_index, active_tab_index);
}


TEST_F(AutomationProxyTest, GetTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  {
    scoped_refptr<TabProxy> tab(window->GetTab(0));
    ASSERT_TRUE(tab.get());
    std::wstring title;
    ASSERT_TRUE(tab->GetTabTitle(&title));
    // BUG [634097] : expected title should be "about:blank"
    ASSERT_STREQ(L"", title.c_str());
  }

  {
    ASSERT_FALSE(window->GetTab(-1));
  }

  {
    scoped_refptr<TabProxy> tab(window->GetTab(1));
    ASSERT_FALSE(tab.get());
  }
};

TEST_F(AutomationProxyTest, NavigateToURL) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  // BUG [634097] : expected title should be "about:blank"
  ASSERT_STREQ(L"", title.c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");

  tab->NavigateToURL(net::FilePathToFileURL(filename));
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  // TODO(vibhor) : Add a test using testserver.
}

TEST_F(AutomationProxyTest, NavigateToURLWithTimeout1) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");

  bool is_timeout;
  tab->NavigateToURLWithTimeout(net::FilePathToFileURL(filename),
                                5000, &is_timeout);
  ASSERT_FALSE(is_timeout);

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, NavigateToURLWithTimeout2) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename1(test_data_directory_);
  filename1 = filename1.AppendASCII("title1.html");

  bool is_timeout;
  tab->NavigateToURLWithTimeout(net::FilePathToFileURL(filename1),
                                1, &is_timeout);
  ASSERT_TRUE(is_timeout);

  FilePath filename2(test_data_directory_);
  filename2 = filename2.AppendASCII("title1.html");
  tab->NavigateToURLWithTimeout(net::FilePathToFileURL(filename2),
                                5000, &is_timeout);
  ASSERT_FALSE(is_timeout);
}

TEST_F(AutomationProxyTest, GoBackForward) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  // BUG [634097] : expected title should be "about:blank"
  ASSERT_STREQ(L"", title.c_str());

  ASSERT_FALSE(tab->GoBack());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"", title.c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");
  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  ASSERT_TRUE(tab->GoBack());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  // BUG [634097] : expected title should be "about:blank"
  ASSERT_STREQ(L"", title.c_str());

  ASSERT_TRUE(tab->GoForward());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  ASSERT_FALSE(tab->GoForward());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, GetCurrentURL) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());
  GURL url;
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  ASSERT_STREQ("about:blank", url.spec().c_str());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("cookie1.html");
  GURL newurl = net::FilePathToFileURL(filename);
  ASSERT_TRUE(tab->NavigateToURL(newurl));
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  // compare canonical urls...
  ASSERT_STREQ(newurl.spec().c_str(), url.spec().c_str());
}

class AutomationProxyTest2 : public AutomationProxyVisibleTest {
 protected:
  AutomationProxyTest2() {
    document1_= test_data_directory_.AppendASCII("title1.html");

    document2_ = test_data_directory_.AppendASCII("title2.html");
    launch_arguments_ = CommandLine(L"");
    launch_arguments_.AppendLooseValue(document1_.ToWStringHack());
    launch_arguments_.AppendLooseValue(document2_.ToWStringHack());
  }

  FilePath document1_;
  FilePath document2_;
};

TEST_F(AutomationProxyTest2, GetActiveTabIndex) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  int tab_count;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(0, active_tab_index);
  int at_index = 1;
  ASSERT_TRUE(window->ActivateTab(at_index));
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(at_index, active_tab_index);
}

TEST_F(AutomationProxyTest2, GetTabTitle) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"title1.html", title.c_str());

  tab = window->GetTab(1);
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, Cookies) {
  GURL url("http://mojo.jojo.google.com");
  std::string value_result;

  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  // test setting the cookie:
  ASSERT_TRUE(tab->SetCookie(url, "foo=baz"));

  ASSERT_TRUE(tab->GetCookieByName(url, "foo", &value_result));
  ASSERT_FALSE(value_result.empty());
  ASSERT_STREQ("baz", value_result.c_str());

  // test clearing the cookie:
  ASSERT_TRUE(tab->SetCookie(url, "foo="));

  ASSERT_TRUE(tab->GetCookieByName(url, "foo", &value_result));
  ASSERT_TRUE(value_result.empty());

  // now, test that we can get multiple cookies:
  ASSERT_TRUE(tab->SetCookie(url, "foo1=baz1"));
  ASSERT_TRUE(tab->SetCookie(url, "foo2=baz2"));

  ASSERT_TRUE(tab->GetCookies(url, &value_result));
  ASSERT_FALSE(value_result.empty());
  EXPECT_TRUE(value_result.find("foo1=baz1") != std::string::npos);
  EXPECT_TRUE(value_result.find("foo2=baz2") != std::string::npos);
}

// TODO(port): Determine what tests need this and port.
#if defined(OS_WIN)
TEST_F(AutomationProxyTest, GetHWND) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());

  HWND handle;
  ASSERT_TRUE(window->GetHWND(&handle));
  ASSERT_TRUE(handle);
}
#endif

TEST_F(AutomationProxyTest, NavigateToURLAsync) {
  AutomationProxy* automation_object = automation();
  scoped_refptr<BrowserProxy> window(automation_object->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("cookie1.html");
  GURL newurl = net::FilePathToFileURL(filename);

  ASSERT_TRUE(tab->NavigateToURLAsync(newurl));
  std::string value = WaitUntilCookieNonEmpty(tab.get(), newurl, "foo", 250,
                                              action_max_timeout_ms());
  ASSERT_STREQ("baz", value.c_str());
}

TEST_F(AutomationProxyTest, AcceleratorNewTab) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));

  int tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  EXPECT_EQ(1, tab_count);

  ASSERT_TRUE(window->RunCommand(IDC_NEW_TAB));
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  EXPECT_EQ(2, tab_count);
  scoped_refptr<TabProxy> tab(window->GetTab(tab_count - 1));
  ASSERT_TRUE(tab.get());
}

class AutomationProxyTest4 : public UITest {
 protected:
  AutomationProxyTest4() : UITest() {
    dom_automation_enabled_ = true;
  }
};

std::wstring CreateJSString(const std::wstring& value) {
  std::wstring jscript;
  SStringPrintf(&jscript,
      L"window.domAutomationController.send(%ls);",
      value.c_str());
  return jscript;
}

TEST_F(AutomationProxyTest4, StringValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring expected(L"string");
  std::wstring jscript = CreateJSString(L"\"" + expected + L"\"");
  std::wstring actual;
  ASSERT_TRUE(tab->ExecuteAndExtractString(L"", jscript, &actual));
  ASSERT_STREQ(expected.c_str(), actual.c_str());
}

std::wstring BooleanToString(bool bool_value) {
  Value* value = Value::CreateBooleanValue(bool_value);
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(*value);
  return UTF8ToWide(json_string);
}

TEST_F(AutomationProxyTest4, BooleanValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  bool expected = true;
  std::wstring jscript = CreateJSString(BooleanToString(expected));
  bool actual = false;
  ASSERT_TRUE(tab->ExecuteAndExtractBool(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}

TEST_F(AutomationProxyTest4, NumberValueIsEchoedByDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  int expected = 1;
  int actual = 0;
  std::wstring expected_string;
  SStringPrintf(&expected_string, L"%d", expected);
  std::wstring jscript = CreateJSString(expected_string);
  ASSERT_TRUE(tab->ExecuteAndExtractInt(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}

// TODO(vibhor): Add a test for ExecuteAndExtractValue() for JSON Dictionary
// type value

class AutomationProxyTest3 : public UITest {
 protected:
  AutomationProxyTest3() : UITest() {
    document1_ = test_data_directory_;
    document1_ = document1_.AppendASCII("frame_dom_access");
    document1_ = document1_.AppendASCII("frame_dom_access.html");

    dom_automation_enabled_ = true;
    launch_arguments_ = CommandLine(L"");
    launch_arguments_.AppendLooseValue(document1_.ToWStringHack());
  }

  FilePath document1_;
};

std::wstring CreateJSStringForDOMQuery(const std::wstring& id) {
  std::wstring jscript(L"window.domAutomationController");
  StringAppendF(&jscript, L".send(document.getElementById('%ls').nodeName);",
                id.c_str());
  return jscript;
}

TEST_F(AutomationProxyTest3, FrameDocumentCanBeAccessed) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring actual;
  std::wstring xpath1 = L"";  // top level frame
  std::wstring jscript1 = CreateJSStringForDOMQuery(L"myinput");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath1, jscript1, &actual));
  ASSERT_EQ(L"INPUT", actual);

  std::wstring xpath2 = L"/html/body/iframe";
  std::wstring jscript2 = CreateJSStringForDOMQuery(L"myspan");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath2, jscript2, &actual));
  ASSERT_EQ(L"SPAN", actual);

  std::wstring xpath3 = L"/html/body/iframe\n/html/body/iframe";
  std::wstring jscript3 = CreateJSStringForDOMQuery(L"mydiv");
  ASSERT_TRUE(tab->ExecuteAndExtractString(xpath3, jscript3, &actual));
  ASSERT_EQ(L"DIV", actual);

  // TODO(evanm): fix or remove this.
  // This part of the test appears to verify that executing JS fails
  // non-HTML pages, but the new tab is now HTML so this test isn't
  // correct.
#if 0
  // Open a new Destinations tab to execute script inside.
  window->RunCommand(IDC_NEWTAB);
  tab = window->GetTab(1);
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(window->ActivateTab(1));

  ASSERT_FALSE(tab->ExecuteAndExtractString(xpath1, jscript1, &actual));
#endif
}

// TODO(port): Need to port constrained_window_proxy.* first.
#if defined(OS_WIN)
TEST_F(AutomationProxyTest, BlockedPopupTest) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("constrained_files");
  filename = filename.AppendASCII("constrained_window.html");

  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));

  ASSERT_TRUE(tab->WaitForBlockedPopupCountToChangeTo(2, 5000));
}

#endif  // defined(OS_WIN)

// TODO(port): Remove HWND if possible
#if defined(OS_WIN)
// Creates a top-level window, makes the |external_tab_window| a child of
// that window and displays them. After displaying the windows the
// function enters a message loop that processes window messages as well
// as calling MessageLoop::current()->RunAllPending() to process any
// incoming IPC messages. The time_to_wait_ms parameter is the maximum
// time the loop will run. To end the loop earlier, post a quit message to
// the thread.
bool ExternalTabMessageLoop(HWND external_tab_window, int time_to_wait_ms) {
  static const wchar_t class_name[] = L"External_Tab_UI_Test_Class";
  static const wchar_t window_title[] = L"External Tab Tester";

  WNDCLASSEX wnd_class = {0};
  wnd_class.cbSize = sizeof(wnd_class);
  wnd_class.style = CS_HREDRAW | CS_VREDRAW;
  wnd_class.lpfnWndProc = DefWindowProc;
  wnd_class.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wnd_class.lpszClassName = class_name;
  ATOM atom = RegisterClassEx(&wnd_class);
  if (0 == atom) {
    NOTREACHED() << "RegisterClassEx";
    return false;
  }

  unsigned long style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
  HWND external_tab_ui_parent = CreateWindow(class_name, window_title,
                                             style, CW_USEDEFAULT, 0,
                                             CW_USEDEFAULT, 0, NULL, NULL,
                                             NULL, NULL);
  style = GetWindowLong(external_tab_window, GWL_STYLE);
  style |= WS_CHILD;
  style &= ~WS_POPUP;
  SetWindowLong(external_tab_window, GWL_STYLE, style);
  SetParent(external_tab_window, external_tab_ui_parent);
  RECT client_rect = {0};
  GetClientRect(external_tab_ui_parent, &client_rect);
  SetWindowPos(external_tab_window, NULL, 0, 0, client_rect.right,
               client_rect.bottom, SWP_NOZORDER);
  ShowWindow(external_tab_window, SW_SHOW);
  ShowWindow(external_tab_ui_parent, SW_SHOW);

  // Allow the renderers to connect.

  const int kTimerIdQuit = 100;
  const int kTimerIdProcessPendingMessages = 101;

  ::SetTimer(external_tab_ui_parent, kTimerIdQuit, time_to_wait_ms, NULL);
  // Process pending messages every 50 milliseconds
  ::SetTimer(external_tab_ui_parent, kTimerIdProcessPendingMessages, 50, NULL);

  MSG msg;
  bool quit = false;
  do {
    BOOL ok = ::GetMessage(&msg, NULL, 0, 0);
    if (!ok || ok == -1)
      break;

    if (msg.message == WM_TIMER && msg.hwnd == external_tab_ui_parent) {
      switch (msg.wParam) {
        case kTimerIdProcessPendingMessages:
          MessageLoop::current()->RunAllPending();
          break;

        case kTimerIdQuit:
          ::KillTimer(external_tab_ui_parent, msg.wParam);
          quit = true;
          break;

        default:
          NOTREACHED() << "invalid timer id";
          break;
      }
    } else if (msg.message == WM_QUIT) {
      quit = true;
    } else {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  // In case there's an interactive user that closes the window.
  } while (!quit && ::IsWindow(external_tab_ui_parent));

  DestroyWindow(external_tab_ui_parent);

  EXPECT_TRUE(UnregisterClassW(reinterpret_cast<const wchar_t*>(atom),
                               wnd_class.hInstance));

  return true;
}

AutomationProxyForExternalTab::AutomationProxyForExternalTab(
    int execution_timeout)
    : AutomationProxy(execution_timeout),
      messages_received_(0),
      navigate_complete_(false) {
}

bool AutomationProxyForExternalTab::WaitForNavigationComplete(
    int max_time_to_wait_ms) {
  base::TimeTicks start(base::TimeTicks::Now());
  while (!navigate_complete_) {
    PlatformThread::Sleep(50);
    MessageLoop::current()->RunAllPending();
    base::TimeTicks end(base::TimeTicks::Now());
    base::TimeDelta delta = end - start;
    if (static_cast<int>(delta.InMilliseconds()) > max_time_to_wait_ms)
      return false;
  }
  return true;
}

void AutomationProxyForExternalTab::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(AutomationProxyForExternalTab, msg)
    IPC_MESSAGE_HANDLER(AutomationMsg_DidNavigate, OnDidNavigate)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardMessageToExternalHost,
                        OnForwardMessageToExternalHost)
  IPC_END_MESSAGE_MAP()
}

void AutomationProxyForExternalTab::OnForwardMessageToExternalHost(
    int handle,
    const std::string& message,
    const std::string& origin,
    const std::string& target) {
  messages_received_++;
  message_ = message;
  origin_ = origin;
  target_ = target;
  PostQuitMessage(0);
}

TEST_F(ExternalTabTestType, CreateExternalTab) {
  HWND external_tab_container = NULL;
  HWND tab_wnd = NULL;
  scoped_refptr<TabProxy> tab(automation()->CreateExternalTab(NULL, gfx::Rect(),
      WS_POPUP, false, &external_tab_container, &tab_wnd));
  EXPECT_TRUE(tab != NULL);
  EXPECT_NE(FALSE, ::IsWindow(external_tab_container));
  if (tab != NULL) {
    tab->NavigateInExternalTab(GURL(L"http://www.google.com"));
    EXPECT_EQ(true, ExternalTabMessageLoop(external_tab_container, 1000));
    // Since the tab goes away lazily, wait a bit
    PlatformThread::Sleep(1000);
    EXPECT_FALSE(tab->is_valid());
  }
}

TEST_F(ExternalTabTestType, IncognitoMode) {
  HWND external_tab_container = NULL;
  HWND tab_wnd = NULL;
  GURL url("http://anatomyofmelancholy.net");
  std::string value_result;

  // Create incognito tab
  scoped_refptr<TabProxy> tab(automation()->CreateExternalTab(NULL, gfx::Rect(),
      WS_POPUP, true, &external_tab_container, &tab_wnd));
  EXPECT_TRUE(tab->SetCookie(url, "robert=burton; "
                                  "expires=Thu, 13 Oct 2011 05:04:03 UTC;"));
  EXPECT_TRUE(tab->GetCookieByName(url, "robert", &value_result));
  EXPECT_EQ("burton", value_result);
  tab = NULL;
  CloseBrowserAndServer();

  value_result.empty();
  clear_profile_ = false;
  external_tab_container = NULL;
  tab_wnd = NULL;
  LaunchBrowserAndServer();
  tab = automation()->CreateExternalTab(NULL, gfx::Rect(),
      WS_POPUP, false, &external_tab_container, &tab_wnd);
  EXPECT_TRUE(tab->GetCookieByName(url, "robert", &value_result));
  EXPECT_EQ("", value_result);
}

TEST_F(ExternalTabTestType, ExternalTabPostMessage) {
  AutomationProxyForExternalTab* proxy =
      static_cast<AutomationProxyForExternalTab*>(automation());

  HWND external_tab_container = NULL;
  HWND tab_wnd = NULL;
  scoped_refptr<TabProxy> tab(proxy->CreateExternalTab(NULL, gfx::Rect(),
      WS_POPUP, false, &external_tab_container, &tab_wnd));
  EXPECT_TRUE(tab != NULL);
  EXPECT_NE(FALSE, ::IsWindow(external_tab_container));
  if (tab != NULL) {
    std::string content =
        "data:text/html,<html><head><script>"
        "function onload() {"
        "  window.externalHost.onmessage = onMessage;"
        "}"
        "function onMessage(evt) {"
        "  window.externalHost.postMessage(evt.data, '*');"
        "}"
        "</script></head>"
        "<body onload='onload()'>external tab test<br></div>"
        "</body></html>";
    tab->NavigateInExternalTab(GURL(content));
    EXPECT_TRUE(proxy->WaitForNavigationComplete(10000));

    tab->HandleMessageFromExternalHost("Hello from gtest", "null", "*");

    EXPECT_TRUE(ExternalTabMessageLoop(external_tab_container, 10000));
    EXPECT_NE(0, proxy->messages_received());

    if (proxy->messages_received()) {
      EXPECT_EQ("Hello from gtest", proxy->message());
    }
  }
}
#endif  // defined(OS_WIN)

// TODO(port): Need to port autocomplete_edit_proxy.* first.
#if defined(OS_WIN) || defined(OS_LINUX)
TEST_F(AutomationProxyTest, AutocompleteGetSetText) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<AutocompleteEditProxy> edit(
      browser->GetAutocompleteEdit());
  ASSERT_TRUE(edit.get());
  EXPECT_TRUE(edit->is_valid());
  const std::wstring text_to_set = L"Lollerskates";
  std::wstring actual_text;
  EXPECT_TRUE(edit->SetText(text_to_set));
  EXPECT_TRUE(edit->GetText(&actual_text));
  EXPECT_EQ(text_to_set, actual_text);
  scoped_refptr<AutocompleteEditProxy> edit2(
      browser->GetAutocompleteEdit());
  EXPECT_TRUE(edit2->GetText(&actual_text));
  EXPECT_EQ(text_to_set, actual_text);
}

TEST_F(AutomationProxyTest, AutocompleteParallelProxy) {
  scoped_refptr<BrowserProxy> browser1(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser1.get());
  scoped_refptr<AutocompleteEditProxy> edit1(
      browser1->GetAutocompleteEdit());
  ASSERT_TRUE(edit1.get());
  EXPECT_TRUE(browser1->RunCommand(IDC_NEW_WINDOW));
  scoped_refptr<BrowserProxy> browser2(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser2.get());
  scoped_refptr<AutocompleteEditProxy> edit2(
      browser2->GetAutocompleteEdit());
  ASSERT_TRUE(edit2.get());
  EXPECT_TRUE(browser2->GetTab(0)->WaitForTabToBeRestored(
      action_max_timeout_ms()));
  const std::wstring text_to_set1 = L"Lollerskates";
  const std::wstring text_to_set2 = L"Roflcopter";
  std::wstring actual_text1, actual_text2;
  EXPECT_TRUE(edit1->SetText(text_to_set1));
  EXPECT_TRUE(edit2->SetText(text_to_set2));
  EXPECT_TRUE(edit1->GetText(&actual_text1));
  EXPECT_TRUE(edit2->GetText(&actual_text2));
  EXPECT_EQ(text_to_set1, actual_text1);
  EXPECT_EQ(text_to_set2, actual_text2);
}

TEST_F(AutomationProxyVisibleTest, AutocompleteMatchesTest) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<AutocompleteEditProxy> edit(
      browser->GetAutocompleteEdit());
  ASSERT_TRUE(edit.get());
  EXPECT_TRUE(browser->ApplyAccelerator(IDC_FOCUS_LOCATION));
  EXPECT_TRUE(edit->is_valid());
  EXPECT_TRUE(edit->SetText(L"Roflcopter"));
  EXPECT_TRUE(edit->WaitForQuery(30000));
  bool query_in_progress;
  EXPECT_TRUE(edit->IsQueryInProgress(&query_in_progress));
  EXPECT_FALSE(query_in_progress);
  std::vector<AutocompleteMatchData> matches;
  EXPECT_TRUE(edit->GetAutocompleteMatches(&matches));
  EXPECT_FALSE(matches.empty());
}
#endif  // defined(OS_WIN) || defined(OS_LINUX)

// Disabled because flaky see bug #5314.
TEST_F(AutomationProxyTest, DISABLED_AppModalDialogTest) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  bool modal_dialog_showing = false;
  MessageBoxFlags::DialogButton button = MessageBoxFlags::DIALOGBUTTON_NONE;
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  EXPECT_EQ(MessageBoxFlags::DIALOGBUTTON_NONE, button);

  // Show a simple alert.
  std::string content =
      "data:text/html,<html><head><script>function onload() {"
      "setTimeout(\"alert('hello');\", 1000); }</script></head>"
      "<body onload='onload()'></body></html>";
  tab->NavigateToURL(GURL(content));
  EXPECT_TRUE(automation()->WaitForAppModalDialog(3000));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(MessageBoxFlags::DIALOGBUTTON_OK, button);

  // Test that clicking missing button fails graciously and does not close the
  // dialog.
  EXPECT_FALSE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_CANCEL));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);

  // Now click OK, that should close the dialog.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);

  // Show a confirm dialog.
  content =
      "data:text/html,<html><head><script>var result = -1; function onload() {"
      "setTimeout(\"result = confirm('hello') ? 0 : 1;\", 1000);} </script>"
      "</head><body onload='onload()'></body></html>";
  tab->NavigateToURL(GURL(content));
  EXPECT_TRUE(automation()->WaitForAppModalDialog(3000));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(MessageBoxFlags::DIALOGBUTTON_OK |
            MessageBoxFlags::DIALOGBUTTON_CANCEL, button);

  // Click OK.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  int result = -1;
  EXPECT_TRUE(tab->ExecuteAndExtractInt(
      L"", L"window.domAutomationController.send(result);", &result));
  EXPECT_EQ(0, result);

  // Try again.
  tab->NavigateToURL(GURL(content));
  EXPECT_TRUE(automation()->WaitForAppModalDialog(3000));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);
  EXPECT_EQ(MessageBoxFlags::DIALOGBUTTON_OK |
            MessageBoxFlags::DIALOGBUTTON_CANCEL, button);

  // Click Cancel this time.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_CANCEL));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  EXPECT_TRUE(tab->ExecuteAndExtractInt(
      L"", L"window.domAutomationController.send(result);", &result));
  EXPECT_EQ(1, result);
}

class AutomationProxyTest5 : public UITest {
 protected:
  AutomationProxyTest5() {
    show_window_ = true;
    dom_automation_enabled_ = true;
    // We need to disable popup blocking to ensure that the RenderView
    // instance for the popup actually closes.
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

TEST_F(AutomationProxyTest5, TestLifetimeOfDomAutomationController) {
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_refptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("dom_automation_test_with_popup.html");

  tab->NavigateToURL(net::FilePathToFileURL(filename));

  // Allow some time for the popup to show up and close.
  PlatformThread::Sleep(2000);

  std::wstring expected(L"string");
  std::wstring jscript = CreateJSString(L"\"" + expected + L"\"");
  std::wstring actual;
  ASSERT_TRUE(tab->ExecuteAndExtractString(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}
