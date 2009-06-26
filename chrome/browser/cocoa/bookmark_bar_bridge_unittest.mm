// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/bookmark_bar_bridge.h"
#include "chrome/browser/cocoa/bookmark_bar_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(jrg): add OCMock to Chromium to save some typing.

namespace {

// Information needed to open a URL, as passed to the
// BookmarkBarController's delegate.
typedef std::pair<GURL,WindowOpenDisposition> OpenInfo;

}  // The namespace must end here -- I need to use OpenInfo in
   // FakeBookmarkBarController but can't place
   // FakeBookmarkBarController itself in the namespace ("error:
   // Objective-C declarations may only appear in global scope")

// Oddly, we are our own delegate.
@interface FakeBookmarkBarController :
  BookmarkBarController<BookmarkURLOpener> {
 @public
  scoped_nsobject<NSMutableArray> callbacks_;

  std::vector<OpenInfo> opens_;
}
@end

@implementation FakeBookmarkBarController

- (id)initWithProfile:(Profile*)profile
          contentArea:(NSView*)content {
  if ((self = [super initWithProfile:profile
                         contentView:content
                            delegate:self])) {
    callbacks_.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (void)loaded:(BookmarkModel*)model {
  [callbacks_ addObject:[NSNumber numberWithInt:0]];
}

- (void)beingDeleted:(BookmarkModel*)model {
  [callbacks_ addObject:[NSNumber numberWithInt:1]];
}

- (void)nodeMoved:(BookmarkModel*)model
        oldParent:(const BookmarkNode*)oldParent oldIndex:(int)oldIndex
        newParent:(const BookmarkNode*)newParent newIndex:(int)newIndex {
  [callbacks_ addObject:[NSNumber numberWithInt:2]];
}

- (void)nodeAdded:(BookmarkModel*)model
           parent:(const BookmarkNode*)oldParent index:(int)index {
  [callbacks_ addObject:[NSNumber numberWithInt:3]];
}

- (void)nodeChanged:(BookmarkModel*)model
               node:(const BookmarkNode*)node {
  [callbacks_ addObject:[NSNumber numberWithInt:4]];
}

- (void)nodeFavIconLoaded:(BookmarkModel*)model
                     node:(const BookmarkNode*)node {
  [callbacks_ addObject:[NSNumber numberWithInt:5]];
}

- (void)nodeChildrenReordered:(BookmarkModel*)model
                         node:(const BookmarkNode*)node {
  [callbacks_ addObject:[NSNumber numberWithInt:6]];
}

// Save the request.
- (void)openBookmarkURL:(const GURL&)url
            disposition:(WindowOpenDisposition)disposition {
  opens_.push_back(OpenInfo(url, disposition));
}

@end


class BookmarkBarBridgeTest : public testing::Test {
 public:
  BookmarkBarBridgeTest() {
    NSRect content_frame = NSMakeRect(0, 0, 100, 100);
    view_.reset([[NSView alloc] initWithFrame:content_frame]);
  }

  CocoaTestHelper cocoa_helper_;
  BrowserTestHelper browser_test_helper_;
  scoped_nsobject<NSView> view_;
};

// Call all the callbacks; make sure they are all redirected to the objc object.
TEST_F(BookmarkBarBridgeTest, TestRedirect) {
  Profile *profile = browser_test_helper_.profile();
  BookmarkModel *model = profile->GetBookmarkModel();

  scoped_nsobject<FakeBookmarkBarController>
    controller([[FakeBookmarkBarController alloc] initWithProfile:profile
                                                      contentArea:view_]);
  EXPECT_TRUE(controller.get());
  scoped_ptr<BookmarkBarBridge> bridge(new BookmarkBarBridge(controller.get(),
                                                             model));
  EXPECT_TRUE(bridge.get());

  bridge->Loaded(NULL);
  bridge->BookmarkModelBeingDeleted(NULL);
  bridge->BookmarkNodeMoved(NULL, NULL, 0, NULL, 0);
  bridge->BookmarkNodeAdded(NULL, NULL, 0);
  bridge->BookmarkNodeChanged(NULL, NULL);
  bridge->BookmarkNodeFavIconLoaded(NULL, NULL);
  bridge->BookmarkNodeChildrenReordered(NULL, NULL);

  // 7 calls above plus an initial Loaded() in init routine makes 8
  EXPECT_TRUE([controller.get()->callbacks_ count] == 8);

  for (int x = 1; x < 8; x++) {
    NSNumber *num = [NSNumber numberWithInt:x-1];
    EXPECT_TRUE([[controller.get()->callbacks_ objectAtIndex:x] isEqual:num]);
  }
}
