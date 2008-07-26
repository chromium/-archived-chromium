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
