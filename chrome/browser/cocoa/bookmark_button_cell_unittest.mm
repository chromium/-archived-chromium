// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BookmarkButtonCellTest : public testing::Test {
 public:
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSButton> view_;
};

// Make sure it's not totally bogus
TEST_F(BookmarkButtonCellTest, SizeForBounds) {
  NSRect frame = NSMakeRect(0, 0, 50, 30);
  view_.reset([[NSButton alloc] initWithFrame:frame]);
  scoped_nsobject<BookmarkButtonCell> cell([[BookmarkButtonCell alloc]
                                               initTextCell:@"Testing"]);
  [view_ setCell:cell.get()];
  [cocoa_helper_.contentView() addSubview:view_.get()];

  NSRect r = NSMakeRect(0, 0, 100, 100);
  NSSize size = [cell.get() cellSizeForBounds:r];
  EXPECT_TRUE(size.width > 0 && size.height > 0);
  EXPECT_TRUE(size.width < 200 && size.height < 200);
}


}  // namespace
