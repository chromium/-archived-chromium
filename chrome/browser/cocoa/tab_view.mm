// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_view.h"

#include "chrome/browser/cocoa/tab_window_controller.h"

@implementation TabView

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // TODO(alcor): register for theming, either here or the cell
    // [self gtm_registerForThemeNotifications];
  }
  return self;
}

- (void)dealloc {
  // [self gtm_unregisterForThemeNotifications];
  [super dealloc];
}

// Overridden so that mouse clicks come to this view (the parent of the
// hierarchy) first. We want to handle clicks and drags in this class and
// leave the background button for display purposes only.
- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent {
  return YES;
}

// Determines which view a click in our frame actually hit. It's always this
// view, never a child.
// TODO(alcor): Figure out what to do with the close button. Are we using a
// NSButton for it, or drawing it ourselves with a cell?
- (NSView *)hitTest:(NSPoint)aPoint {
  if (NSPointInRect(aPoint, [self frame])) return self;
  return nil;
}

// Handle clicks and drags in this button. We get here because we have
// overridden acceptsFirstMouse: and the click is within our bounds.
// TODO(pinkerton/alcor): This routine needs *a lot* of work to marry Cole's
// ideas of dragging cocoa views between windows and how the Browser and
// TabStrip models want to manage tabs.
- (void)mouseDown:(NSEvent *)theEvent {
  // Make sure the controller doesn't go away while we're doing this.
  // TODO(pinkerton): cole had this, not sure why it's necessary.
  [[controller_ retain] autorelease];

  // Fire the action to select the tab.
  if ([[controller_ target] respondsToSelector:[controller_ action]])
    [[controller_ target] performSelector:[controller_ action]
                               withObject:self];

  // TODO(pinkerton): necessary to pre-arrange the tabs here?

  // Resolve overlay back to original window.
  NSWindow* sourceWindow = [self window];
  if ([sourceWindow isKindOfClass:[NSPanel class]]) {
    sourceWindow = [sourceWindow parentWindow];
  }
  TabWindowController* sourceController = [sourceWindow windowController];
  TabWindowController* draggedController = nil;
  TabWindowController* targetController = nil;

  // We don't want to "tear off" a tab if there's only one in the window. Treat
  // it like we're dragging around a tab we've already detached.
  BOOL isLastRemainingTab = [sourceController numberOfTabs] == 1;

  NSWindow* dragWindow = nil;
  NSWindow* dragOverlay = nil;
  BOOL dragging = YES;
  BOOL moved = NO;

  NSDate* targetDwellDate = nil; // The date this target was first chosen
  NSMutableArray* targets = [NSMutableArray array];

  NSPoint lastPoint =
      [[theEvent window] convertBaseToScreen:[theEvent locationInWindow]];

  while (dragging) {
    theEvent =
        [NSApp nextEventMatchingMask:NSLeftMouseUpMask | NSLeftMouseDraggedMask
                           untilDate:[NSDate distantFuture]
                              inMode:NSDefaultRunLoopMode dequeue:YES];
    NSPoint thisPoint = [NSEvent mouseLocation];

    // Find all the windows that could be a target. It has to be of the
    // appropriate class, and visible (obviously).
    if (![targets count]) {
      for (NSWindow* window in [NSApp windows]) {
        if (window == sourceWindow && isLastRemainingTab) continue;
        if (window == dragWindow) continue;
        if (![window isVisible]) continue;
        NSWindowController *controller = [window windowController];
        if ([controller isKindOfClass:[TabWindowController class]]) {
          [targets addObject:controller];
        }
      }
    }

    // Iterate over possible targets checking for the one the mouse is in.
    // The mouse can be in either the tab or window frame.
    TabWindowController* newTarget = nil;
    for (TabWindowController* target in targets) {
      NSRect windowFrame = [[target window] frame];
      if (NSPointInRect(thisPoint, windowFrame)) {
        if (NSPointInRect(thisPoint, [[target tabStripView] frame])) {
          newTarget = target;
        }
        break;
      }
    }

    // If we're now targeting a new window, re-layout the tabs in the old
    // target and reset how long we've been hovering over this new one.
    if (targetController != newTarget) {
      targetDwellDate = [NSDate date];
      [targetController arrangeTabs];
      targetController = newTarget;
    }

    NSEventType type = [theEvent type];
    if (type == NSLeftMouseDragged) {
      moved = YES;
      if (!draggedController) {
        if (isLastRemainingTab) {
          draggedController = sourceController;
          dragWindow = [draggedController window];
        } else {
          // Detach from the current window.
          // TODO(pinkerton): Create a new window, probably with
          // Browser::CreateNewStripWithContents(), but that requires that
          // we make some changes to return the new Browser object.
          draggedController = [sourceController detachTabToNewWindow:self];
          dragWindow = [draggedController window];

          // TODO(pinkerton): reconcile how the view moves from one window
          // to the other with the TabStrip model wanting to create the tabs.
          // [[sourceController tabController] removeTab:tab_];
          // [[sourceController tabController] addTab:tab_];
          [dragWindow setAlphaValue:0.0];
        }

        // Bring the target window to the front and make sure it has a border.
        [dragWindow setLevel:NSFloatingWindowLevel];
        [dragWindow orderFront:nil];
        [dragWindow makeMainWindow];
        [draggedController showOverlay];
        dragOverlay = [draggedController overlayWindow];

        if (![targets count])
          [dragOverlay setHasShadow:NO];
      } else {
        NSPoint origin = [dragWindow frame].origin;
        origin.x += thisPoint.x - lastPoint.x;
        origin.y += thisPoint.y - lastPoint.y;
        [dragWindow setFrameOrigin:NSMakePoint(origin.x, origin.y)];
      }

      // If we're not hovering over any window, make the window is fully
      // opaque. Otherwise, find where the tab might be dropped and insert
      // a placeholder so it appears like it's part of that window.
      if (!targetController) {
        [[dragWindow animator] setAlphaValue:1.0];
      } else {
        if (![[targetController window] isKeyWindow]) {
          // && ([targetDwellDate timeIntervalSinceNow] < -REQUIRED_DWELL)) {
          [[targetController window] makeKeyAndOrderFront:nil];
          [targets removeAllObjects];
          targetDwellDate = nil;
        }

        // Compute where placeholder should go and insert it into the
        // destination tab strip.
        NSRect dropTabFrame = [[targetController tabStripView] frame];
        NSPoint point =
            [sourceWindow convertBaseToScreen:
                [self convertPointToBase:NSZeroPoint]];
        int x = NSWidth([self bounds]) / 2 + point.x - dropTabFrame.origin.x;
        [targetController insertPlaceholderForTab:self atLocation:x];
        [targetController arrangeTabs];

        if (!targetController)
          [dragWindow makeKeyAndOrderFront:nil];
        [[dragWindow animator] setAlphaValue:targetController ? 0.0 : 0.333];

        [[[draggedController overlayWindow] animator]
            setAlphaValue:targetController ? 0.85 : 1.0];
        // [setAlphaValue:targetController ? 0.0 : 0.6];
      }
    } else if (type == NSLeftMouseUp) {
      // Mouse up, break out of the drag event tracking loop
      dragging = NO;
    }
    lastPoint = thisPoint;
  }  // while tracking mouse

  // The drag/click is done. If the user dragged the mouse, finalize the drag
  // and clean up.
  if (moved) {
    TabWindowController *dropController = targetController;
    if (dropController) {
#if 0
// TODO(alcor/pinkerton): hookup drops on existing windows
      NSRect adjustedFrame = [self bounds];
      NSRect dropTabFrame =  [[dropController tabStripView] frame];
      adjustedFrame.origin = [self convertPointToBase:NSZeroPoint];
      adjustedFrame.origin =
          [sourceWindow convertBaseToScreen:adjustedFrame.origin];
      adjustedFrame.origin.x = adjustedFrame.origin.x - dropTabFrame.origin.x;
//    adjustedFrame.origin.y = adjustedFrame.origin.y - dropTabFrame.origin.y;
//    adjustedFrame.size.height += adjustedFrame.origin.y;
      adjustedFrame.origin.y = 0;

      // TODO(alcor): get add tab stuff working
      // [dropController addTab:tab_];

      [self setFrame:adjustedFrame];
      [dropController arrangeTabs];
      [draggedController close];
      [dropController showWindow:nil];
#endif
    } else {
      [[dragWindow animator] setAlphaValue:1.0];
      [dragOverlay setHasShadow:NO];
      [draggedController removeOverlayAfterDelay:
          [[NSAnimationContext currentContext] duration]];
      [dragWindow makeKeyAndOrderFront:nil];

      [[draggedController window] setLevel:NSNormalWindowLevel];
      [draggedController arrangeTabs];
    }
    [sourceController arrangeTabs];
  }
}

@end
