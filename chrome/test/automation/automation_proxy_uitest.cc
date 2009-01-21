// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/constrained_window_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/event.h"
#include "net/base/net_util.h"

class AutomationProxyTest : public UITest {
 protected:
  AutomationProxyTest() {
    dom_automation_enabled_ = true;
    launch_arguments_.AppendSwitchWithValue(switches::kLang,
                                            L"en-us");
  }
};

class AutomationProxyVisibleTest : public UITest {
 protected:
  AutomationProxyVisibleTest() {
    show_window_ = true;
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
    scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(window.get());
  }

  {
    scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(-1));
    ASSERT_FALSE(window.get());
  }

  {
    scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(1));
    ASSERT_FALSE(window.get());
  }
};

TEST_F(AutomationProxyVisibleTest, WindowGetViewBounds) {
  {
    scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());
    scoped_ptr<WindowProxy> window(
      automation()->GetWindowForBrowser(browser.get()));
    ASSERT_TRUE(window.get());

    scoped_ptr<TabProxy> tab1(browser->GetTab(0));
    ASSERT_TRUE(tab1.get());
    GURL tab1_url;
    ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

    // Add another tab so we can simulate dragging.
    ASSERT_TRUE(browser->AppendTab(GURL("about:")));

    scoped_ptr<TabProxy> tab2(browser->GetTab(1));
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
    EXPECT_GT(bounds2.x(), bounds.x());
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
    tab1.reset(browser->GetTab(0));
    ASSERT_TRUE(tab1.get());
    GURL tab1_new_url;
    ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

    tab2.reset(browser->GetTab(1));
    ASSERT_TRUE(tab2.get());
    GURL tab2_new_url;
    ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

    EXPECT_EQ(tab1_url.spec(), tab2_new_url.spec());
    EXPECT_EQ(tab2_url.spec(), tab1_new_url.spec());

    */
  }
}

TEST_F(AutomationProxyTest, GetTabCount) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int tab_count = 0;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);
}

TEST_F(AutomationProxyTest, GetActiveTabIndex) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  int active_tab_index = -1;
  ASSERT_TRUE(window->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);
}

TEST_F(AutomationProxyVisibleTest, AppendTab) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
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

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"title2.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(filename)));

  int appended_tab_index;
  // Append tab will also be active tab
  ASSERT_TRUE(window->GetActiveTabIndex(&appended_tab_index));

  scoped_ptr<TabProxy> tab(window->GetTab(appended_tab_index));
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, ActivateTab) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
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
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  {
    scoped_ptr<TabProxy> tab(window->GetTab(0));
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
    scoped_ptr<TabProxy> tab;
    tab.reset(window->GetTab(1));
    ASSERT_FALSE(tab.get());
  }
};

TEST_F(AutomationProxyTest, NavigateToURL) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_ptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  // BUG [634097] : expected title should be "about:blank"
  ASSERT_STREQ(L"", title.c_str());

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"title2.html");

  tab->NavigateToURL(net::FilePathToFileURL(filename));
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  // TODO(vibhor) : Add a test using testserver.
}

// This test is disabled. See bug 794412
TEST_F(AutomationProxyTest, DISABLED_NavigateToURLWithTimeout1) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_ptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"title2.html");

  bool is_timeout;
  tab->NavigateToURLWithTimeout(net::FilePathToFileURL(filename),
                                10000, &is_timeout);
  ASSERT_FALSE(is_timeout);

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());

  tab->NavigateToURLWithTimeout(net::FilePathToFileURL(filename),
                                1, &is_timeout);
  ASSERT_TRUE(is_timeout);

  Sleep(10);
}

// This test is disabled. See bug 794412.
TEST_F(AutomationProxyTest, DISABLED_NavigateToURLWithTimeout2) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_ptr<TabProxy> tab(window->GetTab(0));
  tab.reset(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring filename1(test_data_directory_);
  file_util::AppendToPath(&filename1, L"title1.html");

  bool is_timeout;
  tab->NavigateToURLWithTimeout(net::FilePathToFileURL(filename1),
                                1, &is_timeout);
  ASSERT_TRUE(is_timeout);

  std::wstring filename2(test_data_directory_);
  file_util::AppendToPath(&filename2, L"title2.html");
  tab->NavigateToURLWithTimeout(net::FilePathToFileURL(filename2),
                                10000, &is_timeout);
  ASSERT_FALSE(is_timeout);

  Sleep(10);
}

TEST_F(AutomationProxyTest, GoBackForward) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_ptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  // BUG [634097] : expected title should be "about:blank"
  ASSERT_STREQ(L"", title.c_str());

  ASSERT_FALSE(tab->GoBack());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"", title.c_str());

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"title2.html");
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
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_ptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());
  GURL url;
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  ASSERT_STREQ("about:blank", url.spec().c_str());

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"cookie1.html");
  GURL newurl = net::FilePathToFileURL(filename);
  ASSERT_TRUE(tab->NavigateToURL(newurl));
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  // compare canonical urls...
  ASSERT_STREQ(newurl.spec().c_str(), url.spec().c_str());
}

class AutomationProxyTest2 : public AutomationProxyVisibleTest {
 protected:
  AutomationProxyTest2() {
    document1_ = test_data_directory_;
    file_util::AppendToPath(&document1_, L"title1.html");

    document2_ = test_data_directory_;
    file_util::AppendToPath(&document2_, L"title2.html");
    launch_arguments_ = CommandLine(L"");
    launch_arguments_.AppendLooseValue(document1_);
    launch_arguments_.AppendLooseValue(document2_);
  }

  std::wstring document1_;
  std::wstring document2_;
};

TEST_F(AutomationProxyTest2, GetActiveTabIndex) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
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
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_ptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"title1.html", title.c_str());

  tab.reset(window->GetTab(1));
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_STREQ(L"Title Of Awesomeness", title.c_str());
}

TEST_F(AutomationProxyTest, Cookies) {
  GURL url(L"http://mojo.jojo.google.com");
  std::string value_result;

  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_ptr<TabProxy> tab(window->GetTab(0));
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

TEST_F(AutomationProxyTest, GetHWND) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_ptr<WindowProxy> window(
      automation()->GetWindowForBrowser(browser.get()));
  ASSERT_TRUE(window.get());

  HWND handle;
  ASSERT_TRUE(window->GetHWND(&handle));
  ASSERT_TRUE(handle);
}

TEST_F(AutomationProxyTest, NavigateToURLAsync) {
  AutomationProxy* automation_object = automation();
  scoped_ptr<BrowserProxy> window(automation_object->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_ptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"cookie1.html");
  GURL newurl = net::FilePathToFileURL(filename);

  ASSERT_TRUE(tab->NavigateToURLAsync(newurl));
  std::string value = WaitUntilCookieNonEmpty(tab.get(), newurl, "foo", 250,
                                              action_max_timeout_ms());
  ASSERT_STREQ("baz", value.c_str());
}

TEST_F(AutomationProxyTest, AcceleratorNewTab) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));

  int old_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&old_tab_count));

  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  int new_tab_count;
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
                                              5000));
  if (new_tab_count == -1)
    FAIL();
  ASSERT_EQ(old_tab_count + 1, new_tab_count);
  scoped_ptr<TabProxy> tab(window->GetTab(new_tab_count - 1));
  ASSERT_TRUE(tab.get());

  std::wstring title;
  int i;
  for (i = 0; i < 10; ++i) {
    Sleep(sleep_timeout_ms());
    ASSERT_TRUE(tab->GetTabTitle(&title));
    if (title == L"Destinations" || title == L"New Tab")
      break;
  }
  // If we got to 10, the new tab failed to open.
  ASSERT_NE(10, i);
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
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_ptr<TabProxy> tab(window->GetTab(0));
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
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_ptr<TabProxy> tab(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  bool expected = true;
  std::wstring jscript = CreateJSString(BooleanToString(expected));
  bool actual = false;
  ASSERT_TRUE(tab->ExecuteAndExtractBool(L"", jscript, &actual));
  ASSERT_EQ(expected, actual);
}

TEST_F(AutomationProxyTest4, NumberValueIsEchoedByDomAutomationController) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_ptr<TabProxy> tab(window->GetTab(0));
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
    file_util::AppendToPath(&document1_, L"frame_dom_access");
    file_util::AppendToPath(&document1_, L"frame_dom_access.html");

    dom_automation_enabled_ = true;
    launch_arguments_ = CommandLine(L"");
    launch_arguments_.AppendLooseValue(document1_);
  }

  std::wstring document1_;
};

std::wstring CreateJSStringForDOMQuery(const std::wstring& id) {
  std::wstring jscript(L"window.domAutomationController");
  StringAppendF(&jscript, L".send(document.getElementById('%ls').nodeName);",
                id.c_str());
  return jscript;
}

TEST_F(AutomationProxyTest3, FrameDocumentCanBeAccessed) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_ptr<TabProxy> tab(window->GetTab(0));
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
  window->ApplyAccelerator(IDC_NEWTAB);
  tab.reset(window->GetTab(1));
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(window->ActivateTab(1));

  std::wstring title;
  int i;
  for (i = 0; i < 10; ++i) {
    Sleep(sleep_timeout_ms());
    ASSERT_TRUE(tab->GetTabTitle(&title));
    if (title == L"Destinations")
      break;
  }
  // If we got to 10, the new tab failed to open.
  ASSERT_NE(10, i);
  ASSERT_FALSE(tab->ExecuteAndExtractString(xpath1, jscript1, &actual));
#endif
}

TEST_F(AutomationProxyTest, DISABLED_ConstrainedWindowTest) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_ptr<TabProxy> tab(window->GetTab(0));
  tab.reset(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"constrained_files");
  file_util::AppendToPath(&filename, L"constrained_window.html");

  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));

  int count;
  ASSERT_TRUE(tab->WaitForChildWindowCountToChange(0, &count, 5000));

  ASSERT_EQ(2, count);

  ConstrainedWindowProxy* cwindow = tab->GetConstrainedWindow(0);
  ASSERT_TRUE(cwindow);

  std::wstring title;
  ASSERT_TRUE(cwindow->GetTitle(&title));
#if defined(GOOGLE_CHROME_BUILD)
  std::wstring app_name = L"Google Chrome";
#else
  std::wstring app_name = L"Chromium";
#endif
  std::wstring window_title = L"Constrained Window 0 - " + app_name;
  ASSERT_STREQ(window_title.c_str(), title.c_str());
  delete cwindow;

  cwindow = tab->GetConstrainedWindow(1);
  ASSERT_TRUE(cwindow);
  ASSERT_TRUE(cwindow->GetTitle(&title));
  window_title = L"Constrained Window 1 - " + app_name;
  ASSERT_STREQ(window_title.c_str(), title.c_str());
  delete cwindow;
}

TEST_F(AutomationProxyTest, CantEscapeByOnloadMoveto) {
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  scoped_ptr<TabProxy> tab(window->GetTab(0));
  tab.reset(window->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"constrained_files");
  file_util::AppendToPath(&filename, L"constrained_window_onload_moveto.html");

  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));

  int count;
  ASSERT_TRUE(tab->WaitForChildWindowCountToChange(0, &count, 5000));

  ASSERT_EQ(1, count);

  ConstrainedWindowProxy* cwindow = tab->GetConstrainedWindow(0);
  ASSERT_TRUE(cwindow);

  gfx::Rect rect;
  bool is_timeout = false;
  ASSERT_TRUE(cwindow->GetBoundsWithTimeout(&rect, 1000, &is_timeout));
  ASSERT_FALSE(is_timeout);
  ASSERT_NE(20, rect.x());
  ASSERT_NE(20, rect.y());
}

bool ExternalTabHandler(HWND external_tab_window) {
  static const wchar_t class_name[] = L"External_Tab_UI_Test_Class";
  static const wchar_t window_title[] = L"External Tab Tester";

  WNDCLASSEX wnd_class = {0};
  wnd_class.cbSize = sizeof(wnd_class);
  wnd_class.style = CS_HREDRAW | CS_VREDRAW;
  wnd_class.lpfnWndProc = DefWindowProc;
  wnd_class.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wnd_class.lpszClassName = class_name;
  ATOM result = RegisterClassEx(&wnd_class);
  if (0 == result) {
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
  Sleep(1000);
  DestroyWindow(external_tab_ui_parent);
  return true;
}

TEST_F(AutomationProxyVisibleTest, CreateExternalTab) {
  HWND external_tab_container = NULL;
  scoped_ptr<TabProxy> tab(automation()->CreateExternalTab(
      &external_tab_container));
  EXPECT_TRUE(tab != NULL);
  EXPECT_NE(FALSE, ::IsWindow(external_tab_container));
  if (tab != NULL) {
    tab->NavigateInExternalTab(GURL(L"http://www.google.com"));
    EXPECT_EQ(true, ExternalTabHandler(external_tab_container));
    // Since the tab goes away lazily, wait a bit
    Sleep(1000);
    EXPECT_FALSE(tab->is_valid());
  }
}

TEST_F(AutomationProxyTest, AutocompleteGetSetText) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_ptr<AutocompleteEditProxy> edit(
      automation()->GetAutocompleteEditForBrowser(browser.get()));
  ASSERT_TRUE(edit.get());
  EXPECT_TRUE(edit->is_valid());
  const std::wstring text_to_set = L"Lollerskates";
  std::wstring actual_text;
  EXPECT_TRUE(edit->SetText(text_to_set));
  EXPECT_TRUE(edit->GetText(&actual_text));
  EXPECT_EQ(text_to_set, actual_text);
  scoped_ptr<AutocompleteEditProxy> edit2(
      automation()->GetAutocompleteEditForBrowser(browser.get()));
  EXPECT_TRUE(edit2->GetText(&actual_text));
  EXPECT_EQ(text_to_set, actual_text);
}

TEST_F(AutomationProxyTest, AutocompleteParallelProxy) {
  scoped_ptr<BrowserProxy> browser1(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser1.get());
  scoped_ptr<AutocompleteEditProxy> edit1(
      automation()->GetAutocompleteEditForBrowser(browser1.get()));
  ASSERT_TRUE(edit1.get());
  EXPECT_TRUE(browser1->ApplyAccelerator(IDC_NEW_WINDOW));
  scoped_ptr<BrowserProxy> browser2(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser2.get());
  scoped_ptr<AutocompleteEditProxy> edit2(
      automation()->GetAutocompleteEditForBrowser(browser2.get()));
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
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_ptr<AutocompleteEditProxy> edit(
      automation()->GetAutocompleteEditForBrowser(browser.get()));
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

// Disabled because flacky see bug #5314.
TEST_F(AutomationProxyTest, DISABLED_AppModalDialogTest) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_ptr<TabProxy> tab(browser->GetTab(0));
  tab.reset(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  bool modal_dialog_showing = false;
  views::DialogDelegate::DialogButton button =
      views::DialogDelegate::DIALOGBUTTON_NONE;
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  EXPECT_EQ(views::DialogDelegate::DIALOGBUTTON_NONE, button);

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
  EXPECT_EQ(views::DialogDelegate::DIALOGBUTTON_OK, button);

  // Test that clicking missing button fails graciously and does not close the
  // dialog.
  EXPECT_FALSE(automation()->ClickAppModalDialogButton(
      views::DialogDelegate::DIALOGBUTTON_CANCEL));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_TRUE(modal_dialog_showing);

  // Now click OK, that should close the dialog.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      views::DialogDelegate::DIALOGBUTTON_OK));
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
  EXPECT_EQ(views::DialogDelegate::DIALOGBUTTON_OK |
            views::DialogDelegate::DIALOGBUTTON_CANCEL, button);

  // Click OK.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      views::DialogDelegate::DIALOGBUTTON_OK));
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
  EXPECT_EQ(views::DialogDelegate::DIALOGBUTTON_OK |
            views::DialogDelegate::DIALOGBUTTON_CANCEL, button);

  // Click Cancel this time.
  EXPECT_TRUE(automation()->ClickAppModalDialogButton(
      views::DialogDelegate::DIALOGBUTTON_CANCEL));
  EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
                                                     &button));
  EXPECT_FALSE(modal_dialog_showing);
  EXPECT_TRUE(tab->ExecuteAndExtractInt(
      L"", L"window.domAutomationController.send(result);", &result));
  EXPECT_EQ(1, result);
}