// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test_render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

class RenderViewHostManagerTest : public RenderViewHostTestHarness {
 public:
  void NavigateActiveAndCommit(const GURL& url) {
    // Note: we navigate the active RenderViewHost because previous navigations
    // won't have committed yet, so NavigateAndCommit does the wrong thing
    // for us.
    controller().LoadURL(url, GURL(), 0);
    active_rvh()->SendNavigate(
        static_cast<MockRenderProcessHost*>(active_rvh()->process())->
            max_page_id() + 1,
        url);
  }
};

// Tests that when you navigate from the New TabPage to another page, and
// then do that same thing in another tab, that the two resulting pages have
// different SiteInstances, BrowsingInstances, and RenderProcessHosts. This is
// a regression test for bug 9364.
TEST_F(RenderViewHostManagerTest, NewTabPageProcesses) {
  GURL ntp(chrome::kChromeUINewTabURL);
  GURL dest("http://www.google.com/");

  // Navigate our first tab to the new tab page and then to the destination.
  NavigateActiveAndCommit(ntp);
  NavigateActiveAndCommit(dest);

  // Make a second tab.
  TestTabContents contents2(profile_.get(), NULL);

  // Load the two URLs in the second tab. Note that the first navigation creates
  // a RVH that's not pending (since there is no cross-site transition), so
  // we use the committed one, but the second one is the opposite.
  contents2.controller().LoadURL(ntp, GURL(), PageTransition::LINK);
  static_cast<TestRenderViewHost*>(contents2.render_manager()->
     current_host())->SendNavigate(100, ntp);
  contents2.controller().LoadURL(dest, GURL(), PageTransition::LINK);
  static_cast<TestRenderViewHost*>(contents2.render_manager()->
      pending_render_view_host())->SendNavigate(101, dest);

  // The two RVH's should be different in every way.
  EXPECT_NE(active_rvh()->process(), contents2.render_view_host()->process());
  EXPECT_NE(active_rvh()->site_instance(),
      contents2.render_view_host()->site_instance());
  EXPECT_NE(active_rvh()->site_instance()->browsing_instance(),
      contents2.render_view_host()->site_instance()->browsing_instance());

  // Navigate both to the new tab page, and verify that they share a
  // SiteInstance.
  NavigateActiveAndCommit(ntp);

  contents2.controller().LoadURL(ntp, GURL(), PageTransition::LINK);
  static_cast<TestRenderViewHost*>(contents2.render_manager()->
     pending_render_view_host())->SendNavigate(102, ntp);

  EXPECT_EQ(active_rvh()->site_instance(),
      contents2.render_view_host()->site_instance());
}

// When there is an error with the specified page, renderer exits view-source
// mode. See WebFrameImpl::DidFail(). We check by this test that
// EnableViewSourceMode message is sent on every navigation regardless
// RenderView is being newly created or reused.
TEST_F(RenderViewHostManagerTest, AlwaysSendEnableViewSourceMode) {
  const GURL kNtpUrl(chrome::kChromeUINewTabURL);
  const GURL kUrl("view-source:http://foo");

  // We have to navigate to some page at first since without this, the first
  // navigation will reuse the SiteInstance created by Init(), and the second
  // one will create a new SiteInstance. Because current_instance and
  // new_instance will be different, a new RenderViewHost will be created for
  // the second navigation. We have to avoid this in order to exercise the
  // target code patch.
  NavigateActiveAndCommit(kNtpUrl);

  // Navigate.
  controller().LoadURL(kUrl, GURL() /* referer */, PageTransition::TYPED);
  // Simulate response from RenderView for FirePageBeforeUnload.
  rvh()->TestOnMessageReceived(
      ViewHostMsg_ShouldClose_ACK(rvh()->routing_id(), true));
  ASSERT_TRUE(pending_rvh());  // New pending RenderViewHost will be created.
  RenderViewHost* last_rvh = pending_rvh();
  int new_id = static_cast<MockRenderProcessHost*>(pending_rvh()->process())->
               max_page_id() + 1;
  pending_rvh()->SendNavigate(new_id, kUrl);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  ASSERT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(kUrl == controller().GetLastCommittedEntry()->url());
  EXPECT_FALSE(controller().pending_entry());
  // Because we're using TestTabContents and TestRenderViewHost in this
  // unittest, no one calls TabContents::RenderViewCreated(). So, we see no
  // EnableViewSourceMode message, here.

  // Clear queued messages before load.
  process()->sink().ClearMessages();
  // Navigate, again.
  controller().LoadURL(kUrl, GURL() /* referer */, PageTransition::TYPED);
  // The same RenderViewHost should be reused.
  EXPECT_FALSE(pending_rvh());
  EXPECT_TRUE(last_rvh == rvh());
  rvh()->SendNavigate(new_id, kUrl);  // The same page_id returned.
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_FALSE(controller().pending_entry());
  // New message should be sent out to make sure to enter view-source mode.
  EXPECT_TRUE(process()->sink().GetUniqueMessageMatching(
      ViewMsg_EnableViewSourceMode::ID));
}
