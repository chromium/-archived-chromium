// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_COTNENTS_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_COTNENTS_CONTROLLER_H_

#include <Cocoa/Cocoa.h>

@class BookmarkView;
@class GrowBoxView;
@class ToolbarView;

class BookmarkModel;
class CommandUpdater;
class LocationBar;
class TabContents;
class TabContentsCommandObserver;
class TabStripModel;
class ToolbarModel;

// A class that controls the contents of a tab, including the toolbar and
// web area.

// TODO(pinkerton): Cole and I went back and forth about whether or not each
// tab should have its own copy of the toolbar. Right now, we decided to leave
// it like this as he expects it will make it easier for him to implement
// tab dragging and tear-off into new windows. It's also not very expensive.
// As we hook things up, we'll see if this imposes other restrictions (such
// as command-handling or dispatch) that will require us to change the view
// layout.
// TODO(jrg): Following on to pink's comments... each tab does in fact
// have its own ToolbarView.  Similarly, each also has its own
// BookmarkView.  That makes things marginally more expensive.

@interface TabContentsController : NSViewController {
 @private
  CommandUpdater* commands_;  // weak, may be nil
  TabContentsCommandObserver* observer_;  // nil if |commands_| is nil
  LocationBar* locationBarBridge_;
  TabContents* contents_;  // weak

  ToolbarModel* toolbarModel_;  // weak, one per window
  IBOutlet ToolbarView* toolbarView_;

  BookmarkModel* bookmarkModel_;  // weak; one per window

  // TODO(jrg): write a BookmarkView
  IBOutlet ToolbarView* /* BookmarkView* */ bookmarkView_;

  IBOutlet NSButton* backButton_;
  IBOutlet NSButton* forwardButton_;
  IBOutlet NSButton* reloadButton_;
  IBOutlet NSButton* starButton_;
  IBOutlet NSButton* goButton_;
  IBOutlet NSTextField* locationBar_;
  IBOutlet NSBox* contentsBox_;
  IBOutlet GrowBoxView* growBox_;

  // The contents box will have an offset if shrunk to make room for
  // the bookmark bar.
  BOOL contentsBoxHasOffset_;
}

// Create the contents of a tab represented by |contents| and loaded from the
// nib given by |name|. |commands| allows tracking of what's enabled and
// disabled. It may be nil if no updating is desired.
- (id)initWithNibName:(NSString*)name
               bundle:(NSBundle*)bundle
             contents:(TabContents*)contents
             commands:(CommandUpdater*)commands
         toolbarModel:(ToolbarModel*)toolbarModel
        bookmarkModel:(BookmarkModel*)bookmarkModel;

// Take this view (toolbar and web contents) full screen
- (IBAction)fullScreen:(id)sender;

// Get the C++ bridge object representing the location bar for this tab.
- (LocationBar*)locationBar;

// Called when the tab contents is about to be put into the view hierarchy as
// the selected tab. Handles things such as ensuring the toolbar is correctly
// enabled.
- (void)willBecomeSelectedTab;

// Called when the tab contents is updated in some non-descript way (the
// notification from the model isn't specific). |updatedContents| could reflect
// an entirely new tab contents object.
- (void)tabDidChange:(TabContents*)updatedContents;

// Called when any url bar state changes. If |tabForRestoring| is non-NULL,
// it points to a TabContents whose state we should restore.
- (void)updateToolbarWithContents:(TabContents*)tabForRestoring;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of this tab.
- (NSRect)growBoxRect;

// Called to update the loading state. Handles updating the go/stop button
// state.
- (void)setIsLoading:(BOOL)isLoading;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar;

// Change the visibility state of the bookmark bar.
- (void)toggleBookmarkBar:(BOOL)enable;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_COTNENTS_CONTROLLER_H_
