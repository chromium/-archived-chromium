// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "views/event.h"


class TabDraggingTest : public UITest {
protected:
  TabDraggingTest() {
    show_window_ = true;
  }
};

// Automated UI test to open three tabs in a new window, and drag Tab_1 into
// the position of Tab_2.
// Disabled as per http://crbug.com/10941
TEST_F(TabDraggingTest, DISABLED_Tab1Tab2) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());

  // Get initial tab count.
  int initial_tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&initial_tab_count));
  ASSERT_TRUE(1 == initial_tab_count);

  // Get Tab_1 which comes with the browser window.
  scoped_refptr<TabProxy> tab1(browser->GetTab(0));
  ASSERT_TRUE(tab1.get());
  GURL tab1_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

  // Add Tab_2.
  GURL tab2_url("about:");
  ASSERT_TRUE(browser->AppendTab(tab2_url));
  scoped_refptr<TabProxy> tab2(browser->GetTab(1));
  ASSERT_TRUE(tab2.get());

  // Add Tab_3.
  GURL tab3_url("about:plugins");
  ASSERT_TRUE(browser->AppendTab(tab3_url));
  scoped_refptr<TabProxy> tab3(browser->GetTab(2));
  ASSERT_TRUE(tab3.get());

  // Make sure 3 tabs are open.
  ASSERT_TRUE(browser->WaitForTabCountToBecome(initial_tab_count + 2,
                                               10000));

  // Get bounds for the tabs.
  gfx::Rect bounds1;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_0, &bounds1, false));
  EXPECT_LT(0, bounds1.x());
  EXPECT_LT(0, bounds1.width());
  EXPECT_LT(0, bounds1.height());

  gfx::Rect bounds2;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_1, &bounds2, false));
  EXPECT_LT(0, bounds2.width());
  EXPECT_LT(0, bounds2.height());
  EXPECT_LT(bounds1.x(), bounds2.x());
  EXPECT_EQ(bounds2.y(), bounds1.y());

  gfx::Rect bounds3;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_2, &bounds3, false));
  EXPECT_LT(0, bounds3.width());
  EXPECT_LT(0, bounds3.height());
  EXPECT_LT(bounds2.x(), bounds3.x());
  EXPECT_EQ(bounds3.y(), bounds2.y());

  // Get url Bar bounds.
  gfx::Rect urlbar_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &urlbar_bounds,
                                    false));
  EXPECT_LT(0, urlbar_bounds.x());
  EXPECT_LT(0, urlbar_bounds.y());
  EXPECT_LT(0, urlbar_bounds.width());
  EXPECT_LT(0, urlbar_bounds.height());

  // TEST: Move Tab_1 to the position of Tab_2
  //   ____________   ____________   ____________
  //  /            \ /            \ /            \
  // |    Tab_1     |     Tab_2    |    Tab_3     |
  //  ---- ---- ---- ---- ---- ---- ---- ---- ----
  //         x---- ---->
  //              ____________
  //             /     X      \
  //            |    Tab_1     |
  //             ---- ---- ----

  POINT start;
  POINT end;
  start.x = bounds1.x() + bounds1.width()/2;
  start.y = bounds1.y() + bounds1.height()/2;
  end.x = start.x + 2*bounds1.width()/3;
  end.y = start.y;
  ASSERT_TRUE(browser->SimulateDrag(start, end,
                                    views::Event::EF_LEFT_BUTTON_DOWN,
                                    false));

  // Now check for expected results.
  tab1 = browser->GetTab(0);
  ASSERT_TRUE(tab1.get());
  GURL tab1_new_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

  tab2 = browser->GetTab(1);
  ASSERT_TRUE(tab2.get());
  GURL tab2_new_url;
  ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

  EXPECT_EQ(tab1_url.spec(), tab2_new_url.spec());
  EXPECT_EQ(tab2_url.spec(), tab1_new_url.spec());
}

// Drag Tab_1 into the position of Tab_3.
// Disabled as per http://crbug.com/10941
TEST_F(TabDraggingTest, DISABLED_Tab1Tab3) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());

  // Get initial tab count.
  int initial_tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&initial_tab_count));
  ASSERT_TRUE(1 == initial_tab_count);

  // Get Tab_1 which comes with the browser window.
  scoped_refptr<TabProxy> tab1(browser->GetTab(0));
  ASSERT_TRUE(tab1.get());
  GURL tab1_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

  // Add Tab_2.
  GURL tab2_url("about:");
  ASSERT_TRUE(browser->AppendTab(tab2_url));
  scoped_refptr<TabProxy> tab2(browser->GetTab(1));
  ASSERT_TRUE(tab2.get());

  // Add Tab_3.
  GURL tab3_url("about:plugins");
  ASSERT_TRUE(browser->AppendTab(tab3_url));
  scoped_refptr<TabProxy> tab3(browser->GetTab(2));
  ASSERT_TRUE(tab3.get());

  // Make sure 3 tabs are open.
  ASSERT_TRUE(browser->WaitForTabCountToBecome(initial_tab_count + 2,
                                               10000));

  // Get bounds for the tabs.
  gfx::Rect bounds1;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_0, &bounds1, false));
  EXPECT_LT(0, bounds1.x());
  EXPECT_LT(0, bounds1.width());
  EXPECT_LT(0, bounds1.height());

  gfx::Rect bounds2;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_1, &bounds2, false));
  EXPECT_LT(0, bounds2.width());
  EXPECT_LT(0, bounds2.height());
  EXPECT_LT(bounds1.x(), bounds2.x());
  EXPECT_EQ(bounds2.y(), bounds1.y());

  gfx::Rect bounds3;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_2, &bounds3, false));
  EXPECT_LT(0, bounds3.width());
  EXPECT_LT(0, bounds3.height());
  EXPECT_LT(bounds2.x(), bounds3.x());
  EXPECT_EQ(bounds3.y(), bounds2.y());

  // Get url Bar bounds.
  gfx::Rect urlbar_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &urlbar_bounds,
                                    false));
  EXPECT_LT(0, urlbar_bounds.x());
  EXPECT_LT(0, urlbar_bounds.y());
  EXPECT_LT(0, urlbar_bounds.width());
  EXPECT_LT(0, urlbar_bounds.height());

  // TEST: Move Tab_1 to the middle position of Tab_3
  //   ____________   ____________   ____________
  //  /            \ /            \ /            \
  // |    Tab_1     |     Tab_2    |    Tab_3     |
  //  ---- ---- ---- ---- ---- ---- ---- ---- ----
  //         x---- ---- ---- ---- ---- ---->
  //                                  ____________
  //                                 /     X      \
  //                                |    Tab_1     |
  //                                 ---- ---- ----

  POINT start;
  POINT end;
  start.x = bounds1.x() + bounds1.width()/2;
  start.y = bounds1.y() + bounds1.height()/2;
  end.x = start.x + bounds1.width()/2 + bounds2.width() + bounds3.width()/2;
  end.y = start.y;
  ASSERT_TRUE(browser->SimulateDrag(start, end,
                                    views::Event::EF_LEFT_BUTTON_DOWN,
                                    false));

  // Now check for expected results.
  tab1 = browser->GetTab(0);
  ASSERT_TRUE(tab1.get());
  GURL tab1_new_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

  tab2 = browser->GetTab(1);
  ASSERT_TRUE(tab2.get());
  GURL tab2_new_url;
  ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

  tab3 = browser->GetTab(2);
  ASSERT_TRUE(tab3.get());
  GURL tab3_new_url;
  ASSERT_TRUE(tab3->GetCurrentURL(&tab3_new_url));

  EXPECT_EQ(tab1_new_url.spec(), tab2_url.spec());
  EXPECT_EQ(tab2_new_url.spec(), tab3_url.spec());
  EXPECT_EQ(tab3_new_url.spec(), tab1_url.spec());
}

// Drag Tab_1 into the position of Tab_3, and press ESCAPE before releasing the
// left mouse button.
TEST_F(TabDraggingTest, Tab1Tab3Escape) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());

  // Get initial tab count.
  int initial_tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&initial_tab_count));
  ASSERT_TRUE(1 == initial_tab_count);

  // Get Tab_1 which comes with the browser window.
  scoped_refptr<TabProxy> tab1(browser->GetTab(0));
  ASSERT_TRUE(tab1.get());
  GURL tab1_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

  // Add Tab_2.
  GURL tab2_url("about:");
  ASSERT_TRUE(browser->AppendTab(tab2_url));
  scoped_refptr<TabProxy> tab2(browser->GetTab(1));
  ASSERT_TRUE(tab2.get());

  // Add Tab_3.
  GURL tab3_url("about:plugins");
  ASSERT_TRUE(browser->AppendTab(tab3_url));
  scoped_refptr<TabProxy> tab3(browser->GetTab(2));
  ASSERT_TRUE(tab3.get());

  // Make sure 3 tabs are open.
  ASSERT_TRUE(browser->WaitForTabCountToBecome(initial_tab_count + 2,
                                               10000));

  // Get bounds for the tabs.
  gfx::Rect bounds1;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_0, &bounds1, false));
  EXPECT_LT(0, bounds1.x());
  EXPECT_LT(0, bounds1.width());
  EXPECT_LT(0, bounds1.height());

  gfx::Rect bounds2;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_1, &bounds2, false));
  EXPECT_LT(0, bounds2.width());
  EXPECT_LT(0, bounds2.height());
  EXPECT_LT(bounds1.x(), bounds2.x());
  EXPECT_EQ(bounds2.y(), bounds1.y());

  gfx::Rect bounds3;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_2, &bounds3, false));
  EXPECT_LT(0, bounds3.width());
  EXPECT_LT(0, bounds3.height());
  EXPECT_LT(bounds2.x(), bounds3.x());
  EXPECT_EQ(bounds3.y(), bounds2.y());

  // Get url Bar bounds.
  gfx::Rect urlbar_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &urlbar_bounds,
                                    false));
  EXPECT_LT(0, urlbar_bounds.x());
  EXPECT_LT(0, urlbar_bounds.y());
  EXPECT_LT(0, urlbar_bounds.width());
  EXPECT_LT(0, urlbar_bounds.height());

  // TEST: Move Tab_1 to the middle position of Tab_3
  //   ____________   ____________   ____________
  //  /            \ /            \ /            \
  // |    Tab_1     |     Tab_2    |    Tab_3     |
  //  ---- ---- ---- ---- ---- ---- ---- ---- ----
  //         x---- ---- ---- ---- ---- ----> + ESCAPE
  //                                  ____________
  //                                 /     X      \
  //                                |    Tab_1     |
  //                                 ---- ---- ----

  POINT start;
  POINT end;
  start.x = bounds1.x() + bounds1.width()/2;
  start.y = bounds1.y() + bounds1.height()/2;
  end.x = start.x + bounds1.width()/2 + bounds2.width() + bounds3.width()/2;
  end.y = start.y;

  // Simulate drag with 'true' as the last parameter. This will interrupt
  // in-flight with Escape.
  ASSERT_TRUE(browser->SimulateDrag(start, end,
                                    views::Event::EF_LEFT_BUTTON_DOWN,
                                    true));

  // Now check for expected results.
  tab1 = browser->GetTab(0);
  ASSERT_TRUE(tab1.get());
  GURL tab1_new_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

  tab2 = browser->GetTab(1);
  ASSERT_TRUE(tab2.get());
  GURL tab2_new_url;
  ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

  tab3 = browser->GetTab(2);
  ASSERT_TRUE(tab3.get());
  GURL tab3_new_url;
  ASSERT_TRUE(tab3->GetCurrentURL(&tab3_new_url));

  // The tabs should be in their original positions.
  EXPECT_EQ(tab1_new_url.spec(), tab1_url.spec());
  EXPECT_EQ(tab2_new_url.spec(), tab2_url.spec());
  EXPECT_EQ(tab3_new_url.spec(), tab3_url.spec());
}

// Drag Tab_2 out of the Tab strip. A new window should open with this tab.
TEST_F(TabDraggingTest, Tab2OutOfTabStrip) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());

  // Get initial tab count.
  int initial_tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&initial_tab_count));
  ASSERT_TRUE(1 == initial_tab_count);

  // Get Tab_1 which comes with the browser window.
  scoped_refptr<TabProxy> tab1(browser->GetTab(0));
  ASSERT_TRUE(tab1.get());
  GURL tab1_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

  // Add Tab_2.
  GURL tab2_url("about:version");
  ASSERT_TRUE(browser->AppendTab(tab2_url));
  scoped_refptr<TabProxy> tab2(browser->GetTab(1));
  ASSERT_TRUE(tab2.get());

  // Add Tab_3.
  GURL tab3_url("about:plugins");
  ASSERT_TRUE(browser->AppendTab(tab3_url));
  scoped_refptr<TabProxy> tab3(browser->GetTab(2));
  ASSERT_TRUE(tab3.get());

  // Make sure 3 tabs are opened.
  ASSERT_TRUE(browser->WaitForTabCountToBecome(initial_tab_count + 2,
                                               10000));

  // Make sure all the tab URL specs are different.
  ASSERT_TRUE(tab1_url != tab2_url);
  ASSERT_TRUE(tab1_url != tab3_url);
  ASSERT_TRUE(tab2_url != tab3_url);

  // Get bounds for the tabs.
  gfx::Rect bounds1;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_0, &bounds1, false));
  EXPECT_LT(0, bounds1.x());
  EXPECT_LT(0, bounds1.width());
  EXPECT_LT(0, bounds1.height());

  gfx::Rect bounds2;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_1, &bounds2, false));
  EXPECT_LT(0, bounds2.width());
  EXPECT_LT(0, bounds2.height());
  EXPECT_LT(bounds1.x(), bounds2.x());
  EXPECT_EQ(bounds2.y(), bounds1.y());

  gfx::Rect bounds3;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_2, &bounds3, false));
  EXPECT_LT(0, bounds3.width());
  EXPECT_LT(0, bounds3.height());
  EXPECT_LT(bounds2.x(), bounds3.x());
  EXPECT_EQ(bounds3.y(), bounds2.y());

  // Get url Bar bounds.
  gfx::Rect urlbar_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &urlbar_bounds,
                                    false));
  EXPECT_LT(0, urlbar_bounds.x());
  EXPECT_LT(0, urlbar_bounds.y());
  EXPECT_LT(0, urlbar_bounds.width());
  EXPECT_LT(0, urlbar_bounds.height());

  // TEST: Move Tab_2 down, out of the tab strip.
  // This should result in the following:
  //  1- Tab_3 shift left in place of Tab_2 in Window 1
  //  2- Tab_1 to remain in its place
  //  3- Tab_2 openes in a new window
  //
  //   ____________   ____________   ____________
  //  /            \ /            \ /            \
  // |    Tab_1     |     Tab_2    |    Tab_3     |
  //  ---- ---- ---- ---- ---- ---- ---- ---- ----
  //                       x
  //                       |
  //                       |  (Drag this below, out of tab strip)
  //                       V
  //                  ____________
  //                 /     X      \
  //                |    Tab_2     |   (New Window)
  //                ---- ---- ---- ---- ---- ---- ----

  POINT start;
  POINT end;
  start.x = bounds2.x() + bounds2.width()/2;
  start.y = bounds2.y() + bounds2.height()/2;
  end.x = start.x;
  end.y = start.y + 3*urlbar_bounds.height();

  // Simulate tab drag.
  ASSERT_TRUE(browser->SimulateDrag(start, end,
                                    views::Event::EF_LEFT_BUTTON_DOWN,
                                    false));

  // Now, first make sure that the old window has only two tabs remaining.
  int new_tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&new_tab_count));
  ASSERT_EQ(2, new_tab_count);

  // Get the two tabs - they are called Tab_1 and Tab_2 in the old window.
  tab1 = browser->GetTab(0);
  ASSERT_TRUE(tab1.get());
  GURL tab1_new_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

  tab2 = browser->GetTab(1);
  ASSERT_TRUE(tab2.get());
  GURL tab2_new_url;
  ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

  // Now check for proper shifting of tabs; i.e., Tab_3 in window 1 should
  // shift left to the position of Tab_2; Tab_1 should stay where it was.
  EXPECT_EQ(tab1_new_url.spec(), tab1_url.spec());
  EXPECT_EQ(tab2_new_url.spec(), tab3_url.spec());

  // Now check to make sure a new window has opened.
  scoped_refptr<BrowserProxy> browser2(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser2.get());
  scoped_refptr<WindowProxy> window2(browser2->GetWindow());
  ASSERT_TRUE(window2.get());

  // Make sure that the new window has only one tab.
  int tab_count_window_2 = 0;
  ASSERT_TRUE(browser2->GetTabCount(&tab_count_window_2));
  ASSERT_EQ(1, tab_count_window_2);

  // Get Tab_1_2 which should be Tab_1 in Window 2.
  scoped_refptr<TabProxy> tab1_2(browser2->GetTab(0));
  ASSERT_TRUE(tab1_2.get());
  GURL tab1_2_url;
  ASSERT_TRUE(tab1_2->GetCurrentURL(&tab1_2_url));

  // Tab_1_2 of Window 2 should essentially be Tab_2 of Window 1.
  EXPECT_EQ(tab1_2_url.spec(), tab2_url.spec());
  EXPECT_NE(tab1_2_url.spec(), tab1_url.spec());
  EXPECT_NE(tab1_2_url.spec(), tab3_url.spec());
}
