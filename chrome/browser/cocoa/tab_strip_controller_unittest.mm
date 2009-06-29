// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Stub model delegate
class TestTabStripDelegate : public TabStripModelDelegate {
 public:
  virtual TabContents* AddBlankTab(bool foreground) {
    return NULL;
  }
  virtual TabContents* AddBlankTabAt(int index, bool foreground) {
    return NULL;
  }
  virtual Browser* CreateNewStripWithContents(TabContents* contents,
                                              const gfx::Rect& window_bounds,
                                              const DockInfo& dock_info) {
    return NULL;
  }
  virtual int GetDragActions() const {
    return 0;
  }
  virtual TabContents* CreateTabContentsForURL(
      const GURL& url,
      const GURL& referrer,
      Profile* profile,
      PageTransition::Type transition,
      bool defer_load,
      SiteInstance* instance) const {
    return NULL;
  }
  virtual bool CanDuplicateContentsAt(int index) { return true; }
  virtual void DuplicateContentsAt(int index) { }
  virtual void CloseFrameAfterDragSession() { }
  virtual void CreateHistoricalTab(TabContents* contents) { }
  virtual bool RunUnloadListenerBeforeClosing(TabContents* contents) {
    return true;
  }
  virtual bool CanRestoreTab() {
    return true;
  }
  virtual void RestoreTab() { }

  virtual bool CanCloseContentsAt(int index) { return true; }
};

class TabStripControllerTest : public testing::Test {
 public:
  TabStripControllerTest() {
    NSView* parent = cocoa_helper_.contentView();
    NSRect content_frame = [parent frame];

    // Create the "switch view" (view that gets changed out when a tab
    // switches).
    NSRect switch_frame = NSMakeRect(0, 0, content_frame.size.width, 500);
    scoped_nsobject<NSView> switch_view([[NSView alloc]
                                          initWithFrame:switch_frame]);
    [parent addSubview:switch_view.get()];

    // Create the tab strip view. It's expected to have a child button in it
    // already as the "new tab" button so create that too.
    NSRect strip_frame = NSMakeRect(0, NSMaxY(switch_frame),
                                    content_frame.size.width, 30);
    tab_strip_.reset([[NSView alloc] initWithFrame:strip_frame]);
    [parent addSubview:tab_strip_.get()];
    NSRect button_frame = NSMakeRect(0, 0, 15, 15);
    scoped_nsobject<NSButton> close_button([[NSButton alloc]
                                             initWithFrame:button_frame]);
    [tab_strip_ addSubview:close_button.get()];

    // Create the controller using that view and a model we create that has
    // no other Browser ties.
    delegate_.reset(new TestTabStripDelegate());
    model_.reset(new TabStripModel(delegate_.get(),
                                   browser_helper_.profile()));
    controller_.reset([[TabStripController alloc]
                        initWithView:(TabStripView*)tab_strip_.get()
                          switchView:switch_view.get()
                               model:model_.get()]);
  }

  CocoaTestHelper cocoa_helper_;
  BrowserTestHelper browser_helper_;
  scoped_ptr<TestTabStripDelegate> delegate_;
  scoped_ptr<TabStripModel> model_;
  scoped_nsobject<TabStripController> controller_;
  scoped_nsobject<NSView> tab_strip_;
};

// Test adding and removing tabs and making sure that views get added to
// the tab strip.
TEST_F(TabStripControllerTest, AddRemoveTabs) {
  EXPECT_TRUE(model_->empty());
#if 0
  // TODO(pinkerton): Creating a TabContents crashes an unrelated test, even
  // if you don't do anything with it. http://crbug.com/10899
  SiteInstance* instance =
      SiteInstance::CreateSiteInstance(browser_helper_.profile());
  TabContents* tab_contents =
      new TabContents(browser_helper_.profile(), instance);
  model_->AppendTabContents(tab_contents, true);
  EXPECT_EQ(model_->count(), 1);
#endif
}

TEST_F(TabStripControllerTest, SelectTab) {
  // TODO(pinkerton): Creating a TabContents crashes an unrelated test, even
  // if you don't do anything with it. http://crbug.com/10899
}

TEST_F(TabStripControllerTest, RearrangeTabs) {
  // TODO(pinkerton): Creating a TabContents crashes an unrelated test, even
  // if you don't do anything with it. http://crbug.com/10899
}

// Test that changing the number of tabs broadcasts a
// kTabStripNumberOfTabsChanged notifiction.
TEST_F(TabStripControllerTest, Notifications) {
  // TODO(pinkerton): Creating a TabContents crashes an unrelated test, even
  // if you don't do anything with it. http://crbug.com/10899
}

}  // namespace
