// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"  // IDC_*
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/cocoa/tab_view.h"

@implementation BrowserWindowController

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|. Note that the nib also sets this controller
// up as the window's delegate.
- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithWindowNibName:@"BrowserWindow"])) {
    browser_ = browser;
    DCHECK(browser_);
    windowShim_ = new BrowserWindowCocoa(browser, self, [self window]);
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
  [super windowDidLoad];

  // Create a controller for the tab strip, giving it the model object for
  // this window's Browser and the tab strip view. The controller will handle
  // registering for the appropriate tab notifications from the back-end and
  // managing the creation of new tabs.
  tabStripController_ = [[TabStripController alloc]
                          initWithView:[self tabStripView] browser:browser_];
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

// Called right after our window became the main window.
- (void)windowDidBecomeMain:(NSNotification *)notification {
  BrowserList::SetLastActive(browser_);
}

// Update a toggle state for an NSMenuItem if modified.
// Take care to insure |item| looks like a NSMenuItem.
// Called by validateUserInterfaceItem:.
- (void)updateToggleStateWithTag:(NSInteger)tag forItem:(id)item {
  if (![item respondsToSelector:@selector(state)] ||
      ![item respondsToSelector:@selector(setState:)])
    return;

  // On Windows this logic happens in bookmark_bar_view.cc.  On the
  // Mac we're a lot more MVC happy so we've moved it into a
  // controller.  To be clear, this simply updates the menu item; it
  // does not display the bookmark bar itself.
  if (tag == IDC_SHOW_BOOKMARK_BAR) {
    bool toggled = windowShim_->IsBookmarkBarVisible();
    NSInteger oldState = [item state];
    NSInteger newState = toggled ? NSOnState : NSOffState;
    if (oldState != newState)
      [item setState:newState];
  }
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
    if (browser_->command_updater()->SupportsCommand(tag)) {
      // Generate return value (enabled state)
      enable = browser_->command_updater()->IsCommandEnabled(tag) ? YES : NO;

      // If the item is toggleable, find it's toggle state and
      // try to update it.  This is a little awkward, but the alternative is
      // to check after a commandDispatch, which seems worse.
      [self updateToggleStateWithTag:tag forItem:item];
    }
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

- (void)setIsLoading:(BOOL)isLoading {
  [tabStripController_ setIsLoading:isLoading];
}

// Called to start/stop the loading animations.
- (void)updateLoadingAnimations:(BOOL)animate {
  if (animate) {
    // TODO(pinkerton): determine what throbber animation is necessary and
    // start a timer to periodically update. Windows tells the tab strip to
    // do this. It uses a single timer to coalesce the multiple things that
    // could be updating. http://crbug.com/8281
  } else {
    // TODO(pinkerton): stop the timer.
  }
}

// Make the location bar the first responder, if possible.
- (void)focusLocationBar {
  [tabStripController_ focusLocationBar];
}

- (void)arrangeTabs {
  NOTIMPLEMENTED();
}

- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView {
  // Fetch the tab contents for the tab being dragged
  int index = [tabStripController_ indexForTabView:tabView];
  TabContents* contents = browser_->tabstrip_model()->GetTabContentsAt(index);

  // Set the window size. Need to do this before we detach the tab so it's
  // still in the window.
  NSRect windowRect = [[tabView window] frame];
  gfx::Rect browserRect(windowRect.origin.x, windowRect.origin.y,
                        windowRect.size.width, windowRect.size.height);

  // Create the new window with a single tab in its model, the one being
  // dragged.
  DockInfo dockInfo;
  Browser* newBrowser =
      browser_->tabstrip_model()->TearOffTabContents(contents,
                                                     browserRect,
                                                     dockInfo);

  // Get the new controller by asking the new window for its delegate.
  TabWindowController* controller =
      [newBrowser->window()->GetNativeHandle() delegate];
  DCHECK(controller && [controller isKindOfClass:[TabWindowController class]]);

  // Detach it from the source window, which just updates the model without
  // deleting the tab contents.
  browser_->tabstrip_model()->DetachTabContentsAt(index);

  return controller;
}

- (void)insertPlaceholderForTab:(TabView*)tab atLocation:(NSInteger)xLocation {
  NOTIMPLEMENTED();
}

- (void)removePlaceholder {
  NOTIMPLEMENTED();
}

- (BOOL)isBookmarkBarVisible {
  return [tabStripController_ isBookmarkBarVisible];

}

- (void)toggleBookmarkBar {
  [tabStripController_ toggleBookmarkBar];
}

- (NSInteger)numberOfTabs {
  return browser_->tabstrip_model()->count();
}

@end
