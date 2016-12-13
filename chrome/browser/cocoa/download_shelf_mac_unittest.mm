// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/download_shelf_mac.h"
#include "testing/gtest/include/gtest/gtest.h"


// A fake implementation of DownloadShelfController. It implements only the
// methods that DownloadShelfMac call during the tests in this file. We get this
// class into the DownloadShelfMac constructor by some questionable casting --
// Objective C is a dynamic language, so we pretend that's ok.

@interface FakeDownloadShelfController : NSObject {
 @public
  int callCountIsVisible;
  int callCountShow;
  int callCountHide;
}

- (BOOL)isVisible;
- (IBAction)show:(id)sender;
- (IBAction)hide:(id)sender;
@end

@implementation FakeDownloadShelfController

- (BOOL)isVisible {
  ++callCountIsVisible;
  return YES;
}

- (IBAction)show:(id)sender {
  ++callCountShow;
}

- (IBAction)hide:(id)sender {
  ++callCountHide;
}

@end


namespace {

class DownloadShelfMacTest : public testing::Test {

  virtual void SetUp() {
    shelf_controller_.reset([[FakeDownloadShelfController alloc] init]);
  }

 protected:
  scoped_nsobject<FakeDownloadShelfController> shelf_controller_;
  CocoaTestHelper helper_;
  BrowserTestHelper browser_helper_;
};

TEST_F(DownloadShelfMacTest, CreationCallsShow) {
  // Also make sure the DownloadShelfMacTest constructor doesn't crash.
  DownloadShelfMac shelf(browser_helper_.browser(),
      (DownloadShelfController*)shelf_controller_.get());
  EXPECT_EQ(1, shelf_controller_.get()->callCountShow);
}

TEST_F(DownloadShelfMacTest, ForwardsShow) {
  DownloadShelfMac shelf(browser_helper_.browser(),
      (DownloadShelfController*)shelf_controller_.get());
  EXPECT_EQ(1, shelf_controller_.get()->callCountShow);
  shelf.Show();
  EXPECT_EQ(2, shelf_controller_.get()->callCountShow);
}

TEST_F(DownloadShelfMacTest, ForwardsHide) {
  DownloadShelfMac shelf(browser_helper_.browser(),
      (DownloadShelfController*)shelf_controller_.get());
  EXPECT_EQ(0, shelf_controller_.get()->callCountHide);
  shelf.Close();
  EXPECT_EQ(1, shelf_controller_.get()->callCountHide);
}

TEST_F(DownloadShelfMacTest, ForwardsIsShowing) {
  DownloadShelfMac shelf(browser_helper_.browser(),
      (DownloadShelfController*)shelf_controller_.get());
  EXPECT_EQ(0, shelf_controller_.get()->callCountIsVisible);
  shelf.IsShowing();
  EXPECT_EQ(1, shelf_controller_.get()->callCountIsVisible);
}

}  // namespace
