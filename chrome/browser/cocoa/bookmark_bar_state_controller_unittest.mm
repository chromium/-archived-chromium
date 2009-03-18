// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/bookmark_bar_state_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface NoPrefSaveBBStateController : BookmarkBarStateController {
 @public
  BOOL toggled_;
}
@end

@implementation NoPrefSaveBBStateController
- (void)togglePreference {
  toggled_ = !toggled_;
}
@end


class BookmarkBarStateControllerTest : public testing::Test {
 public:
  BrowserTestHelper browser_test_helper_;
};


TEST_F(BookmarkBarStateControllerTest, MainTest) {
  Browser* browser = browser_test_helper_.GetBrowser();
  NoPrefSaveBBStateController *c = [[[NoPrefSaveBBStateController alloc]
                                     initWithBrowser:browser]
                                     autorelease];
  EXPECT_TRUE(c);
  EXPECT_FALSE(c->toggled_);
  BOOL old_visible = [c visible];

  [c toggleBookmarkBar];
  EXPECT_TRUE(c->toggled_ != NO);
  EXPECT_NE((bool)old_visible, (bool)[c visible]);

  [c toggleBookmarkBar];
  EXPECT_FALSE(c->toggled_);
  EXPECT_EQ((bool)old_visible, (bool)[c visible]);
}
