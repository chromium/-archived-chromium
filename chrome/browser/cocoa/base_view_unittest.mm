// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/base_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BaseViewTest : public testing::Test {
 public:
  BaseViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 100);
    view_.reset([[BaseView alloc] initWithFrame:frame]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  scoped_nsobject<BaseView> view_;
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(BaseViewTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(BaseViewTest, Display) {
  [view_ display];
}

// Convert a rect in |view_|'s Cocoa coordinate system to gfx::Rect's top-left
// coordinate system. Repeat the process in reverse and make sure we come out
// with the original rect.
TEST_F(BaseViewTest, NSRectToRect) {
  NSRect convert = NSMakeRect(10, 10, 50, 50);
  gfx::Rect converted = [view_ NSRectToRect:convert];
  EXPECT_EQ(converted.x(), 10);
  EXPECT_EQ(converted.y(), 40);  // Due to view being 100px tall.
  EXPECT_EQ(converted.width(), convert.size.width);
  EXPECT_EQ(converted.height(), convert.size.height);

  // Go back the other way.
  NSRect back_again = [view_ RectToNSRect:converted];
  EXPECT_EQ(back_again.origin.x, convert.origin.x);
  EXPECT_EQ(back_again.origin.y, convert.origin.y);
  EXPECT_EQ(back_again.size.width, convert.size.width);
  EXPECT_EQ(back_again.size.height, convert.size.height);
}

}  // namespace
