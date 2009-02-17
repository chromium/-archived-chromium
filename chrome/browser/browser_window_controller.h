// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_BROWSER_WINDOW_CONTROLLER_H_

// A class acting as the Objective-C controller for the Browser object. Handles
// interactions between Cocoa and the cross-platform code.

#import <Cocoa/Cocoa.h>

class Browser;
class BrowserWindow;
@class TabStripView;
@class TabContentsController;
@class TabStripController;

@interface BrowserWindowController :
    NSWindowController<NSUserInterfaceValidations> {
 @private
  Browser* browser_;
  BrowserWindow* windowShim_;
  TabStripController* tabStripController_;

  IBOutlet NSBox* contentBox_;
  IBOutlet TabStripView* tabStripView_;

  // Views for the toolbar
  IBOutlet NSView* toolbarView_;
  IBOutlet NSTextField* urlBarView_;
}

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|.
- (id)initWithBrowser:(Browser*)browser;

// call to make the browser go away from other places in the cross-platform
// code.
- (void)destroyBrowser;

// Access the C++ bridge between the NSWindow and the rest of Chromium
- (BrowserWindow*)browserWindow;

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

#endif  // CHROME_BROWSER_BROWSER_WINDOW_CONTROLLER_H_
