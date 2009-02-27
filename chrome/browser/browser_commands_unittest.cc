// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"

typedef BrowserWithTestWindowTest BrowserCommandsTest;

// Tests IDC_SELECT_TAB_0, IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB and
// IDC_SELECT_LAST_TAB.
TEST_F(BrowserCommandsTest, TabNavigationAccelerators) {
  // Create three tabs.
  AddTestingTab(browser());
  AddTestingTab(browser());
  AddTestingTab(browser());

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
  GURL url1 = test_url_with_path("1");
  GURL url2 = test_url_with_path("2");
  GURL url3 = test_url_with_path("3");

  // Navigate to the three urls, then go back.
  AddTestingTab(browser());
  browser()->OpenURL(url1, GURL(), CURRENT_TAB, PageTransition::TYPED);
  browser()->OpenURL(url2, GURL(), CURRENT_TAB, PageTransition::TYPED);
  browser()->OpenURL(url3, GURL(), CURRENT_TAB, PageTransition::TYPED);

  size_t initial_window_count = BrowserList::size();

  // Duplicate the tab.
  browser()->ExecuteCommand(IDC_DUPLICATE_TAB);

  // The duplicated tab should not end up in a new window.
  int window_count = BrowserList::size();
  ASSERT_EQ(initial_window_count, window_count);

  // And we should have a newly duplicated tab.
  ASSERT_EQ(2, browser()->tab_count());

  // Verify the stack of urls.
  NavigationController* controller =
      browser()->GetTabContentsAt(1)->controller();
  ASSERT_EQ(3, controller->GetEntryCount());
  ASSERT_EQ(2, controller->GetCurrentEntryIndex());
  ASSERT_TRUE(url1 == controller->GetEntryAtIndex(0)->url());
  ASSERT_TRUE(url2 == controller->GetEntryAtIndex(1)->url());
  ASSERT_TRUE(url3 == controller->GetEntryAtIndex(2)->url());
}

TEST_F(BrowserCommandsTest, BookmarkCurrentPage) {
  // We use profile() here, since it's a TestingProfile.
  profile()->CreateBookmarkModel(true);
  profile()->BlockUntilBookmarkModelLoaded();

  // Navigate to a url.
  GURL url1 = test_url_with_path("1");
  AddTestingTab(browser());
  browser()->OpenURL(url1, GURL(), CURRENT_TAB, PageTransition::TYPED);

  // TODO(beng): remove this once we can use WebContentses directly in testing
  //             instead of the TestTabContents which causes this command not to
  //             be enabled when the tab is added (and selected).
  browser()->command_updater()->UpdateCommandEnabled(IDC_STAR, true);

  // Star it.
  browser()->ExecuteCommand(IDC_STAR);

  // It should now be bookmarked in the bookmark model.
  EXPECT_EQ(profile(), browser()->profile());
  EXPECT_TRUE(browser()->profile()->GetBookmarkModel()->IsBookmarked(url1));
}
