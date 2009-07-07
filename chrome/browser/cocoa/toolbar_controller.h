// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/command_observer_bridge.h"
#include "chrome/common/pref_member.h"

class CommandUpdater;
class LocationBar;
class LocationBarViewMac;
namespace ToolbarControllerInternal {
class PrefObserverBridge;
}
class Profile;
class TabContents;
class ToolbarModel;
class ToolbarView;

// Field editor used for the location bar.
@interface LocationBarFieldEditor : NSTextView
// Copy contents of the TextView to the designated clipboard as plain text.
- (void)performCopy:(NSPasteboard*)pb;

// Same as above, note that this calls through to performCopy.
- (void)performCut:(NSPasteboard*)pb;
@end

// A controller for the toolbar in the browser window. Manages updating the
// state for location bar and back/fwd/reload/go buttons.

@interface ToolbarController : NSViewController<CommandObserverProtocol> {
 @private
  ToolbarModel* toolbarModel_;  // weak, one per window
  CommandUpdater* commands_;  // weak, one per window
  Profile* profile_;  // weak, one per window
  scoped_ptr<CommandObserverBridge> commandObserver_;
  scoped_ptr<LocationBarViewMac> locationBarView_;
  scoped_nsobject<LocationBarFieldEditor> locationBarFieldEditor_;  // strong

  // Used for monitoring the optional toolbar button prefs.
  scoped_ptr<ToolbarControllerInternal::PrefObserverBridge> prefObserver_;
  BooleanPrefMember showHomeButton_;
  BooleanPrefMember showPageOptionButtons_;

  IBOutlet NSMenu* pageMenu_;
  IBOutlet NSMenu* wrenchMenu_;

  // The ordering is important for unit tests. If new items are added or the
  // ordering is changed, make sure to update |-toolbarViews| and the
  // corresponding enum in the unit tests.
  IBOutlet NSButton* backButton_;
  IBOutlet NSButton* forwardButton_;
  IBOutlet NSButton* reloadButton_;
  IBOutlet NSButton* homeButton_;
  IBOutlet NSButton* starButton_;
  IBOutlet NSButton* goButton_;
  IBOutlet NSButton* pageButton_;
  IBOutlet NSButton* wrenchButton_;
  IBOutlet NSTextField* locationBar_;
}

// Initialize the toolbar and register for command updates. The profile is
// needed for initializing the location bar.
- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile;

// Get the C++ bridge object representing the location bar for this tab.
- (LocationBar*)locationBar;

// Called by the Window delegate so we can provide a custom field editor if
// needed.
// Note that this may be called for objects unrelated to the toolbar.
// returns nil if we don't want to override the custom field editor for |obj|.
- (id)customFieldEditorForObject:(id)obj;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar;

// Updates the toolbar (and transitively the location bar) with the states of
// the specified |tab|.  If |shouldRestore| is true, we're switching
// (back?) to this tab and should restore any previous location bar state
// (such as user editing) as well.
- (void)updateToolbarWithContents:(TabContents*)tabForRestoring
               shouldRestoreState:(BOOL)shouldRestore;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Called to update the loading state. Handles updating the go/stop button
// state.
- (void)setIsLoading:(BOOL)isLoading;

// Actions for the optional menu buttons for the page and wrench menus. These
// will show a menu while the mouse is down.
- (IBAction)showPageMenu:(id)sender;
- (IBAction)showWrenchMenu:(id)sender;

@end

// A set of private methods used by tests, in the absence of "friends" in ObjC.
@interface ToolbarController(PrivateTestMethods)
// Returns an array of views in the order of the outlets above.
- (NSArray*)toolbarViews;
- (void)showOptionalHomeButton;
- (void)showOptionalPageWrenchButtons;
@end

#endif  // CHROME_BROWSER_COCOA_TOOLBAR_CONTROLLER_H_
