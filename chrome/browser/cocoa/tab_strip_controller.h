// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

@class TabStripView;
@class BookmarkBarStateController;

class BookmarkModel;
class Browser;
class CommandUpdater;
class LocationBar;
class TabStripBridge;
class TabStripModel;
class TabContents;
class ToolbarModel;

// A class that handles managing the tab strip in a browser window. It uses
// a supporting C++ bridge object to register for notifications from the
// TabStripModel. The Obj-C part of this class handles drag and drop and all
// the other Cocoa-y aspects.
//
// When a new tab is created, we create a TabController which manages loading
// the contents, including toolbar, from a separate nib file. This controller
// then handles replacing the contentView of the window. As tabs are switched,
// the single child of the contentView is swapped around to hold the contents
// (toolbar and all) representing that tab.

@interface TabStripController : NSObject {
 @private
  TabContents* currentTab_;   // weak, tab for which we're showing state
  TabStripView* tabView_;  // weak
  NSButton* newTabButton_;  // weak, obtained from the nib.
  TabStripBridge* bridge_;
  TabStripModel* tabModel_;  // weak
  ToolbarModel* toolbarModel_;  // weak, one per browser
  BookmarkModel* bookmarkModel_;  // weak, one per profile (= one per Browser*)
  CommandUpdater* commands_;  // weak, may be nil
  // access to the TabContentsControllers (which own the parent view
  // for the toolbar and associated tab contents) given an index. This needs
  // to be kept in the same order as the tab strip's model as we will be
  // using its index from the TabStripModelObserver calls.
  NSMutableArray* tabContentsArray_;
  // an array of TabControllers which manage the actual tab views. As above,
  // this is kept in the same order as the tab strip model.
  NSMutableArray* tabArray_;

  // Controller for bookmark bar state, shared among all TabContents.
  BookmarkBarStateController* bookmarkBarStateController_;
}

// Initialize the controller with a view and browser that contains
// everything else we'll need.
- (id)initWithView:(TabStripView*)view
           browser:(Browser*)browser;

// Get the C++ bridge object representing the location bar for the current tab.
- (LocationBar*)locationBar;

// Updates the toolbar (and transitively the location bar) with the states of
// the specified |tab|.  If |shouldRestore| is true, we're switching
// (back?) to this tab and should restore any previous location bar state
// (such as user editing) as well.
- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
- (NSRect)selectedTabGrowBoxRect;

// Called to tell the selected tab to update its loading state.
- (void)setIsLoading:(BOOL)isLoading;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar;

// Return a boolean (ObjC BOOL, not C++ bool) to say if the bookmark
// bar is visible.
- (BOOL)isBookmarkBarVisible;

// Turn on or off the bookmark bar for *ALL* tabs.
- (void)toggleBookmarkBar;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
