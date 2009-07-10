// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/find_notification_details.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Expose private variables to make testing easier.
@interface FindBarCocoaController(Testing)
- (NSTextField*)findText;
- (NSTextField*)resultsLabel;
@end

@implementation FindBarCocoaController(Testing)
- (NSTextField*)findText {
  return findText_;
}

- (NSTextField*)resultsLabel {
  return resultsLabel_;
}
@end

namespace {

class FindBarCocoaControllerTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    // TODO(rohitrao): We don't really need to do this once per test.
    // Consider moving it to SetUpTestCase().
    controller_.reset([[FindBarCocoaController alloc] init]);
    [helper_.contentView() addSubview:[controller_ view]];
  }

 protected:
  CocoaTestHelper helper_;
  scoped_nsobject<FindBarCocoaController> controller_;
};

TEST_F(FindBarCocoaControllerTest, ShowAndHide) {
  NSView* findBarView = [controller_ view];

  ASSERT_TRUE([findBarView isHidden]);
  ASSERT_FALSE([controller_ isFindBarVisible]);

  [controller_ showFindBar];
  EXPECT_FALSE([findBarView isHidden]);
  EXPECT_TRUE([controller_ isFindBarVisible]);

  [controller_ hideFindBar];
  EXPECT_TRUE([findBarView isHidden]);
  EXPECT_FALSE([controller_ isFindBarVisible]);
}

TEST_F(FindBarCocoaControllerTest, SetFindText) {
  NSView* findBarView = [controller_ view];
  NSTextField* findText = [controller_ findText];

  // Start by making the find bar visible.
  [controller_ showFindBar];
  EXPECT_FALSE([findBarView isHidden]);

  // Set the find text.
  const std::string kFindText = "Google";
  [controller_ setFindText:ASCIIToUTF16(kFindText)];
  EXPECT_EQ(
      NSOrderedSame,
      [[findText stringValue] compare:base::SysUTF8ToNSString(kFindText)]);

  // Call clearResults, which doesn't actually clear the find text but
  // simply sets it back to what it was before.  This is silly, but
  // matches the behavior on other platforms.  |details| isn't used by
  // our implementation of clearResults, so it's ok to pass in an
  // empty |details|.
  FindNotificationDetails details;
  [controller_ clearResults:details];
  EXPECT_EQ(
      NSOrderedSame,
      [[findText stringValue] compare:base::SysUTF8ToNSString(kFindText)]);
}

TEST_F(FindBarCocoaControllerTest, ResultLabelUpdatesCorrectly) {
  // TODO(rohitrao): Test this.  It may involve creating some dummy
  // FindNotificationDetails objects.
}

}  // namespace
