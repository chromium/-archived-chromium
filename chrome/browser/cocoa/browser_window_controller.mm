// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"  // IDC_*
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/web_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/status_bubble_mac.h"
#import "chrome/browser/cocoa/tab_strip_model_observer_bridge.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/cocoa/tab_view.h"
#import "chrome/browser/cocoa/toolbar_controller.h"

namespace {

// Size of the gradient. Empirically determined so that the gradient looks
// like what the heuristic does when there are just a few tabs.
const int kWindowGradientHeight = 24;

}

@interface BrowserWindowController(Private)

- (void)positionToolbar;

// Leopard's gradient heuristic gets confused by our tabs and makes the title
// gradient jump when creating a tab that is less than a tab width from the
// right side of the screen. This function disables Leopard's gradient
// heuristic.
- (void)fixWindowGradient;

@end


@implementation BrowserWindowController

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|. Note that the nib also sets this controller
// up as the window's delegate.
- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithWindowNibName:@"BrowserWindow"])) {
    DCHECK(browser);
    browser_.reset(browser);
    tabObserver_.reset(
        new TabStripModelObserverBridge(browser->tabstrip_model(), self));
    windowShim_.reset(new BrowserWindowCocoa(browser, self, [self window]));

    // The window is now fully realized and |-windowDidLoad:| has been
    // called. We shouldn't do much in wDL because |windowShim_| won't yet
    // be initialized (as it's called in response to |[self window]| above).
    // Retain it per the comment in the header.
    window_.reset([[self window] retain]);

    // Get the most appropriate size for the window. The window shim will handle
    // flipping the coordinates for us so we can use it to save some code.
    gfx::Rect windowRect = browser_->GetSavedWindowBounds();
    windowShim_->SetBounds(windowRect);

    // Create a controller for the tab strip, giving it the model object for
    // this window's Browser and the tab strip view. The controller will handle
    // registering for the appropriate tab notifications from the back-end and
    // managing the creation of new tabs.
    tabStripController_.reset([[TabStripController alloc]
                                initWithView:[self tabStripView]
                                  switchView:[self tabContentArea]
                                     browser:browser_.get()]);

    // Create a controller for the toolbar, giving it the toolbar model object
    // and the toolbar view from the nib. The controller will handle
    // registering for the appropriate command state changes from the back-end.
    toolbarController_.reset([[ToolbarController alloc]
                               initWithModel:browser->toolbar_model()
                                    commands:browser->command_updater()]);
    [self positionToolbar];
    [self fixWindowGradient];

    // Create the bridge for the status bubble.
    statusBubble_.reset(new StatusBubbleMac([self window]));
  }
  return self;
}

- (void)dealloc {
  browser_->CloseAllTabs();
  [super dealloc];
}

// Access the C++ bridge between the NSWindow and the rest of Chromium
- (BrowserWindow*)browserWindow {
  return windowShim_.get();
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
  BrowserList::SetLastActive(browser_.get());
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
  return [toolbarController_ locationBar];
}

- (StatusBubble*)statusBubble {
  return statusBubble_.get();
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  [toolbarController_ updateToolbarWithContents:shouldRestore ? tab : NULL];
}

- (void)setStarredState:(BOOL)isStarred {
  [toolbarController_ setStarredState:isStarred];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
// |windowGrowBox| needs to be in the window's coordinate system.
- (NSRect)selectedTabGrowBoxRect {
  return [tabStripController_ selectedTabGrowBoxRect];
}

- (void)setIsLoading:(BOOL)isLoading {
  [toolbarController_ setIsLoading:isLoading];
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
  [toolbarController_ focusLocationBar];
}

- (void)arrangeTabs {
  NOTIMPLEMENTED();
}

- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView {
  // Fetch the tab contents for the tab being dragged
  int index = [tabStripController_ indexForTabView:tabView];
  TabContents* contents = browser_->tabstrip_model()->GetTabContentsAt(index);

  // Set the window size. Need to do this before we detach the tab so it's
  // still in the window. We have to flip the coordinates as that's what
  // is expected by the Browser code.
  NSWindow* sourceWindow = [tabView window];
  NSRect windowRect = [sourceWindow frame];
  NSScreen* screen = [sourceWindow screen];
  windowRect.origin.y =
      [screen frame].size.height - windowRect.size.height -
          windowRect.origin.y;
  gfx::Rect browserRect(windowRect.origin.x, windowRect.origin.y,
                        windowRect.size.width, windowRect.size.height);

  // Detach it from the source window, which just updates the model without
  // deleting the tab contents. This needs to come before creating the new
  // Browser because it clears the TabContents' delegate, which gets hooked
  // up during creation of the new window.
  browser_->tabstrip_model()->DetachTabContentsAt(index);

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

- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture {
  DCHECK(oldContents != newContents);

  // We do not store the focus when closing the tab to work-around bug 4633.
  // Some reports seem to show that the focus manager and/or focused view can
  // be garbage at that point, it is not clear why.
  if (oldContents && !oldContents->is_being_destroyed() &&
      oldContents->AsWebContents())
    oldContents->AsWebContents()->view()->StoreFocus();

  // Update various elements that are interested in knowing the current
  // TabContents.
#if 0
// TODO(pinkerton):Update as more things become window-specific
  infobar_container_->ChangeTabContents(new_contents);
  contents_container_->SetTabContents(new_contents);
#endif
  newContents->DidBecomeSelected();

  if (BrowserList::GetLastActive() == browser_ &&
      !browser_->tabstrip_model()->closing_all() &&
      newContents->AsWebContents()) {
    newContents->AsWebContents()->view()->RestoreFocus();
  }

#if 0
// TODO(pinkerton):Update as more things become window-specific
  // Update all the UI bits.
  UpdateTitleBar();
  toolbar_->SetProfile(new_contents->profile());
  UpdateToolbar(new_contents, true);
  UpdateUIForContents(new_contents);
#endif
}

@end


@interface NSWindow (NSPrivateApis)
// Note: These functions are private, use -[NSObject respondsToSelector:]
// before calling them.

- (void)setAutorecalculatesContentBorderThickness:(BOOL)b
                                          forEdge:(NSRectEdge)e;
- (void)setContentBorderThickness:(CGFloat)b forEdge:(NSRectEdge)e;
@end


@implementation BrowserWindowController (Private)

// Position |toolbarView_| below the tab strip, but not as a sibling. The
// toolbar is part of the window's contentView, mainly because we want the
// opacity during drags to be the same as the web content.
- (void)positionToolbar {
  NSView* contentView = [self tabContentArea];
  NSRect contentFrame = [contentView frame];
  NSView* toolbarView = [toolbarController_ view];
  NSRect toolbarFrame = [toolbarView frame];

  // Shrink the content area by the height of the toolbar.
  contentFrame.size.height -= toolbarFrame.size.height;
  [contentView setFrame:contentFrame];

  // Move the toolbar above the content area, within the window's content view
  // (as opposed to the tab strip, which is a sibling).
  toolbarFrame.origin.y = NSMaxY(contentFrame);
  toolbarFrame.origin.x = 0;
  toolbarFrame.size.width = contentFrame.size.width;
  [toolbarView setFrame:toolbarFrame];
  [[[self window] contentView] addSubview:toolbarView];
}

- (void)fixWindowGradient {
  NSWindow* win = [self window];
  if ([win respondsToSelector:@selector(
          setAutorecalculatesContentBorderThickness:forEdge:)] &&
      [win respondsToSelector:@selector(
           setContentBorderThickness:forEdge:)]) {
    [win setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
    [win setContentBorderThickness:kWindowGradientHeight forEdge:NSMaxYEdge];

    [win setAutorecalculatesContentBorderThickness:NO forEdge:NSMinYEdge];
    [win setContentBorderThickness:kWindowGradientHeight forEdge:NSMinYEdge];
  }
}

@end
