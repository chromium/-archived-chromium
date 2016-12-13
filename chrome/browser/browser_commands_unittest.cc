// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"

typedef BrowserWithTestWindowTest BrowserCommandsTest;

// Tests IDC_SELECT_TAB_0, IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB and
// IDC_SELECT_LAST_TAB.
TEST_F(BrowserCommandsTest, TabNavigationAccelerators) {
  GURL about_blank(chrome::kAboutBlankURL);

  // Create three tabs.
  AddTab(browser(), about_blank);
  AddTab(browser(), about_blank);
  AddTab(browser(), about_blank);

  // Select the second tab.
  browser()->SelectTabContentsAt(1, false);

  // Navigate to the first tab using an accelerator.
  browser()->ExecuteCommand(IDC_SELECT_TAB_0);
  ASSERT_EQ(0, browser()->selected_index());

  // Navigate to the second tab using the next accelerators.
  browser()->ExecuteCommand(IDC_SELECT_NEXT_TAB);
  ASSERT_EQ(1, browser()->selected_index());

  // Navigate back to the first tab using the previous accelerators.
  browser()->ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
  ASSERT_EQ(0, browser()->selected_index());

  // Navigate to the last tab using the select last accelerator.
  browser()->ExecuteCommand(IDC_SELECT_LAST_TAB);
  ASSERT_EQ(2, browser()->selected_index());
}

// Tests IDC_DUPLICATE_TAB.
TEST_F(BrowserCommandsTest, DuplicateTab) {
  GURL url1("http://foo/1");
  GURL url2("http://foo/2");
  GURL url3("http://foo/3");

  // Navigate to the three urls, then go back.
  AddTab(browser(), url1);
  NavigateAndCommitActiveTab(url2);
  NavigateAndCommitActiveTab(url3);

  size_t initial_window_count = BrowserList::size();

  // Duplicate the tab.
  browser()->ExecuteCommand(IDC_DUPLICATE_TAB);

  // The duplicated tab should not end up in a new window.
  size_t window_count = BrowserList::size();
  ASSERT_EQ(initial_window_count, window_count);

  // And we should have a newly duplicated tab.
  ASSERT_EQ(2, browser()->tab_count());

  // Verify the stack of urls.
  NavigationController& controller =
      browser()->GetTabContentsAt(1)->controller();
  ASSERT_EQ(3, controller.entry_count());
  ASSERT_EQ(2, controller.GetCurrentEntryIndex());
  ASSERT_TRUE(url1 == controller.GetEntryAtIndex(0)->url());
  ASSERT_TRUE(url2 == controller.GetEntryAtIndex(1)->url());
  ASSERT_TRUE(url3 == controller.GetEntryAtIndex(2)->url());
}

TEST_F(BrowserCommandsTest, BookmarkCurrentPage) {
  // We use profile() here, since it's a TestingProfile.
  profile()->CreateBookmarkModel(true);
  profile()->BlockUntilBookmarkModelLoaded();

  // Navigate to a url.
  GURL url1("http://foo/1");
  AddTab(browser(), url1);
  browser()->OpenURL(url1, GURL(), CURRENT_TAB, PageTransition::TYPED);

  // TODO(beng): remove this once we can use TabContentses directly in testing
  //             instead of the TestTabContents which causes this command not to
  //             be enabled when the tab is added (and selected).
  browser()->command_updater()->UpdateCommandEnabled(IDC_STAR, true);

  // Star it.
  browser()->ExecuteCommand(IDC_STAR);

  // It should now be bookmarked in the bookmark model.
  EXPECT_EQ(profile(), browser()->profile());
  EXPECT_TRUE(browser()->profile()->GetBookmarkModel()->IsBookmarked(url1));
}

// Tests back/forward in new tab (Control + Back/Forward button in the UI).
TEST_F(BrowserCommandsTest, BackForwardInNewTab) {
  GURL url1("http://foo/1");
  GURL url2("http://foo/2");

  // Make a tab with the two pages navigated in it.
  AddTab(browser(), url1);
  NavigateAndCommitActiveTab(url2);

  // Go back in a new background tab.
  browser()->GoBack(NEW_BACKGROUND_TAB);
  EXPECT_EQ(0, browser()->selected_index());
  ASSERT_EQ(2, browser()->tab_count());

  // The original tab should be unchanged.
  TabContents* zeroth = browser()->GetTabContentsAt(0);
  EXPECT_EQ(url2, zeroth->GetURL());
  EXPECT_TRUE(zeroth->controller().CanGoBack());
  EXPECT_FALSE(zeroth->controller().CanGoForward());

  // The new tab should be like the first one but navigated back.
  TabContents* first = browser()->GetTabContentsAt(1);
  EXPECT_EQ(url1, browser()->GetTabContentsAt(1)->GetURL());
  EXPECT_FALSE(first->controller().CanGoBack());
  EXPECT_TRUE(first->controller().CanGoForward());

  // Select the second tab and make it go forward in a new background tab.
  browser()->SelectTabContentsAt(1, true);
  // TODO(brettw) bug 11055: It should not be necessary to commit the load here,
  // but because of this bug, it will assert later if we don't. When the bug is
  // fixed, one of the three commits here related to this bug should be removed
  // (to test both codepaths).
  CommitPendingLoad(&first->controller());
  EXPECT_EQ(1, browser()->selected_index());
  browser()->GoForward(NEW_BACKGROUND_TAB);

  // The previous tab should be unchanged and still in the foreground.
  EXPECT_EQ(url1, first->GetURL());
  EXPECT_FALSE(first->controller().CanGoBack());
  EXPECT_TRUE(first->controller().CanGoForward());
  EXPECT_EQ(1, browser()->selected_index());

  // There should be a new tab navigated forward.
  ASSERT_EQ(3, browser()->tab_count());
  TabContents* second = browser()->GetTabContentsAt(2);
  EXPECT_EQ(url2, second->GetURL());
  EXPECT_TRUE(second->controller().CanGoBack());
  EXPECT_FALSE(second->controller().CanGoForward());

  // Now do back in a new foreground tab. Don't bother re-checking every sngle
  // thing above, just validate that it's opening properly.
  browser()->SelectTabContentsAt(2, true);
  // TODO(brettw) bug 11055: see the comment above about why we need this.
  CommitPendingLoad(&second->controller());
  browser()->GoBack(NEW_FOREGROUND_TAB);
  ASSERT_EQ(3, browser()->selected_index());
  ASSERT_EQ(url1, browser()->GetSelectedTabContents()->GetURL());

  // Same thing again for forward.
  // TODO(brettw) bug 11055: see the comment above about why we need this.
  CommitPendingLoad(&browser()->GetSelectedTabContents()->controller());
  browser()->GoForward(NEW_FOREGROUND_TAB);
  ASSERT_EQ(4, browser()->selected_index());
  ASSERT_EQ(url2, browser()->GetSelectedTabContents()->GetURL());
}
