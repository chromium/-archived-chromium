// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/browser.h"
#import "chrome/browser/browser_window_cocoa.h"
#import "chrome/browser/browser_window_controller.h"

@implementation BrowserWindowController

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|. Note that the nib also sets this controller
// up as the window's delegate.
- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithWindowNibName:@"BrowserWindow"])) {
    browser_ = browser;
    window_shim_ = new BrowserWindowCocoa(self, [self window]);
  }
  return self;
}

- (void)dealloc {
  browser_->CloseAllTabs();
  delete browser_;
  delete window_shim_;
  [super dealloc];
}

// Access the C++ bridge between the NSWindow and the rest of Chromium
- (BrowserWindow*)browserWindow {
  return window_shim_;
}

- (void)windowDidLoad {
  [(NSControl*)[url_bar_ view]
      setStringValue:@"http://the.interwebs.start.here"];
}

- (void)destroyBrowser {
  // we need the window to go away now, other areas of code will be checking
  // the number of browser objects remaining after we finish so we can't defer
  // deletion via autorelease.
  [self autorelease];
}

// Called when the window is closing from Cocoa. Destroy this controller,
// which will tear down the rest of the infrastructure as the Browser is
// itself destroyed.
- (void)windowWillClose:(NSNotification *)notification {
  [self autorelease];
}

// Called when the user wants to close a window. Usually it's ok, but we may
// want to prompt the user when they have multiple tabs open, for example.
- (BOOL)windowShouldClose:(id)sender {
  // TODO(pinkerton): check tab model to see if it's ok to close the 
  // window. Use NSGetAlertPanel() and runModalForWindow:.
  return YES;
}

// NSToolbar delegate methods

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:[back_button_ itemIdentifier],
                                   [forward_button_ itemIdentifier],
                                   [url_bar_ itemIdentifier], nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
  return [NSArray arrayWithObjects:[back_button_ itemIdentifier],
                                   [forward_button_ itemIdentifier],
                                   [url_bar_ itemIdentifier], nil];
}

@end
