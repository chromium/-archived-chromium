// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/bookmark_bar_bridge.h"
#include "webkit/glue/window_open_disposition.h"

@class BookmarkBarStateController;
class BookmarkModel;
class BookmarkNode;
@class BookmarkBarView;
class Profile;

// The interface for an object which can open URLs for a bookmark.
@protocol BookmarkURLOpener
- (void)openBookmarkURL:(const GURL&)url
            disposition:(WindowOpenDisposition)disposition;
@end


// A controller for the bookmark bar in the browser window. Handles showing
// and hiding based on the preference in the given profile.
@interface BookmarkBarController : NSObject {
 @private
  BookmarkModel* bookmarkModel_;  // weak; part of the profile owned by the
                                  // top-level Browser object.

  // Currently these two are always the same, but they mean slightly
  // different things.  contentAreaHasOffset_ is an implementation
  // detail of bookmark bar visibility.
  BOOL contentViewHasOffset_;
  BOOL barIsVisible_;

  // The view of the bookmark bar itself.
  // Not made into a scoped_nsobject since I may move it into a nib.
  // (See TODO in initWithProfile: in bookmark_bar_controller.mm).
  IBOutlet BookmarkBarView* bookmarkBarView_;

  // The tab content area for the window (where the web goes)
  IBOutlet NSView* contentView_;

  // Bridge from Chrome-style C++ notifications (e.g. derived from
  // BookmarkModelObserver)
  scoped_ptr<BookmarkBarBridge> bridge_;

  // Delegate which can open URLs for us.
  id<BookmarkURLOpener> delegate_;  // weak
}

// Initializes the controller with the given browser profile and
// content view.  We use |content| as a parent view for the bookmark
// bar view and for geometry management.  |delegate| is used for
// opening URLs.
- (id)initWithProfile:(Profile*)profile
          contentView:(NSView*)content
             delegate:(id<BookmarkURLOpener>)delegate;

// Resizes the bookmark bar based on the state of the content area.
- (void)resizeBookmarkBar;

// Returns whether or not the bookmark bar is visible.
- (BOOL)isBookmarkBarVisible;

// Toggle the state of the bookmark bar.
- (void)toggleBookmarkBar;

@end

// Redirects from BookmarkBarBridge, the C++ object which glues us to
// the rest of Chromium.  Internal to BookmarkBarController.
@interface BookmarkBarController(BridgeRedirect)
- (void)loaded:(BookmarkModel*)model;
- (void)beingDeleted:(BookmarkModel*)model;
- (void)nodeMoved:(BookmarkModel*)model
        oldParent:(const BookmarkNode*)oldParent oldIndex:(int)oldIndex
        newParent:(const BookmarkNode*)newParent newIndex:(int)newIndex;
- (void)nodeAdded:(BookmarkModel*)model
           parent:(const BookmarkNode*)oldParent index:(int)index;
- (void)nodeChanged:(BookmarkModel*)model
               node:(const BookmarkNode*)node;
- (void)nodeFavIconLoaded:(BookmarkModel*)model
                     node:(const BookmarkNode*)node;
- (void)nodeChildrenReordered:(BookmarkModel*)model
                         node:(const BookmarkNode*)node;
@end


// These APIs should only be used by unit tests, in place of "friend" classes.
@interface BookmarkBarController(TestingAPI)
// Access to the bookmark bar's view represented by this controller.
- (NSView*)view;
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
