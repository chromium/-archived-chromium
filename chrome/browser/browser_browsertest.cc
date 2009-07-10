// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/l10n_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Given a page title, returns the expected window caption string.
std::wstring WindowCaptionFromPageTitle(std::wstring page_title) {
#if defined(OS_WIN) || defined(OS_LINUX)
  if (page_title.empty())
    return l10n_util::GetString(IDS_PRODUCT_NAME);

  return l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT, page_title);
#elif defined(OS_MACOSX)
  // On Mac, we don't want to suffix the page title with the application name.
  if (page_title.empty())
    return l10n_util::GetString(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
  return page_title;
#endif
}

}  // namespace

class BrowserTest : public InProcessBrowserTest {
 protected:
  // In RTL locales wrap the page title with RTL embedding characters so that it
  // matches the value returned by GetWindowTitle().
  std::wstring LocaleWindowCaptionFromPageTitle(
      const std::wstring& expected_title) {
    std::wstring page_title = WindowCaptionFromPageTitle(expected_title);
#if defined(OS_WIN)
    std::string locale = g_browser_process->GetApplicationLocale();
    if (l10n_util::GetTextDirectionForLocale(locale.c_str()) ==
        l10n_util::RIGHT_TO_LEFT) {
      l10n_util::WrapStringWithLTRFormatting(&page_title);
    }

    return page_title;
#else
    // Do we need to use the above code on POSIX as well?
    return page_title;
#endif
  }
};

// Launch the app on a page with no title, check that the app title was set
// correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, NoTitle) {
  ui_test_utils::NavigateToURL(browser(),
                               ui_test_utils::GetTestUrl(L".", L"title1.html"));
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(L"title1.html"),
            UTF16ToWideHack(browser()->GetCurrentPageTitle()));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(ASCIIToUTF16("title1.html"), tab_title);
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, Title) {
  ui_test_utils::NavigateToURL(browser(),
                               ui_test_utils::GetTestUrl(L".", L"title2.html"));
  const std::wstring test_title(L"Title Of Awesomeness");
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(test_title),
            UTF16ToWideHack(browser()->GetCurrentPageTitle()));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(WideToUTF16(test_title), tab_title);
}
