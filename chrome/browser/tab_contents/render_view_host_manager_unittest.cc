// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test_render_view_host.h"
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
