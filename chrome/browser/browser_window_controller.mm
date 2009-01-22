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
    windowShim_ = new BrowserWindowCocoa(self, [self window]);
  }
  return self;
}

- (void)dealloc {
  browser_->CloseAllTabs();
  delete browser_;
  delete windowShim_;
  [super dealloc];
}

// Access the C++ bridge between the NSWindow and the rest of Chromium
- (BrowserWindow*)browserWindow {
  return windowShim_;
}

- (void)windowDidLoad {
  [urlBarView_ setStringValue:@"http://the.interwebs.start.here"];
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

// Called to validate menu and toolbar items when this window is key. All the
// items we care about have been set with the |commandDispatch:| action and
// a target of FirstResponder in IB. If it's not one of those, let it
// continue up the responder chain to be handled elsewhere. We pull out the
// tag as the cross-platform constant to differentiate and dispatch the
// various commands.
// NOTE: we might have to handle state for app-wide menu items,
// although we could cheat and directly ask the app controller if our
// command_updater doesn't support the command. This may or may not be an issue,
// too early to tell.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:)) {
    NSInteger tag = [item tag];
    if (browser_->command_updater()->SupportsCommand(tag))
      enable = browser_->command_updater()->IsCommandEnabled(tag) ? YES : NO;
  }
  return enable;
}

// Called when the user picks a menu or toolbar item when this window is key.
// Calls through to the browser object to execute the command. This assumes that
// the command is supported and doesn't check, otherwise it would have been
// disabled in the UI in validateUserInterfaceItem:.
- (void)commandDispatch:(id)sender {
  NSInteger tag = [sender tag];
  browser_->ExecuteCommand(tag);
}

@end
