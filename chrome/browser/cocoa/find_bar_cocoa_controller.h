// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"

#include "base/string16.h"

class BrowserWindowCocoa;
class FindBarBridge;
class FindNotificationDetails;

// A controller for the find bar in the browser window.  Manages
// updating the state of the find bar and provides a target for the
// next/previous/close buttons.  Certain operations require a pointer
// to the cross-platform FindBarController, so be sure to call
// setFindBarBridge: after creating this controller.

@interface FindBarCocoaController : NSViewController {
 @private
  IBOutlet NSTextField* findText_;
  IBOutlet NSTextField* resultsLabel_;
  IBOutlet NSButton* nextButton_;
  IBOutlet NSButton* previousButton_;

  // Needed to call methods on FindBarController.
  FindBarBridge* findBarBridge_;  // weak
};

// Initializes a new FindBarCocoaController.
- (id)init;

- (void)setFindBarBridge:(FindBarBridge*)findBar;

- (IBAction)close:(id)sender;

- (IBAction)nextResult:(id)sender;

- (IBAction)previousResult:(id)sender;

// Positions the find bar based on the location of |contentArea|.
- (void)positionFindBarView:(NSView*)contentArea;

// Methods called from FindBarBridge.
- (void)showFindBar;
- (void)hideFindBar;
- (void)setFocusAndSelection;
- (void)setFindText:(const string16&)findText;

- (void)clearResults:(const FindNotificationDetails&)results;
- (void)updateUIForFindResult:(const FindNotificationDetails&)results
                     withText:(const string16&)findText;
- (BOOL)isFindBarVisible;

@end
