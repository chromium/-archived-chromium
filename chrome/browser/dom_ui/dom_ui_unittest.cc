// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/renderer_host/test_render_view_host.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

class DOMUITest : public RenderViewHostTestHarness {
 public:
  DOMUITest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DOMUITest);
};

// Tests that the New Tab Page flags are correctly set and propogated by
// WebContents when we first navigate to a DOM UI page, then to a standard
// non-DOM-UI page.
TEST_F(DOMUITest, DOMUIToStandard) {
  // Start a pending load.
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  controller()->LoadURL(new_tab_url, GURL(), PageTransition::LINK);

  // The navigation entry should be pending with no committed entry.
  ASSERT_TRUE(controller()->GetPendingEntry());
  ASSERT_FALSE(controller()->GetLastCommittedEntry());

  // Check the things the pending DOM UI should have set.
  EXPECT_FALSE(contents()->ShouldDisplayURL());
  EXPECT_FALSE(contents()->ShouldDisplayFavIcon());
  EXPECT_TRUE(contents()->IsBookmarkBarAlwaysVisible());
  EXPECT_TRUE(contents()->FocusLocationBarByDefault());

  // Now commit the load.
  rvh()->SendNavigate(1, new_tab_url);

  // The same flags should be set as before now that the load has committed.
  // Note that the location bar isn't focused now. Once the load commits, we
  // don't care about this flag, so this value is OK.
  EXPECT_FALSE(contents()->ShouldDisplayURL());
  EXPECT_FALSE(contents()->ShouldDisplayFavIcon());
  EXPECT_TRUE(contents()->IsBookmarkBarAlwaysVisible());
  EXPECT_FALSE(contents()->FocusLocationBarByDefault());

  // Start a pending navigation to a regular page.
  GURL next_url("http://google.com/");
  controller()->LoadURL(next_url, GURL(), PageTransition::LINK);

  // Check the flags. Some should reflect the new page (URL, title), some should
  // reflect the old one (bookmark bar) until it has committed.
  EXPECT_TRUE(contents()->ShouldDisplayURL());
  EXPECT_TRUE(contents()->ShouldDisplayFavIcon());
  EXPECT_TRUE(contents()->IsBookmarkBarAlwaysVisible());
  EXPECT_FALSE(contents()->FocusLocationBarByDefault());

  // Commit the regular page load. Note that we must send it to the "pending"
  // RenderViewHost, since this transition will also cause a process transition,
  // and our RVH pointer will be the "committed" one.
  static_cast<TestRenderViewHost*>(
      contents()->render_manager()->pending_render_view_host())->SendNavigate(
          2, next_url);

  // The state should now reflect a regular page.
  EXPECT_TRUE(contents()->ShouldDisplayURL());
  EXPECT_TRUE(contents()->ShouldDisplayFavIcon());
  EXPECT_FALSE(contents()->IsBookmarkBarAlwaysVisible());
  EXPECT_FALSE(contents()->FocusLocationBarByDefault());
}

TEST_F(DOMUITest, DOMUIToDOMUI) {
  // Do a load (this state is tested above).
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  controller()->LoadURL(new_tab_url, GURL(), PageTransition::LINK);
  rvh()->SendNavigate(1, new_tab_url);

  // Start another pending load of the new tab page.
  controller()->LoadURL(new_tab_url, GURL(), PageTransition::LINK);
  rvh()->SendNavigate(2, new_tab_url);

  // The flags should be the same as the non-pending state.
  EXPECT_FALSE(contents()->ShouldDisplayURL());
  EXPECT_FALSE(contents()->ShouldDisplayFavIcon());
  EXPECT_TRUE(contents()->IsBookmarkBarAlwaysVisible());
  EXPECT_FALSE(contents()->FocusLocationBarByDefault());
}

TEST_F(DOMUITest, StandardToDOMUI) {
  // Start a pending navigation to a regular page.
  GURL std_url("http://google.com/");
  controller()->LoadURL(std_url, GURL(), PageTransition::LINK);

  // The state should now reflect the default.
  EXPECT_TRUE(contents()->ShouldDisplayURL());
  EXPECT_TRUE(contents()->ShouldDisplayFavIcon());
  EXPECT_FALSE(contents()->IsBookmarkBarAlwaysVisible());
  EXPECT_FALSE(contents()->FocusLocationBarByDefault());

  // Commit the load, the state should be the same.
  rvh()->SendNavigate(1, std_url);
  EXPECT_TRUE(contents()->ShouldDisplayURL());
  EXPECT_TRUE(contents()->ShouldDisplayFavIcon());
  EXPECT_FALSE(contents()->IsBookmarkBarAlwaysVisible());
  EXPECT_FALSE(contents()->FocusLocationBarByDefault());

  // Start a pending load for a DOMUI.
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  controller()->LoadURL(new_tab_url, GURL(), PageTransition::LINK);
  EXPECT_FALSE(contents()->ShouldDisplayURL());
  EXPECT_FALSE(contents()->ShouldDisplayFavIcon());
  EXPECT_FALSE(contents()->IsBookmarkBarAlwaysVisible());
  EXPECT_TRUE(contents()->FocusLocationBarByDefault());

  // Committing DOM UI is tested above.
}
