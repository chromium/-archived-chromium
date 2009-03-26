// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_window_controller.h"

#include "base/logging.h"
#import "chrome/browser/cocoa/tab_strip_view.h"

@interface TabWindowController(PRIVATE)
- (void)setUseOverlay:(BOOL)useOverlay;
@end

@implementation TabWindowController
@synthesize tabStripView = tabStripView_;

- (void)windowDidLoad {
  // Place the tab bar above the content box and add it to the view hierarchy
  // as a sibling of the content view so it can overlap with the window frame.
  NSRect tabFrame = [contentBox_ frame];
  tabFrame.origin = NSMakePoint(0, NSMaxY(tabFrame));
  tabFrame.size.height = NSHeight([tabStripView_ frame]);
  [tabStripView_ setFrame:tabFrame];
  [[[[self window] contentView] superview] addSubview:tabStripView_];

  // tab switching will destroy the content area, so nil this out to ensure
  // that nobody tries to use it.
  contentBox_ = nil;
}

- (void)removeOverlay {
  [self setUseOverlay:NO];
}

- (void)removeOverlayAfterDelay:(NSTimeInterval)delay {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(removeOverlay)
                                             object:nil];
  [self performSelector:@selector(removeOverlay)
             withObject:nil
             afterDelay:delay];
}

- (void)showOverlay {
  [self setUseOverlay:YES];
}

// If |useOverlay| is YES, creates a new overlay window and puts the tab strip
// and the content area inside of it. This allows it to have a different opacity
// from the title bar. If NO, returns everything to the previous state and
// destroys the overlay window until it's needed again. The tab strip and window
// contents are returned to the original window.
- (void)setUseOverlay:(BOOL)useOverlay {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(removeOverlay)
                                             object:nil];
  if (useOverlay && !overlayWindow_) {
    DCHECK(!cachedContentView_);
    overlayWindow_ = [[NSPanel alloc] initWithContentRect:[[self window] frame]
                                                styleMask:NSBorderlessWindowMask
                                                  backing:NSBackingStoreBuffered
                                                    defer:YES];
    [overlayWindow_ setTitle:@"overlay"];
    [overlayWindow_ setBackgroundColor:[NSColor clearColor]];
    [overlayWindow_ setOpaque:NO];
    NSView *contentView = [overlayWindow_ contentView];
    [contentView addSubview:[self tabStripView]];
    cachedContentView_ = [[self window] contentView];
    [contentView addSubview:cachedContentView_];
    [overlayWindow_ setHasShadow:YES];
    [[self window] addChildWindow:overlayWindow_ ordered:NSWindowAbove];
    [overlayWindow_ orderFront:nil];
    [[self window] setHasShadow:NO];
  } else if (!useOverlay && overlayWindow_) {
    DCHECK(cachedContentView_);
    [[self window] setHasShadow:YES];
    [[self window] setContentView:cachedContentView_];
    [[cachedContentView_ superview] addSubview:[self tabStripView]];
    [[self window] makeFirstResponder:cachedContentView_];
    [[self window] display];
    [[self window] removeChildWindow:overlayWindow_];
    [overlayWindow_ orderOut:nil];
    [overlayWindow_ release];
    overlayWindow_ = nil;
    cachedContentView_ = nil;
  }
}

- (NSWindow*)overlayWindow {
  return overlayWindow_;
}

- (void)arrangeTabs {
  // subclass must implement
  NOTIMPLEMENTED();
}

- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView {
  // subclass must implement
  NOTIMPLEMENTED();
  return NULL;
}

- (void)insertPlaceholderForTab:(TabView*)tab atLocation:(NSInteger)xLocation {
  // subclass must implement
  NOTIMPLEMENTED();
}

- (void)removePlaceholder {
  // subclass must implement
  NOTIMPLEMENTED();
}

- (NSInteger)numberOfTabs {
  // subclass must implement
  NOTIMPLEMENTED();
  return 0;
}

@end
