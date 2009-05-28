// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

using std::wstring;

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

class SessionHistoryTest : public UITest {
 protected:
  SessionHistoryTest() : UITest() {
    FilePath path(test_data_directory_);
    path = path.AppendASCII("session_history");

    url_prefix_ = UTF8ToWide(net::FilePathToFileURL(path).spec());
  }

  virtual void SetUp() {
    UITest::SetUp();

    window_ = automation()->GetBrowserWindow(0);
    ASSERT_TRUE(window_.get());

    int active_tab_index = -1;
    ASSERT_TRUE(window_->GetActiveTabIndex(&active_tab_index));
    tab_ = window_->GetTab(active_tab_index);
    ASSERT_TRUE(tab_.get());
  }

  // Simulate clicking a link.  Only works on the frames.html testserver page.
  void ClickLink(std::string node_id) {
    GURL url("javascript:clickLink('" + node_id + "')");
    ASSERT_TRUE(tab_->NavigateToURL(url));
  }

  // Simulate filling in form data.  Only works on the frames.html page with
  // subframe = form.html, and on form.html itself.
  void FillForm(std::string node_id, std::string value) {
    GURL url("javascript:fillForm('" + node_id + "', '" + value + "')");
    // This will return immediately, but since the JS executes synchronously
    // on the renderer, it will complete before the next navigate message is
    // processed.
    ASSERT_TRUE(tab_->NavigateToURLAsync(url));
  }

  // Simulate submitting a form.  Only works on the frames.html page with
  // subframe = form.html, and on form.html itself.
  void SubmitForm(std::string node_id) {
    GURL url("javascript:submitForm('" + node_id + "')");
    ASSERT_TRUE(tab_->NavigateToURL(url));
  }

  // Navigate session history using history.go(distance).
  void JavascriptGo(std::string distance) {
    GURL url("javascript:history.go('" + distance + "')");
    ASSERT_TRUE(tab_->NavigateToURL(url));
  }

  wstring GetTabTitle() {
    wstring title;
    EXPECT_TRUE(tab_->GetTabTitle(&title));
    return title;
  }

  // Try 10 times to get the right tab title.
  wstring TestTabTitle(const wstring& value) {
    // Error pages load separately, but the UI automation system does not wait
    // for error pages to load before returning after a navigation request.
    // So, we need to sleep a little.
    const int kWaitForErrorPageMsec = 200;

    for (int i = 0; i < 10; ++i) {
      if (value.compare(GetTabTitle()) == 0)
        return value;
      PlatformThread::Sleep(kWaitForErrorPageMsec);
    }
    return GetTabTitle();
  }

  GURL GetTabURL() {
    GURL url;
    EXPECT_TRUE(tab_->GetCurrentURL(&url));
    return url;
  }

 protected:
  wstring url_prefix_;
  scoped_refptr<BrowserProxy> window_;
  scoped_refptr<TabProxy> tab_;
};

TEST_F(SessionHistoryTest, BasicBackForward) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      server->TestServerPage("files/session_history/bot1.html")));
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      server->TestServerPage("files/session_history/bot2.html")));
  EXPECT_EQ(L"bot2", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      server->TestServerPage("files/session_history/bot3.html")));
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, bot2, *bot3]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot2", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot2", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      server->TestServerPage("files/session_history/bot3.html")));
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, *bot3]

  ASSERT_FALSE(tab_->GoForward());
  EXPECT_EQ(L"bot3", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot3", GetTabTitle());
}

// Test that back/forward works when navigating in subframes.
TEST_F(SessionHistoryTest, FrameBackForward) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // about:blank should be loaded first.
  GURL home(WideToUTF8(homepage_));
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());
  EXPECT_EQ(home, GetTabURL());

  GURL frames(server->TestServerPage("files/session_history/frames.html"));
  ASSERT_TRUE(tab_->NavigateToURL(frames));
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot2");
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot3");
  EXPECT_EQ(L"bot3", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, bot2, *bot3]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());
  EXPECT_EQ(home, GetTabURL());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ClickLink("abot1");
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, bot2, *bot1]

  ASSERT_FALSE(tab_->GoForward());
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"bot1", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

// Test that back/forward preserves POST data and document state in subframes.
TEST_F(SessionHistoryTest, FrameFormBackForward) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  GURL frames(server->TestServerPage("files/session_history/frames.html"));
  ASSERT_TRUE(tab_->NavigateToURL(frames));
  EXPECT_EQ(L"bot1", GetTabTitle());

  ClickLink("aform");
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ(L"text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, *form, post]

  ClickLink("abot2");
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, form, *bot2]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ(L"text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, form, *post]

  if (false) {
    // TODO(mpcomplete): reenable this when WebKit bug 10199 is fixed:
    // "returning to a POST result within a frame does a GET instead of a POST"
    ClickLink("abot2");
    EXPECT_EQ(L"bot2", GetTabTitle());
    EXPECT_EQ(frames, GetTabURL());

    ASSERT_TRUE(tab_->GoBack());
    EXPECT_EQ(L"text=&select=a", GetTabTitle());
    EXPECT_EQ(frames, GetTabURL());
  }
}

// TODO(mpcomplete): enable this when Bug 734372 is fixed:
// "Doing a session history navigation does not restore newly-created subframe
// document state"
// Test that back/forward preserves POST data and document state when navigating
// across frames (ie, from frame -> nonframe).
TEST_F(SessionHistoryTest, DISABLED_CrossFrameFormBackForward) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  GURL frames(server->TestServerPage("files/session_history/frames.html"));
  ASSERT_TRUE(tab_->NavigateToURL(frames));
  EXPECT_EQ(L"bot1", GetTabTitle());

  ClickLink("aform");
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ(L"text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  // history is [blank, bot1, *form, post]

  // ClickLink(L"abot2");
  GURL bot2("files/session_history/bot2.html");
  ASSERT_TRUE(tab_->NavigateToURL(bot2));
  EXPECT_EQ(L"bot2", GetTabTitle());
  EXPECT_EQ(bot2, GetTabURL());

  // history is [blank, bot1, form, *bot2]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(L"form", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());

  SubmitForm("isubmit");
  EXPECT_EQ(L"text=&select=a", GetTabTitle());
  EXPECT_EQ(frames, GetTabURL());
}

// Test that back/forward entries are created for reference fragment
// navigations. Bug 730379.
TEST_F(SessionHistoryTest, FragmentBackForward) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  GURL fragment(server->TestServerPage("files/session_history/fragment.html"));
  ASSERT_TRUE(tab_->NavigateToURL(fragment));
  EXPECT_EQ(L"fragment", GetTabTitle());
  EXPECT_EQ(fragment, GetTabURL());

  GURL::Replacements ref_params;

  ref_params.SetRef("a", url_parse::Component(0, 1));
  GURL fragment_a(fragment.ReplaceComponents(ref_params));
  ASSERT_TRUE(tab_->NavigateToURL(fragment_a));
  EXPECT_EQ(L"fragment", GetTabTitle());
  EXPECT_EQ(fragment_a, GetTabURL());

  ref_params.SetRef("b", url_parse::Component(0, 1));
  GURL fragment_b(fragment.ReplaceComponents(ref_params));
  ASSERT_TRUE(tab_->NavigateToURL(fragment_b));
  EXPECT_EQ(L"fragment", GetTabTitle());
  EXPECT_EQ(fragment_b, GetTabURL());

  ref_params.SetRef("c", url_parse::Component(0, 1));
  GURL fragment_c(fragment.ReplaceComponents(ref_params));
  ASSERT_TRUE(tab_->NavigateToURL(fragment_c));
  EXPECT_EQ(L"fragment", GetTabTitle());
  EXPECT_EQ(fragment_c, GetTabURL());

  // history is [blank, fragment, fragment#a, fragment#b, *fragment#c]

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment_b, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment_a, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment, GetTabURL());

  ASSERT_TRUE(tab_->GoForward());
  EXPECT_EQ(fragment_a, GetTabURL());

  GURL bot3(server->TestServerPage("files/session_history/bot3.html"));
  ASSERT_TRUE(tab_->NavigateToURL(bot3));
  EXPECT_EQ(L"bot3", GetTabTitle());
  EXPECT_EQ(bot3, GetTabURL());

  // history is [blank, fragment, fragment#a, bot3]

  ASSERT_FALSE(tab_->GoForward());
  EXPECT_EQ(bot3, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment_a, GetTabURL());

  ASSERT_TRUE(tab_->GoBack());
  EXPECT_EQ(fragment, GetTabURL());
}

// Test that the javascript window.history object works.
// NOTE: history.go(N) does not do anything if N is outside the bounds of the
// back/forward list (such as trigger our start/stop loading events).  This
// means the test will hang if it attempts to navigate too far forward or back,
// since we'll be waiting forever for a load stop event.
TEST_F(SessionHistoryTest, JavascriptHistory) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // about:blank should be loaded first.
  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      server->TestServerPage("files/session_history/bot1.html")));
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      server->TestServerPage("files/session_history/bot2.html")));
  EXPECT_EQ(L"bot2", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      server->TestServerPage("files/session_history/bot3.html")));
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, bot2, *bot3]

  JavascriptGo("-1");
  EXPECT_EQ(L"bot2", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ(L"bot2", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  JavascriptGo("2");
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, bot2, *bot3]

  JavascriptGo("-3");
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  ASSERT_TRUE(tab_->NavigateToURL(
      server->TestServerPage("files/session_history/bot3.html")));
  EXPECT_EQ(L"bot3", GetTabTitle());

  // history is [blank, bot1, *bot3]

  ASSERT_FALSE(tab_->GoForward());
  EXPECT_EQ(L"bot3", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  JavascriptGo("-1");
  EXPECT_EQ(L"", GetTabTitle());

  ASSERT_FALSE(tab_->GoBack());
  EXPECT_EQ(L"", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ(L"bot1", GetTabTitle());

  JavascriptGo("1");
  EXPECT_EQ(L"bot3", GetTabTitle());

  // TODO(creis): Test that JavaScript history navigations work across tab
  // types.  For example, load about:network in a tab, then a real page, then
  // try to go back and forward with JavaScript.  Bug 1136715.
  // (Hard to test right now, because pages like about:network cause the
  // TabProxy to hang.  This is because they do not appear to use the
  // NotificationService.)
}

// This test is flaky and has been disabled. It looks like the server does not
// start fast enough, and the navigation fails (with 404). See bug 8444.
TEST_F(SessionHistoryTest, DISABLED_LocationReplace) {
  // Test that using location.replace doesn't leave the title of the old page
  // visible.
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  ASSERT_TRUE(tab_->NavigateToURL(server->TestServerPage(
      "files/session_history/replace.html?no-title.html")));
  EXPECT_EQ(L"", GetTabTitle());
}

}  // namespace
