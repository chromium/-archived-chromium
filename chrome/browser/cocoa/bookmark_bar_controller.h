// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

@class BookmarkBarStateController;
class BookmarkModel;
class Profile;
@class ToolbarView;

// A controller for the bookmark bar in the browser window. Handles showing
// and hiding based on the preference in the given profile.

@interface BookmarkBarController : NSObject {
 @private
  BookmarkModel* bookmarkModel_;  // weak; part of the profile owned by the
                                  // top-level Browser object.

  // Controller for bookmark bar state, shared among all TabContents.
  scoped_nsobject<BookmarkBarStateController> bookmarkBarStateController_;
  BOOL contentAreaHasOffset_;

  // TODO(jrg): write a BookmarkView
  IBOutlet ToolbarView* /* BookmarkView* */ bookmarkView_;
  IBOutlet NSView* contentArea_;
}

// Initializes the controller with the given browser profile and content view.
- (id)initWithProfile:(Profile*)profile
          contentArea:(NSView*)content;

// Resizes the bookmark bar based on the state of the content area.
- (void)resizeBookmarkBar;

// Returns whether or not the bookmark bar is visible.
- (BOOL)isBookmarkBarVisible;

// Toggle the state of the bookmark bar.
- (void)toggleBookmarkBar;

@end

// These APIs should only be used by unit tests, in place of "friend" classes.
@interface BookmarkBarController(TestingAPI)
// Access to the bookmark bar's view represented by this controller.
- (NSView*)view;
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
