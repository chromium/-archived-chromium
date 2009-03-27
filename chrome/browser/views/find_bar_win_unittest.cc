// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/find_notification_details.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/web_contents_view.h"
#include "chrome/browser/views/find_bar_win.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/host_resolver_unittest.h"

const std::wstring kFramePage = L"files/find_in_page/frames.html";
const std::wstring kFrameData = L"files/find_in_page/framedata_general.html";
const std::wstring kUserSelectPage = L"files/find_in_page/user-select.html";
const std::wstring kCrashPage = L"files/find_in_page/crash_1341577.html";
const std::wstring kTooFewMatchesPage = L"files/find_in_page/bug_1155639.html";

class FindInPageNotificationObserver : public NotificationObserver {
 public:
  explicit FindInPageNotificationObserver(TabContents* parent_tab)
      : parent_tab_(parent_tab),
        active_match_ordinal_(-1),
        number_of_matches_(0) {
    registrar_.Add(this, NotificationType::FIND_RESULT_AVAILABLE,
        Source<TabContents>(parent_tab_));
    ui_test_utils::RunMessageLoop();
  }

  int active_match_ordinal() const { return active_match_ordinal_; }

  int number_of_matches() const { return number_of_matches_; }

  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::FIND_RESULT_AVAILABLE) {
      Details<FindNotificationDetails> find_details(details);
      if (find_details->request_id() == kFindInPageRequestId) {
        // We get multiple responses and one of those will contain the ordinal.
        // This message comes to us before the final update is sent.
        if (find_details->active_match_ordinal() > -1)
          active_match_ordinal_ = find_details->active_match_ordinal();
        if (find_details->final_update()) {
          number_of_matches_ = find_details->number_of_matches();
          MessageLoopForUI::current()->Quit();
        } else {
          DLOG(INFO) << "Ignoring, since we only care about the final message";
        }
      }
    } else {
      NOTREACHED();
    }
  }

  // The Find mechanism is over asynchronous IPC, so a search is kicked off and
  // we wait for notification to find out what the results are. As the user is
  // typing, new search requests can be issued and the Request ID helps us make
  // sense of whether this is the current request or an old one. The unit tests,
  // however, which uses this constant issues only one search at a time, so we
  // don't need a rolling id to identify each search. But, we still need to
  // specify one, so we just use a fixed one - its value does not matter.
  static const int kFindInPageRequestId;

 private:
  NotificationRegistrar registrar_;
  TabContents* parent_tab_;
  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_;
  int number_of_matches_;
};

typedef enum FindInPageDirection { BACK = 0, FWD = 1 };
typedef enum FindInPageCase { IGNORE_CASE = 0, CASE_SENSITIVE = 1 };

class FindInPageControllerTest : public InProcessBrowserTest {
 public:
  FindInPageControllerTest() {
    host_mapper_ = new net::RuleBasedHostMapper();
    // Avoid making external DNS lookups. In this test we don't need this
    // to succeed.
    host_mapper_->AddSimulatedFailure("*.google.com");
    scoped_host_mapper_.Init(host_mapper_.get());
  }

 protected:
  int FindInPage(const std::wstring& search_string,
                 FindInPageDirection forward,
                 FindInPageCase match_case,
                 bool find_next) {
    WebContents* web_contents =
        browser()->GetSelectedTabContents()->AsWebContents();
    if (web_contents) {
      web_contents->set_current_find_request_id(
          FindInPageNotificationObserver::kFindInPageRequestId);
      web_contents->render_view_host()->StartFinding(
          FindInPageNotificationObserver::kFindInPageRequestId,
          search_string, forward == FWD, match_case == CASE_SENSITIVE,
          find_next);
      return FindInPageNotificationObserver(web_contents).number_of_matches();
    }
    return 0;
  }

 private:
  scoped_refptr<net::RuleBasedHostMapper> host_mapper_;
  net::ScopedHostMapper scoped_host_mapper_;
};

// This test loads a page with frames and starts FindInPage requests
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageFrames) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our frames page.
  GURL url = server->TestServerPageW(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Try incremental search (mimicking user typing in).
  EXPECT_EQ(18, FindInPage(L"g",       FWD, IGNORE_CASE, false));
  EXPECT_EQ(11, FindInPage(L"go",      FWD, IGNORE_CASE, false));
  EXPECT_EQ(04, FindInPage(L"goo",     FWD, IGNORE_CASE, false));
  EXPECT_EQ(03, FindInPage(L"goog",    FWD, IGNORE_CASE, false));
  EXPECT_EQ(02, FindInPage(L"googl",   FWD, IGNORE_CASE, false));
  EXPECT_EQ(01, FindInPage(L"google",  FWD, IGNORE_CASE, false));
  EXPECT_EQ(00, FindInPage(L"google!", FWD, IGNORE_CASE, false));

  // Negative test (no matches should be found).
  EXPECT_EQ(0, FindInPage(L"Non-existing string", FWD, IGNORE_CASE, false));

  // 'horse' only exists in the three right frames.
  EXPECT_EQ(3, FindInPage(L"horse", FWD, IGNORE_CASE, false));

  // 'cat' only exists in the first frame.
  EXPECT_EQ(1, FindInPage(L"cat", FWD, IGNORE_CASE, false));

  // Try searching again, should still come up with 1 match.
  EXPECT_EQ(1, FindInPage(L"cat", FWD, IGNORE_CASE, false));

  // Try searching backwards, ignoring case, should still come up with 1 match.
  EXPECT_EQ(1, FindInPage(L"CAT", BACK, IGNORE_CASE, false));

  // Try case sensitive, should NOT find it.
  EXPECT_EQ(0, FindInPage(L"CAT", FWD, CASE_SENSITIVE, false));

  // Try again case sensitive, but this time with right case.
  EXPECT_EQ(1, FindInPage(L"dog", FWD, CASE_SENSITIVE, false));

  // Try non-Latin characters ('Hreggvidur' with 'eth' for 'd' in left frame).
  EXPECT_EQ(1, FindInPage(L"Hreggvi\u00F0ur", FWD, IGNORE_CASE, false));
  EXPECT_EQ(1, FindInPage(L"Hreggvi\u00F0ur", FWD, CASE_SENSITIVE, false));
  EXPECT_EQ(0, FindInPage(L"hreggvi\u00F0ur", FWD, CASE_SENSITIVE, false));
}
