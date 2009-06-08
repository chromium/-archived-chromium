// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class GradientButtonCellTest : public testing::Test {
 public:
  GradientButtonCellTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    view_.reset([[NSButton alloc] initWithFrame:frame]);
    scoped_nsobject<GradientButtonCell> cell([[GradientButtonCell alloc]
                                              initTextCell:@"Testing"]);
    [view_ setCell:cell.get()];
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSButton> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(GradientButtonCellTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(GradientButtonCellTest, Display) {
  [view_ display];
}

}  // namespace
