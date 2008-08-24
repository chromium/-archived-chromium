// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/find_in_page_controller.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

class FindInPageControllerTest : public UITest {
 public:
  FindInPageControllerTest() {
    show_window_ = true;
  }
};

const std::wstring kFramePage = L"files/find_in_page/frames.html";
const std::wstring kUserSelectPage = L"files/find_in_page/user-select.html";

// This test loads a page with frames and starts FindInPage requests
TEST_F(FindInPageControllerTest, FindInPageFrames) {
  TestServer server(L"chrome/test/data");

  // First we navigate to our frames page.
  GURL url = server.TestServerPageW(kFramePage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));

  // Try incremental search (mimicking user typing in).
  EXPECT_EQ(18, tab->FindInPage(L"g",       FWD, IGNORE_CASE));
  EXPECT_EQ(11, tab->FindInPage(L"go",      FWD, IGNORE_CASE));
  EXPECT_EQ(04, tab->FindInPage(L"goo",     FWD, IGNORE_CASE));
  EXPECT_EQ(03, tab->FindInPage(L"goog",    FWD, IGNORE_CASE));
  EXPECT_EQ(02, tab->FindInPage(L"googl",   FWD, IGNORE_CASE));
  EXPECT_EQ(01, tab->FindInPage(L"google",  FWD, IGNORE_CASE));
  EXPECT_EQ(00, tab->FindInPage(L"google!", FWD, IGNORE_CASE));

  // Negative test (no matches should be found).
  EXPECT_EQ(0, tab->FindInPage(L"Non-existing string", FWD, IGNORE_CASE));

  // 'horse' only exists in the three right frames.
  EXPECT_EQ(3, tab->FindInPage(L"horse", FWD, IGNORE_CASE));

  // 'cat' only exists in the first frame.
  EXPECT_EQ(1, tab->FindInPage(L"cat", FWD, IGNORE_CASE));

  // Try searching again, should still come up with 1 match.
  EXPECT_EQ(1, tab->FindInPage(L"cat", FWD, IGNORE_CASE));

  // Try searching backwards, ignoring case, should still come up with 1 match.
  EXPECT_EQ(1, tab->FindInPage(L"CAT", BACK, IGNORE_CASE));

  // Try case sensitive, should NOT find it.
  EXPECT_EQ(0, tab->FindInPage(L"CAT", FWD, CASE_SENSITIVE));

  // Try again case sensitive, but this time with right case.
  EXPECT_EQ(1, tab->FindInPage(L"dog", FWD, CASE_SENSITIVE));

  // Try non-Latin characters ('Hreggvidur' with 'eth' for 'd' in left frame).
  EXPECT_EQ(1, tab->FindInPage(L"Hreggvi\u00F0ur", FWD, IGNORE_CASE));
  EXPECT_EQ(1, tab->FindInPage(L"Hreggvi\u00F0ur", FWD, CASE_SENSITIVE));
  EXPECT_EQ(0, tab->FindInPage(L"hreggvi\u00F0ur", FWD, CASE_SENSITIVE));
}

// Load a page with no selectable text and make sure we don't crash.
TEST_F(FindInPageControllerTest, FindUnSelectableText) {
  TestServer server(L"chrome/test/data");

  GURL url = server.TestServerPageW(kUserSelectPage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));

  EXPECT_EQ(0, tab->FindInPage(L"text",                FWD, IGNORE_CASE));
  EXPECT_EQ(0, tab->FindInPage(L"Non-existing string", FWD, IGNORE_CASE));
}

