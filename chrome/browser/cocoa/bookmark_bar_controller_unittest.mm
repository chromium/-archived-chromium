// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const int kContentAreaHeight = 500;

class BookmarkBarControllerTest : public testing::Test {
 public:
  BookmarkBarControllerTest() {
    NSRect content_frame = NSMakeRect(0, 0, 800, kContentAreaHeight);
    content_area_.reset([[NSView alloc] initWithFrame:content_frame]);
    bar_.reset(
        [[BookmarkBarController alloc] initWithProfile:helper_.profile()
                                           contentView:content_area_.get()
                                              delegate:nil]);
    NSView* parent = cocoa_helper_.contentView();
    [parent addSubview:content_area_.get()];
    [parent addSubview:[bar_ view]];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSView> content_area_;
  BrowserTestHelper helper_;
  scoped_nsobject<BookmarkBarController> bar_;
};

TEST_F(BookmarkBarControllerTest, ShowHide) {
  // Assume hidden by default in a new profile.
  EXPECT_FALSE([bar_ isBookmarkBarVisible]);
  EXPECT_TRUE([[bar_ view] isHidden]);

  // Show and hide it by toggling.
  [bar_ toggleBookmarkBar];
  EXPECT_TRUE([bar_ isBookmarkBarVisible]);
  EXPECT_FALSE([[bar_ view] isHidden]);
  NSRect content_frame = [content_area_ frame];
  EXPECT_NE(content_frame.size.height, kContentAreaHeight);
  [bar_ toggleBookmarkBar];
  EXPECT_FALSE([bar_ isBookmarkBarVisible]);
  EXPECT_TRUE([[bar_ view] isHidden]);
  content_frame = [content_area_ frame];
  EXPECT_EQ(content_frame.size.height, kContentAreaHeight);
}

// TODO(jrg): replace getTabContents
TEST_F(BookmarkBarControllerTest, OpenBookmark) {
}

// TODO(jrg): Make sure showing the bookmark bar calls loaded: (to process bookmarks)
TEST_F(BookmarkBarControllerTest, ShowAndLoad) {
}

// TODO(jrg): Make sure a cleared bar has no subviews
TEST_F(BookmarkBarControllerTest, Clear) {
}

// TODO(jrg): Make sure loaded: does something useful
TEST_F(BookmarkBarControllerTest, Loaded) {
  // Clear; make sure no views
  // Call loaded:
  // Make sure subviews
}

// TODO(jrg): Test cellForBookmarkNode:
TEST_F(BookmarkBarControllerTest, Cell) {
}

// TODO(jrg): Test frameForBookmarkAtIndex
TEST_F(BookmarkBarControllerTest, FrameAtIndex) {
}

TEST_F(BookmarkBarControllerTest, Contents) {
  // TODO(jrg): addNodesToBar has a LOT of TODOs; when flushed out, write
  // appropriate tests.
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarControllerTest, Display) {
  [[bar_ view] display];
}

}  // namespace
