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

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/views/event.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"


class TabDraggingTest : public UITest {
protected:
  TabDraggingTest() {
    show_window_ = true;
  }
};

/*
// Automated UI test to open three tabs in a new window, and drag Tab-1 into
// the position of Tab-2.
TEST_F(TabDraggingTest, Tab1Tab2) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_ptr<WindowProxy> window(
      automation()->GetWindowForBrowser(browser.get()));
  ASSERT_TRUE(window.get());

  // Get initial tab count.
  int initial_tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&initial_tab_count));

  // Get Tab-1 which comes with the browser window.
  scoped_ptr<TabProxy> tab1(browser->GetTab(0));
  ASSERT_TRUE(tab1.get());
  GURL tab1_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

  // Add Tab-2.
  GURL tab2_url("about:");
  ASSERT_TRUE(browser->AppendTab(tab2_url));
  scoped_ptr<TabProxy> tab2(browser->GetTab(1));
  ASSERT_TRUE(tab2.get());
  
  // Add Tab-3.
  GURL tab3_url("about:plugins");
  ASSERT_TRUE(browser->AppendTab(tab3_url));
  scoped_ptr<TabProxy> tab3(browser->GetTab(2));
  ASSERT_TRUE(tab3.get());

  // Make sure 3 tabs are open
  int final_tab_count = 0;
  ASSERT_TRUE(browser->WaitForTabCountToChange(initial_tab_count,
                                               &final_tab_count,
                                               10000));
  ASSERT_TRUE(final_tab_count == initial_tab_count + 2);

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

  // TEST: Move Tab-1 to the position of Tab-2
  //   ____________   ____________   ____________
  //  /            \ /            \ /            \
  // |    Tab-1     |     Tab-2    |    Tab-3     |
  //  ---- ---- ---- ---- ---- ---- ---- ---- ----
  //         x---- ---->
  //              ____________  
  //             /     X      \ 
  //            |    Tab-1     |
  //             ---- ---- ----

  POINT start;
  POINT end;
  start.x = bounds1.x() + bounds1.width()/2;
  start.y = bounds1.y() + bounds1.height()/2;
  end.x = start.x + 2*bounds1.width()/3;
  end.y = start.y;
  ASSERT_TRUE(browser->SimulateDrag(start, end,
                                    ChromeViews::Event::EF_LEFT_BUTTON_DOWN));

  // Now check for expected results.
  tab1.reset(browser->GetTab(0));
  ASSERT_TRUE(tab1.get());
  GURL tab1_new_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

  tab2.reset(browser->GetTab(1));
  ASSERT_TRUE(tab2.get());
  GURL tab2_new_url;
  ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

  EXPECT_EQ(tab1_url.spec(), tab2_new_url.spec());
  EXPECT_EQ(tab2_url.spec(), tab1_new_url.spec());
}

// Drag Tab-1 into the position of Tab-3. 
TEST_F(TabDraggingTest, Tab1Tab3) {
  /*
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_ptr<WindowProxy> window(
      automation()->GetWindowForBrowser(browser.get()));
  ASSERT_TRUE(window.get());

  // Get initial tab count.
  int initial_tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&initial_tab_count));

  // Get Tab-1 which comes with the browser window.
  scoped_ptr<TabProxy> tab1(browser->GetTab(0));
  ASSERT_TRUE(tab1.get());
  GURL tab1_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_url));

  // Add Tab-2.
  GURL tab2_url("about:");
  ASSERT_TRUE(browser->AppendTab(tab2_url));
  scoped_ptr<TabProxy> tab2(browser->GetTab(1));
  ASSERT_TRUE(tab2.get());
  
  // Add Tab-3.
  GURL tab3_url("about:plugins");
  ASSERT_TRUE(browser->AppendTab(tab3_url));
  scoped_ptr<TabProxy> tab3(browser->GetTab(2));
  ASSERT_TRUE(tab3.get());

  // Make sure 3 tabs are open
  int final_tab_count = 0;
  ASSERT_TRUE(browser->WaitForTabCountToChange(initial_tab_count,
                                               &final_tab_count,
                                               10000));
  ASSERT_TRUE(final_tab_count == initial_tab_count + 2);

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

  // TEST: Move Tab-1 to the middle position of Tab-3
  //   ____________   ____________   ____________
  //  /            \ /            \ /            \
  // |    Tab-1     |     Tab-2    |    Tab-3     |
  //  ---- ---- ---- ---- ---- ---- ---- ---- ----
  //         x---- ---- ---- ---- ---- ---->
  //                                  ____________  
  //                                 /     X      \ 
  //                                |    Tab-1     |
  //                                 ---- ---- ----

  POINT start;
  POINT end;  
  start.x = bounds1.x() + bounds1.width()/2;
  start.y = bounds1.y() + bounds1.height()/2;
  end.x = start.x + bounds1.width()/2 + bounds2.width() + bounds3.width()/2;
  end.y = start.y;
  ASSERT_TRUE(browser->SimulateDrag(start, end,
                                    ChromeViews::Event::EF_LEFT_BUTTON_DOWN));

  // Now check for expected results.
  tab1.reset(browser->GetTab(0));
  ASSERT_TRUE(tab1.get());
  GURL tab1_new_url;
  ASSERT_TRUE(tab1->GetCurrentURL(&tab1_new_url));

  tab2.reset(browser->GetTab(1));
  ASSERT_TRUE(tab2.get());
  GURL tab2_new_url;
  ASSERT_TRUE(tab2->GetCurrentURL(&tab2_new_url));

  tab3.reset(browser->GetTab(2));
  ASSERT_TRUE(tab3.get());
  GURL tab3_new_url;
  ASSERT_TRUE(tab3->GetCurrentURL(&tab3_new_url));

  EXPECT_EQ(tab1_new_url.spec(), tab2_url.spec());
  EXPECT_EQ(tab2_new_url.spec(), tab3_url.spec());
  EXPECT_EQ(tab3_new_url.spec(), tab1_url.spec());
}
*/
