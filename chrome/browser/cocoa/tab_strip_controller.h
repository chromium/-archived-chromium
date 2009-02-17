// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

class CommandUpdater;
class LocationBar;
@class TabStripView;
class TabStripBridge;
class TabStripModel;
class TabContents;

// A class that handles managing the tab strip in a browser window. It uses
// a supporting C++ bridge object to register for notifications from the
// TabStripModel. The Obj-C part of this class handles drag and drop and all
// the other Cocoa-y aspects.
//
// When a new tab is created, it loads the contents, including
// toolbar, from a separate nib file and replaces the contentView of the
// window. As tabs are switched, the single child of the contentView is
// swapped around to hold the contents (toolbar and all) representing that tab.

@interface TabStripController : NSObject {
 @private
  TabContents* currentTab_;   // weak, tab for which we're showing state
  TabStripView* tabView_;  // weak
  NSButton* newTabButton_;
  TabStripBridge* bridge_;
  TabStripModel* model_;  // weak
  CommandUpdater* commands_;  // weak, may be nil
  // maps TabContents to a TabContentsController (which owns the parent view
  // for the toolbar and associated tab contents)
  NSMutableDictionary* tabContentsToController_;
}

// Initialize the controller with a view, model, and command updater for
// tracking what's enabled and disabled. |commands| may be nil if no updating
// is desired.
- (id)initWithView:(TabStripView*)view 
             model:(TabStripModel*)model
          commands:(CommandUpdater*)commands;

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

@end

#endif  // CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
