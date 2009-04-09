// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/command_observer_bridge.h"

class CommandUpdater;
class LocationBar;
class LocationBarViewMac;
class TabContents;
class ToolbarModel;
class ToolbarView;

// A controller for the toolbar in the browser window. Manages updating the
// state for location bar and back/fwd/reload/go buttons.

@interface ToolbarController : NSViewController<CommandObserverProtocol> {
 @private
  ToolbarModel* toolbarModel_;  // weak, one per window
  CommandUpdater* commands_;  // weak, one per window
  CommandObserverBridge* commandObserver_;
  LocationBarViewMac* locationBarView_;

  IBOutlet NSButton* backButton_;
  IBOutlet NSButton* forwardButton_;
  IBOutlet NSButton* reloadButton_;
  IBOutlet NSButton* starButton_;
  IBOutlet NSButton* goButton_;
  IBOutlet NSTextField* locationBar_;
}

// Initialize the toolbar and register for command updates.
- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands;

// Get the C++ bridge object representing the location bar for this tab.
- (LocationBar*)locationBar;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar;

// Called when any url bar state changes. If |tabForRestoring| is non-NULL,
// it points to a TabContents whose state we should restore.
- (void)updateToolbarWithContents:(TabContents*)tabForRestoring;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Called to update the loading state. Handles updating the go/stop button
// state.
- (void)setIsLoading:(BOOL)isLoading;

@end

#endif  // CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_
