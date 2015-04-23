// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

// Pretend BookmarkURLOpener delegate to keep track of requests
@interface BookmarkURLOpenerPong : NSObject<BookmarkURLOpener> {
 @public
  GURL url_;
}
@end

@implementation BookmarkURLOpenerPong
- (void)openBookmarkURL:(const GURL&)url
            disposition:(WindowOpenDisposition)disposition {
  url_ = url;
}
@end


namespace {

static const int kContentAreaHeight = 500;

class BookmarkBarControllerTest : public testing::Test {
 public:
  BookmarkBarControllerTest() {
    NSRect content_frame = NSMakeRect(0, 0, 800, kContentAreaHeight);
    NSRect bar_frame = NSMakeRect(0, 0, 800, 0);
    content_area_.reset([[NSView alloc] initWithFrame:content_frame]);
    bar_view_.reset([[NSView alloc] initWithFrame:bar_frame]);
    [bar_view_ setHidden:YES];
    BookmarkBarView *bbv = (BookmarkBarView*)bar_view_.get();
    bar_.reset(
        [[BookmarkBarController alloc] initWithProfile:helper_.profile()
                                                  view:bbv
                                        webContentView:content_area_.get()
                                              delegate:nil]);
    NSView* parent = cocoa_helper_.contentView();
    [parent addSubview:content_area_.get()];
    [parent addSubview:[bar_ view]];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSView> content_area_;
  scoped_nsobject<NSView> bar_view_;
  BrowserTestHelper helper_;
  scoped_nsobject<BookmarkBarController> bar_;
};

TEST_F(BookmarkBarControllerTest, ShowHide) {
  // Assume hidden by default in a new profile.
  EXPECT_FALSE([bar_ isBookmarkBarVisible]);
  EXPECT_TRUE([[bar_ view] isHidden]);
  EXPECT_EQ([bar_view_ frame].size.height, 0);

  // Show and hide it by toggling.
  [bar_ toggleBookmarkBar];
  EXPECT_TRUE([bar_ isBookmarkBarVisible]);
  EXPECT_FALSE([[bar_ view] isHidden]);
  NSRect content_frame = [content_area_ frame];
  EXPECT_NE(content_frame.size.height, kContentAreaHeight);
  EXPECT_GT([bar_view_ frame].size.height, 0);
  [bar_ toggleBookmarkBar];
  EXPECT_FALSE([bar_ isBookmarkBarVisible]);
  EXPECT_TRUE([[bar_ view] isHidden]);
  content_frame = [content_area_ frame];
  EXPECT_EQ(content_frame.size.height, kContentAreaHeight);
  EXPECT_EQ([bar_view_ frame].size.height, 0);
}

// Confirm openBookmark: forwards the request to the controller's delegate
TEST_F(BookmarkBarControllerTest, OpenBookmark) {
  GURL gurl("http://walla.walla.ding.dong.com");
  scoped_ptr<BookmarkNode> node(new BookmarkNode(gurl));
  scoped_nsobject<BookmarkURLOpenerPong> pong([[BookmarkURLOpenerPong alloc]
                                                init]);
  [bar_ setDelegate:pong.get()];

  scoped_nsobject<NSButtonCell> cell([[NSButtonCell alloc] init]);
  scoped_nsobject<NSButton> button([[NSButton alloc] init]);
  [button setCell:cell.get()];
  [cell setRepresentedObject:[NSValue valueWithPointer:node.get()]];
  [bar_ openBookmark:button];

  EXPECT_EQ(pong.get()->url_, node->GetURL());
}

// TODO(jrg): Make sure showing the bookmark bar calls loaded: (to
// process bookmarks)
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
