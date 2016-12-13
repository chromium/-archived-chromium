// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/common/notification_type.h"
#include "testing/gtest/include/gtest/gtest.h"

// A BrowserWindowCocoa that goes PONG when
// BOOKMARK_BAR_VISIBILITY_PREF_CHANGED is sent.  This is so we can be
// sure we are observing it.
class BrowserWindowCocoaPong : public BrowserWindowCocoa {
 public:
  BrowserWindowCocoaPong(Browser* browser,
                         BrowserWindowController* controller,
                         NSWindow* window) :
  BrowserWindowCocoa(browser, controller, window) {
    pong_ = false;
  }
  virtual ~BrowserWindowCocoaPong() { }

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type.value == NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED)
      pong_ = true;
    BrowserWindowCocoa::Observe(type, source, details);
  }

  bool pong_;
};

// Main test class.
class BrowserWindowCocoaTest : public testing::Test {

  virtual void SetUp() {
    controller_.reset([[BrowserWindowController alloc]
                        initWithBrowser:browser_helper_.browser()
                          takeOwnership:NO]);
  }

 public:
  // Order is very important here.  We want the controller deleted
  // before the pool, and want the pool deleted before
  // BrowserTestHelper.
  CocoaTestHelper cocoa_helper_;
  BrowserTestHelper browser_helper_;
  base::ScopedNSAutoreleasePool pool_;
  scoped_nsobject<BrowserWindowController> controller_;
};


TEST_F(BrowserWindowCocoaTest, TestNotification) {
  BrowserWindowCocoaPong *bwc = new BrowserWindowCocoaPong(
    browser_helper_.browser(),
    controller_.get(),
    cocoa_helper_.window());

  EXPECT_FALSE(bwc->pong_);
  bookmark_utils::ToggleWhenVisible(browser_helper_.profile());
  // Confirm we are listening
  EXPECT_TRUE(bwc->pong_);

  delete bwc;
  // If this does NOT crash it confirms we stopped listening in the destructor.
  bookmark_utils::ToggleWhenVisible(browser_helper_.profile());
}


TEST_F(BrowserWindowCocoaTest, TestBookmarkBarVisible) {
  BrowserWindowCocoaPong *bwc = new BrowserWindowCocoaPong(
    browser_helper_.browser(),
    controller_.get(),
    cocoa_helper_.window());
  scoped_ptr<BrowserWindowCocoaPong> scoped_bwc(bwc);

  bool before = bwc->IsBookmarkBarVisible();
  bookmark_utils::ToggleWhenVisible(browser_helper_.profile());
  EXPECT_NE(before, bwc->IsBookmarkBarVisible());

  bookmark_utils::ToggleWhenVisible(browser_helper_.profile());
  EXPECT_EQ(before, bwc->IsBookmarkBarVisible());
}

@interface FakeController : NSWindowController {
  BOOL fullscreen_;
}
@end

@implementation FakeController
- (void)setFullscreen:(BOOL)fullscreen {
  fullscreen_ = fullscreen;
}
- (BOOL)isFullscreen {
  return fullscreen_;
}
@end

TEST_F(BrowserWindowCocoaTest, TestFullscreen) {
  scoped_nsobject<FakeController> fake_controller_([[FakeController alloc]
                                                     init]);
  BrowserWindowCocoaPong *bwc = new BrowserWindowCocoaPong(
    browser_helper_.browser(),
    (BrowserWindowController*)fake_controller_.get(),
    cocoa_helper_.window());
  scoped_ptr<BrowserWindowCocoaPong> scoped_bwc(bwc);

  EXPECT_FALSE(bwc->IsFullscreen());
  bwc->SetFullscreen(true);
  EXPECT_TRUE(bwc->IsFullscreen());
  bwc->SetFullscreen(false);
  EXPECT_FALSE(bwc->IsFullscreen());
}

/* TODO(???): test other methods of BrowserWindowCocoa */
