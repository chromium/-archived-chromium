// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsautorelease_pool.h"
#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/tab_controller.h"
#import "chrome/browser/cocoa/tab_controller_target.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Implements the target interface for the tab, which gets sent messages when
// the tab is clicked on by the user and when its close box is clicked.
@interface TabControllerTestTarget : NSObject<TabControllerTarget> {
 @private
  bool selected_;
  bool closed_;
}
- (bool)selected;
- (bool)closed;
@end

@implementation TabControllerTestTarget
- (bool)selected {
  return selected_;
}
- (bool)closed {
  return closed_;
}
- (void)selectTab:(id)sender {
  selected_ = true;
}
- (void)closeTab:(id)sender {
  closed_ = true;
}
- (void)mouseTimer:(NSTimer*)timer {
  // Fire the mouseUp to break the TabView drag loop.
  NSEvent* current = [[NSApplication sharedApplication] currentEvent];
  NSWindow* window = [timer userInfo];
  NSEvent* up = [NSEvent mouseEventWithType:NSLeftMouseUp
                                   location:[current locationInWindow]
                              modifierFlags:0
                                  timestamp:[current timestamp]
                               windowNumber:[window windowNumber]
                                    context:nil
                                eventNumber:0
                                 clickCount:1
                                   pressure:1.0];
  [window postEvent:up atStart:YES];
}
@end

namespace {

// The dragging code in TabView makes heavy use of autorelease pools so
// inherit from Platform test to have one created for us.
class TabControllerTest : public PlatformTest {
 public:
  TabControllerTest() { }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
};

// Tests creating the controller, sticking it in a window, and removing it.
TEST_F(TabControllerTest, Creation) {
  NSWindow* window = cocoa_helper_.window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];
  EXPECT_TRUE([controller tabView]);
  EXPECT_EQ([[controller view] window], window);
  [[controller view] display];  // Test drawing to ensure nothing leaks/crashes.
  [[controller view] removeFromSuperview];
}

// Tests sending it a close message and ensuring that the target/action get
// called. Mimics the user clicking on the close button in the tab.
TEST_F(TabControllerTest, Close) {
  NSWindow* window = cocoa_helper_.window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];

  scoped_nsobject<TabControllerTestTarget> target(
      [[TabControllerTestTarget alloc] init]);
  EXPECT_FALSE([target closed]);
  [controller setTarget:target];
  EXPECT_EQ(target.get(), [controller target]);

  [controller closeTab:nil];
  EXPECT_TRUE([target closed]);

  [[controller view] removeFromSuperview];
}

// Tests setting the |selected| property via code.
TEST_F(TabControllerTest, APISelection) {
  NSWindow* window = cocoa_helper_.window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];

  EXPECT_FALSE([controller selected]);
  [controller setSelected:YES];
  EXPECT_TRUE([controller selected]);

  [[controller view] removeFromSuperview];
}

// Tests that setting the title of a tab sets the tooltip as well.
TEST_F(TabControllerTest, ToolTip) {
  NSWindow* window = cocoa_helper_.window();

  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];

  EXPECT_TRUE([[controller toolTip] length] == 0);
  NSString *tooltip_string = @"Some text to use as a tab title";
  [controller setTitle:tooltip_string];
  EXPECT_TRUE([tooltip_string isEqualToString:[controller toolTip]]);
}

// Tests setting the |loading| property via code.
TEST_F(TabControllerTest, Loading) {
  NSWindow* window = cocoa_helper_.window();
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];

  EXPECT_EQ(kTabDone, [controller loadingState]);
  [controller setLoadingState:kTabWaiting];
  EXPECT_EQ(kTabWaiting, [controller loadingState]);
  [controller setLoadingState:kTabLoading];
  EXPECT_EQ(kTabLoading, [controller loadingState]);
  [controller setLoadingState:kTabDone];
  EXPECT_EQ(kTabDone, [controller loadingState]);

  [[controller view] removeFromSuperview];
}

// Tests selecting the tab with the mouse click and ensuring the target/action
// get called.
// TODO(pinkerton): It's yucky that TabView bakes in the dragging so that we
// can't test this class w/out lots of extra effort. When cole finishes the
// rewrite, we should move all that logic out into a separate controller which
// we can dependency-inject/mock so it has very simple click behavior for unit
// testing.
TEST_F(TabControllerTest, UserSelection) {
  NSWindow* window = cocoa_helper_.window();

  // Create a tab at a known location in the window that we can click on
  // to activate selection.
  scoped_nsobject<TabController> controller([[TabController alloc] init]);
  [[window contentView] addSubview:[controller view]];
  NSRect frame = [[controller view] frame];
  frame.size.width = [TabController minTabWidth];
  frame.origin = NSMakePoint(0, 0);
  [[controller view] setFrame:frame];

  // Set the target and action.
  scoped_nsobject<TabControllerTestTarget> target(
      [[TabControllerTestTarget alloc] init]);
  EXPECT_FALSE([target selected]);
  [controller setTarget:target];
  [controller setAction:@selector(selectTab:)];
  EXPECT_EQ(target.get(), [controller target]);
  EXPECT_EQ(@selector(selectTab:), [controller action]);

  // In order to track a click, we have to fake a mouse down and a mouse
  // up, but the down goes into a tight drag loop. To break the loop, we have
  // to fire a timer that sends a mouse up event while the "drag" is ongoing.
  [NSTimer scheduledTimerWithTimeInterval:0.1
                                   target:target.get()
                                 selector:@selector(mouseTimer:)
                                 userInfo:window
                                  repeats:NO];
  NSEvent* current = [[NSApplication sharedApplication] currentEvent];
  NSPoint click_point = NSMakePoint(frame.size.width / 2,
                                    frame.size.height / 2);
  NSEvent* down = [NSEvent mouseEventWithType:NSLeftMouseDown
                                     location:click_point
                                modifierFlags:0
                                    timestamp:[current timestamp]
                                 windowNumber:[window windowNumber]
                                      context:nil
                                  eventNumber:0
                                   clickCount:1
                                     pressure:1.0];
  [[controller view] mouseDown:down];

  // Check our target was told the tab got selected.
  EXPECT_TRUE([target selected]);

  [[controller view] removeFromSuperview];
}

}  // namespace
