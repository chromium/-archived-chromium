// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/browser.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"

@implementation BrowserWindowController

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|. Note that the nib also sets this controller
// up as the window's delegate.
- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithWindowNibName:@"BrowserWindow"])) {
    browser_ = browser;
    DCHECK(browser_);
    windowShim_ = new BrowserWindowCocoa(self, [self window]);
  }
  return self;
}

- (void)dealloc {
  browser_->CloseAllTabs();
  [tabStripController_ release];
  delete browser_;
  delete windowShim_;
  [super dealloc];
}

// Access the C++ bridge between the NSWindow and the rest of Chromium
- (BrowserWindow*)browserWindow {
  return windowShim_;
}

- (void)windowDidLoad {
  // Create a controller for the tab strip, giving it the model object for
  // this window's Browser and the tab strip view. The controller will handle
  // registering for the appropriate tab notifications from the back-end and 
  // managing the creation of new tabs.
  tabStripController_ = 
      [[TabStripController alloc]
          initWithView:tabStripView_ 
              tabModel:browser_->tabstrip_model()
          toolbarModel:browser_->toolbar_model()
              commands:browser_->command_updater()];

  // Place the tab bar above the content box and add it to the view hierarchy
  // as a sibling of the content view so it can overlap with the window frame.
  NSRect tabFrame = [contentBox_ frame];
  tabFrame.origin = NSMakePoint(0, NSMaxY(tabFrame));
  tabFrame.size.height = NSHeight([tabStripView_ frame]);
  [tabStripView_ setFrame:tabFrame];
  [[[[self window] contentView] superview] addSubview:tabStripView_];
}

- (void)destroyBrowser {
  // We need the window to go away now.
  [self autorelease];
}

// Called when the window meets the criteria to be closed (ie,
// |-windowShoudlClose:| returns YES). We must be careful to preserve the
// semantics of BrowserWindow::Close() and not call the Browser's dtor directly
// from this method.
- (void)windowWillClose:(NSNotification *)notification {
  DCHECK(!browser_->tabstrip_model()->count());
  
  // We can't acutally use |-autorelease| here because there's an embedded
  // run loop in the |-performClose:| which contains its own autorelease pool.
  // Instead we use call it after a zero-length delay, which gets us back
  // to the main event loop.
  [self performSelector:@selector(autorelease)
              withObject:nil
              afterDelay:0];
}

// Called when the user wants to close a window or from the shutdown process.
// The Browser object is in control of whether or not we're allowed to close. It
// may defer closing due to several states, such as onUnload handlers needing to
// be fired. If closing is deferred, the Browser will handle the processing
// required to get us to the closing state and (by watching for all the tabs
// going away) will again call to close the window when it's finally ready.
- (BOOL)windowShouldClose:(id)sender {
  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return NO;

  if (!browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    [[self window] orderOut:self];
    browser_->OnWindowClosing();
    return NO;
  }

  // the tab strip is empty, it's ok to close the window
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

- (LocationBar*)locationBar {
  return [tabStripController_ locationBar];
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  [tabStripController_ updateToolbarWithContents:tab
                                 shouldRestoreState:shouldRestore];
}

- (void)setStarredState:(BOOL)isStarred {
  [tabStripController_ setStarredState:isStarred];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
// |windowGrowBox| needs to be in the window's coordinate system.
- (NSRect)selectedTabGrowBoxRect {
  return [tabStripController_ 
              selectedTabGrowBoxRect];
}

@end
