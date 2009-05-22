// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_controller.h"
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

// Determines which view a click in our frame actually hit. It's either this
// view or our child close button.
- (NSView *)hitTest:(NSPoint)aPoint {
  NSPoint viewPoint = [self convertPoint:aPoint fromView:[self superview]];
  NSRect frame = [self frame];

  // Reduce the width of the hit rect slightly to remove the overlap
  // between adjacent tabs.  The drawing code in TabCell has the top
  // corners of the tab inset by height*2/3, so we inset by half of
  // that here.  This doesn't completely eliminate the overlap, but it
  // works well enough.
  NSRect hitRect = NSInsetRect(frame, frame.size.height / 3.0f, 0);
  if (NSPointInRect(viewPoint, [closeButton_ frame])) return closeButton_;
  if (NSPointInRect(aPoint, hitRect)) return self;
  return nil;
}

// Handle clicks and drags in this button. We get here because we have
// overridden acceptsFirstMouse: and the click is within our bounds.
// TODO(pinkerton/alcor): This routine needs *a lot* of work to marry Cole's
// ideas of dragging cocoa views between windows and how the Browser and
// TabStrip models want to manage tabs.
- (void)mouseDown:(NSEvent *)theEvent {
  static const CGFloat kTearDistance = 36.0;

  // Make sure the controller doesn't go away while we're doing this.
  // TODO(pinkerton): cole had this, not sure why it's necessary.
  [[controller_ retain] autorelease];

  // Fire the action to select the tab.
  if ([[controller_ target] respondsToSelector:[controller_ action]])
    [[controller_ target] performSelector:[controller_ action]
                               withObject:self];

  // Resolve overlay back to original window.
  NSWindow* sourceWindow = [self window];
  if ([sourceWindow isKindOfClass:[NSPanel class]]) {
    sourceWindow = [sourceWindow parentWindow];
  }

  TabWindowController* sourceController = [sourceWindow windowController];

  // We don't want to "tear off" a tab if there's only one in the window. Treat
  // it like we're dragging around a tab we've already detached. Note that
  // unit tests might have |-numberOfTabs| reporting zero since the model
  // won't be fully hooked up. We need to be prepared for that and not send
  // them into the "magnetic" codepath.
  BOOL isLastRemainingTab = [sourceController numberOfTabs] <= 1;

  BOOL dragging = YES;
  BOOL moveBetweenWindows = NO;

  NSPoint lastPoint =
    [[theEvent window] convertBaseToScreen:[theEvent locationInWindow]];

  // First, go through the magnetic drag cycle. We break out of this if
  // "stretchiness" ever exceeds the a set amount.
  NSRect frame = [self frame];
  if (!isLastRemainingTab) {
    while (dragging) {
      theEvent =
      [NSApp nextEventMatchingMask:NSLeftMouseUpMask | NSLeftMouseDraggedMask
                         untilDate:[NSDate distantFuture]
                            inMode:NSDefaultRunLoopMode dequeue:YES];
      NSPoint thisPoint = [NSEvent mouseLocation];
      CGFloat stretchiness = thisPoint.y - lastPoint.y;
      stretchiness = copysign(sqrtf(fabs(stretchiness))/sqrtf(kTearDistance),
                              stretchiness);
      [sourceController insertPlaceholderForTab:self
                                          frame:NSOffsetRect(frame,
                                                   thisPoint.x - lastPoint.x, 0)
                                  yStretchiness:stretchiness];

      CGFloat tearForce = fabs(thisPoint.y - lastPoint.y);
      if (tearForce > kTearDistance) break;
      if ([theEvent type] == NSLeftMouseUp) {
        // Mouse up, break out of the drag event tracking loop
        dragging = NO;
        break;
      }
    }
  }

  if (dragging)
    [sourceController removePlaceholder];

  TabWindowController* draggedController = nil;
  TabWindowController* targetController = nil;

  NSWindow* dragWindow = nil;
  NSWindow* dragOverlay = nil;

  // Do not start dragging until the user has "torn" the tab off by
  // moving more than 3 pixels.
  BOOL torn = NO;
  static const double kDragStartDistance = 3.0;

  NSDate* targetDwellDate = nil; // The date this target was first chosen
  NSMutableArray* targets = [NSMutableArray array];

  while (dragging) {
    theEvent =
        [NSApp nextEventMatchingMask:NSLeftMouseUpMask | NSLeftMouseDraggedMask
                           untilDate:[NSDate distantFuture]
                              inMode:NSDefaultRunLoopMode dequeue:YES];
    NSPoint thisPoint = [NSEvent mouseLocation];

    if (!torn) {
      double dx = thisPoint.x - lastPoint.x;
      double dy = thisPoint.y - lastPoint.y;

      if (dx * dx + dy * dy < kDragStartDistance * kDragStartDistance
          && [theEvent type] == NSLeftMouseDragged) {
        continue;
      }
      torn = YES;
    }

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
        NSRect tabStripFrame = [[target tabStripView] frame];
        tabStripFrame = [[target tabStripView] convertRectToBase:tabStripFrame];
        tabStripFrame.origin = [[target window]
                                 convertBaseToScreen:tabStripFrame.origin];
        if (NSPointInRect(thisPoint, tabStripFrame)) {
          newTarget = target;
        }
        break;
      }
    }

    // If we're now targeting a new window, re-layout the tabs in the old
    // target and reset how long we've been hovering over this new one.
    if (targetController != newTarget) {
      targetDwellDate = [NSDate date];
      [targetController removePlaceholder];
      targetController = newTarget;
    }

    NSEventType type = [theEvent type];
    if (type == NSLeftMouseDragged) {
      moveBetweenWindows = YES;
      if (!draggedController) {
        if (isLastRemainingTab) {
          draggedController = sourceController;
          dragWindow = [draggedController window];
        } else {
          // Detach from the current window and put it in a new window.
          draggedController = [sourceController detachTabToNewWindow:self];
          dragWindow = [draggedController window];
          [dragWindow setAlphaValue:0.0];
        }

        // Bring the target window to the front and make sure it has a border.
        [dragWindow setLevel:NSFloatingWindowLevel];
        [dragWindow orderFront:nil];
        [dragWindow makeMainWindow];
        [draggedController showOverlay];
        dragOverlay = [draggedController overlayWindow];

        NSPoint origin = [sourceWindow frame].origin;

        origin.y += thisPoint.y - lastPoint.y;
        [dragWindow setFrameOrigin:NSMakePoint(origin.x, origin.y)];
        //if (![targets count])
        //  [dragOverlay setHasShadow:NO];
      } else {
        NSPoint origin = [dragWindow frame].origin;
        origin.x += thisPoint.x - lastPoint.x;
        origin.y += thisPoint.y - lastPoint.y;
        [dragWindow setFrameOrigin:NSMakePoint(origin.x, origin.y)];
      }

      // If we're not hovering over any window, make the window is fully
      // opaque. Otherwise, find where the tab might be dropped and insert
      // a placeholder so it appears like it's part of that window.
      if (targetController) {
        if (![[targetController window] isKeyWindow]) {
          // && ([targetDwellDate timeIntervalSinceNow] < -REQUIRED_DWELL)) {
          [[targetController window] makeKeyAndOrderFront:nil];
          [targets removeAllObjects];
          targetDwellDate = nil;
        }

        // Compute where placeholder should go and insert it into the
        // destination tab strip.
        NSRect dropTabFrame = [[targetController tabStripView] frame];
        NSView *draggedTabView = [draggedController selectedTabView];
        NSRect tabFrame = [draggedTabView frame];
        tabFrame = [draggedTabView convertRectToBase:[self bounds]];
        tabFrame.origin = [dragWindow
                            convertBaseToScreen:tabFrame.origin];
        tabFrame.origin = [[targetController window]
                            convertScreenToBase:tabFrame.origin];
        tabFrame = [[targetController tabStripView]
                     convertRectFromBase:tabFrame];
        NSPoint point =
            [sourceWindow convertBaseToScreen:
                [self convertPointToBase:NSZeroPoint]];
        [targetController insertPlaceholderForTab:self
                                            frame:tabFrame
                                    yStretchiness:0];
        [targetController layoutTabs];
      } else {
        [dragWindow makeKeyAndOrderFront:nil];
      }

      [dragWindow setHasShadow:NO];
      [dragWindow setAlphaValue:targetController ? 0.1 : 0.5];
      [[draggedController overlayWindow]
       setAlphaValue:targetController ? 0.85 : 1.0];
    } else if (type == NSLeftMouseUp) {
      // Mouse up, break out of the drag event tracking loop
      dragging = NO;
    }
    lastPoint = thisPoint;
  }  // while tracking mouse

  // The drag/click is done. If the user dragged the mouse, finalize the drag
  // and clean up.
  if (moveBetweenWindows) {
    // Move between windows. If |targetController| is nil, we're not dropping
    // into any existing window.
    TabWindowController* dropController = targetController;
    if (dropController) {
      NSView* draggedTabView = [draggedController selectedTabView];
      [draggedController removeOverlay];
      [dropController moveTabView:draggedTabView
                   fromController:draggedController];
      [dropController showWindow:nil];
    } else {
      [targetController removePlaceholder];
      [[dragWindow animator] setAlphaValue:1.0];
      [dragOverlay setHasShadow:NO];
      [dragWindow setHasShadow:YES];
      [draggedController removeOverlay];
      [dragWindow makeKeyAndOrderFront:nil];

      [[draggedController window] setLevel:NSNormalWindowLevel];
      [draggedController layoutTabs];
    }
    [sourceController layoutTabs];
  } else {
    // Move or click within a window. We need to differentiate between a
    // click on the tab and a drag by checking against the initial x position.
    NSPoint currentPoint = [NSEvent mouseLocation];
    BOOL wasDrag = fabs(currentPoint.x - lastPoint.x) > kDragStartDistance;
    if (wasDrag) {
      // Move tab to new location.
      TabWindowController* dropController = sourceController;
      [dropController moveTabView:[dropController selectedTabView]
                   fromController:nil];
    }
  }

  [sourceController removePlaceholder];
}

- (void)otherMouseUp:(NSEvent*) theEvent {
  // Support middle-click-to-close.
  if ([theEvent buttonNumber] == 2) {
    [controller_ closeTab:self];
  }
}

@end
