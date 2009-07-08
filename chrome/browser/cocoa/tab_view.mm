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

- (void)awakeFromNib {
  // Set up the tracking rect for the close button mouseover.  Add it
  // to the |closeButton_| view, but we'll handle the message ourself.
  // The mouseover is always enabled, because the close button works
  // regardless of key/main/active status.
  trackingArea_.reset(
      [[NSTrackingArea alloc] initWithRect:[closeButton_ bounds]
                                   options:NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveAlways
                                     owner:self
                                  userInfo:nil]);
  [closeButton_ addTrackingArea:trackingArea_.get()];
}

- (void)dealloc {
  // [self gtm_unregisterForThemeNotifications];
  [closeButton_ removeTrackingArea:trackingArea_.get()];
  [super dealloc];
}

// Overridden so that mouse clicks come to this view (the parent of the
// hierarchy) first. We want to handle clicks and drags in this class and
// leave the background button for display purposes only.
- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent {
  return YES;
}

- (void)mouseEntered:(NSEvent *)theEvent {
  // We only set up one tracking area, so we know any mouseEntered:
  // messages are for close button mouseovers.
  [closeButton_ setImage:[NSImage imageNamed:@"close_bar_h"]];
}

- (void)mouseExited:(NSEvent *)theEvent {
  // We only set up one tracking area, so we know any mouseExited:
  // messages are for close button mouseovers.
  [closeButton_ setImage:[NSImage imageNamed:@"close_bar"]];
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

static const CGFloat kTearDistance = 36.0;
static const NSTimeInterval kTearDuration = 0.333;
static const double kDragStartDistance = 3.0;

- (void)mouseDown:(NSEvent *)theEvent {
  // Fire the action to select the tab.
  if ([[controller_ target] respondsToSelector:[controller_ action]])
    [[controller_ target] performSelector:[controller_ action]
                               withObject:self];

  // Resolve overlay back to original window.
  sourceWindow_ = [self window];
  if ([sourceWindow_ isKindOfClass:[NSPanel class]]) {
    sourceWindow_ = [sourceWindow_ parentWindow];
  }

  sourceWindowFrame_ = [sourceWindow_ frame];
  sourceTabFrame_ = [self frame];
  sourceController_ = [sourceWindow_ windowController];
  draggedController_ = nil;
  dragWindow_ = nil;
  dragOverlay_ = nil;
  targetController_ = nil;
  tabWasDragged_ = NO;
  tearTime_ = 0.0;
  draggingWithinTabStrip_ = YES;

  // We don't want to "tear off" a tab if there's only one in the window. Treat
  // it like we're dragging around a tab we've already detached. Note that
  // unit tests might have |-numberOfTabs| reporting zero since the model
  // won't be fully hooked up. We need to be prepared for that and not send
  // them into the "magnetic" codepath.
  isTheOnlyTab_ = [sourceController_ numberOfTabs] <= 1;

  dragOrigin_ = [NSEvent mouseLocation];

  // Because we move views between windows, we need to handle the event loop
  // ourselves. Ideally we should use the standard event loop.
  while (1) {
    theEvent =
    [NSApp nextEventMatchingMask:NSLeftMouseUpMask | NSLeftMouseDraggedMask
                       untilDate:[NSDate distantFuture]
                          inMode:NSDefaultRunLoopMode dequeue:YES];
    NSPoint thisPoint = [NSEvent mouseLocation];

    NSEventType type = [theEvent type];
    if (type == NSLeftMouseDragged) {
      [self mouseDragged:theEvent];
    } else { // Mouse Up
      [self mouseUp:theEvent];
      break;
    }
  }
}

- (void)mouseDragged:(NSEvent *)theEvent {
  // First, go through the magnetic drag cycle. We break out of this if
  // "stretchiness" ever exceeds the a set amount.
  tabWasDragged_ = YES;

  if (isTheOnlyTab_) draggingWithinTabStrip_ = NO;
  if (draggingWithinTabStrip_) {
    NSRect frame = [self frame];
    NSPoint thisPoint = [NSEvent mouseLocation];
    CGFloat stretchiness = thisPoint.y - dragOrigin_.y;
    stretchiness = copysign(sqrtf(fabs(stretchiness))/sqrtf(kTearDistance),
                            stretchiness) / 2.0;
    CGFloat offset = thisPoint.x - dragOrigin_.x;
    if (fabsf(offset) > 100) stretchiness = 0;
    [sourceController_ insertPlaceholderForTab:self
                                         frame:NSOffsetRect(sourceTabFrame_,
                                                            offset, 0)
                                 yStretchiness:stretchiness];

    CGFloat tearForce = fabs(thisPoint.y - dragOrigin_.y);
    if (tearForce > kTearDistance) {
      draggingWithinTabStrip_ = NO;
      // When you finally leave the strip, we treat that as the origin.
      dragOrigin_.x = thisPoint.x;
    } else {
      return;
    }
  }

  NSPoint lastPoint =
    [[theEvent window] convertBaseToScreen:[theEvent locationInWindow]];

  // Do not start dragging until the user has "torn" the tab off by
  // moving more than 3 pixels.
  NSDate* targetDwellDate = nil; // The date this target was first chosen
  NSMutableArray* targets = [NSMutableArray array];

  NSPoint thisPoint = [NSEvent mouseLocation];

  // Find all the windows that could be a target. It has to be of the
  // appropriate class, and visible (obviously).
  if (![targets count]) {
    for (NSWindow* window in [NSApp windows]) {
      if (window == sourceWindow_ && isTheOnlyTab_) continue;
      if (window == dragWindow_) continue;
      if (![window isVisible]) continue;
      NSWindowController *controller = [window windowController];
      if ([controller isKindOfClass:[TabWindowController class]]) {
        TabWindowController* realController =
            static_cast<TabWindowController*>(controller);
        if ([realController canReceiveFrom:sourceController_]) {
          [targets addObject:controller];
        }
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
  if (targetController_ != newTarget) {
    targetDwellDate = [NSDate date];
    [targetController_ removePlaceholder];
    targetController_ = newTarget;
    if (!newTarget) {
      tearTime_ = [NSDate timeIntervalSinceReferenceDate];
      tearOrigin_ = [dragWindow_ frame].origin;
    }
  }

  // Create or identify the dragged controller.
  if (!draggedController_) {
    if (isTheOnlyTab_) {
      draggedController_ = sourceController_;
      dragWindow_ = [draggedController_ window];
    } else {
      // Detach from the current window and put it in a new window.
      draggedController_ = [sourceController_ detachTabToNewWindow:self];
      dragWindow_ = [draggedController_ window];
      [dragWindow_ setAlphaValue:0.0];
    }

    // Bring the target window to the front and make sure it has a border.
    [dragWindow_ setLevel:NSFloatingWindowLevel];
    [dragWindow_ orderFront:nil];
    [dragWindow_ makeMainWindow];
    [draggedController_ showOverlay];
    dragOverlay_ = [draggedController_ overlayWindow];
    //if (![targets count])
    //  [dragOverlay_ setHasShadow:NO];
    if (!isTheOnlyTab_) {
      tearTime_ = [NSDate timeIntervalSinceReferenceDate];
      tearOrigin_ = sourceWindowFrame_.origin;
    }
  }

  float tearProgress = [NSDate timeIntervalSinceReferenceDate] - tearTime_;
  tearProgress /= kTearDuration;
  tearProgress = sqrtf(MAX(MIN(tearProgress, 1.0), 0.0));

  // Move the dragged window to the right place on the screen.
  NSPoint origin = sourceWindowFrame_.origin;
  origin.x += (thisPoint.x - dragOrigin_.x);
  origin.y += (thisPoint.y - dragOrigin_.y);

  if (tearProgress < 1) {
    // If the tear animation is not complete, call back to ourself with the
    // same event to animate even if the mouse isn't moving.
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSelector:@selector(mouseDragged:)
               withObject:theEvent
               afterDelay:1.0f/30.0f];

    origin.x = (1 - tearProgress) *  tearOrigin_.x + tearProgress * origin.x;
    origin.y = (1 - tearProgress) * tearOrigin_.y + tearProgress * origin.y;
  }

  if (targetController_)
    origin.y = [[targetController_ window] frame].origin.y;
  [dragWindow_ setFrameOrigin:NSMakePoint(origin.x, origin.y)];

  // If we're not hovering over any window, make the window is fully
  // opaque. Otherwise, find where the tab might be dropped and insert
  // a placeholder so it appears like it's part of that window.
  if (targetController_) {
    if (![[targetController_ window] isKeyWindow]) {
      // && ([targetDwellDate timeIntervalSinceNow] < -REQUIRED_DWELL)) {
      [[targetController_ window] orderFront:nil];
      [targets removeAllObjects];
      targetDwellDate = nil;
    }

    // Compute where placeholder should go and insert it into the
    // destination tab strip.
    NSRect dropTabFrame = [[targetController_ tabStripView] frame];
    TabView *draggedTabView = (TabView *)[draggedController_ selectedTabView];
    NSRect tabFrame = [draggedTabView frame];
    tabFrame.origin = [dragWindow_ convertBaseToScreen:tabFrame.origin];
    tabFrame.origin = [[targetController_ window]
                        convertScreenToBase:tabFrame.origin];
    tabFrame = [[targetController_ tabStripView]
                convertRectFromBase:tabFrame];
    NSPoint point =
      [sourceWindow_ convertBaseToScreen:
       [draggedTabView convertPointToBase:NSZeroPoint]];
    [targetController_ insertPlaceholderForTab:self
                                         frame:tabFrame
                                 yStretchiness:0];
    [targetController_ layoutTabs];
  } else {
    [dragWindow_ makeKeyAndOrderFront:nil];
  }
  BOOL chromeShouldBeVisible = targetController_ == nil;

  if (chromeIsVisible_ != chromeShouldBeVisible) {
    [dragWindow_ setHasShadow:YES];
    if (targetController_) {
      [NSAnimationContext beginGrouping];
      [[NSAnimationContext currentContext] setDuration:0.00001];
      [[dragWindow_ animator] setAlphaValue:0.0];
      [NSAnimationContext endGrouping];
      [[draggedController_ overlayWindow] setHasShadow:YES];
      [[[draggedController_ overlayWindow] animator] setAlphaValue:1.0];
    } else {
      [[draggedController_ overlayWindow] setAlphaValue:1.0];
      [[draggedController_ overlayWindow] setHasShadow:NO];
      [[dragWindow_ animator] setAlphaValue:0.5];
    }
    chromeIsVisible_ = chromeShouldBeVisible;
  }
}

- (void)mouseUp:(NSEvent *)theEvent {
  // The drag/click is done. If the user dragged the mouse, finalize the drag
  // and clean up.

  if (draggingWithinTabStrip_) {
    if (tabWasDragged_) {
      // Move tab to new location.
      TabWindowController* dropController = sourceController_;
      [dropController moveTabView:[dropController selectedTabView]
                   fromController:nil];
    }
  } else if (targetController_) {
    // Move between windows. If |targetController_| is nil, we're not dropping
    // into any existing window.
    NSView* draggedTabView = [draggedController_ selectedTabView];
    [draggedController_ removeOverlay];
    [targetController_ moveTabView:draggedTabView
                    fromController:draggedController_];
    [targetController_ showWindow:nil];
  } else {
    [dragWindow_ setAlphaValue:1.0];
    [dragOverlay_ setHasShadow:NO];
    [dragWindow_ setHasShadow:YES];
    [draggedController_ removeOverlay];
    [dragWindow_ makeKeyAndOrderFront:nil];

    [[draggedController_ window] setLevel:NSNormalWindowLevel];
    [draggedController_ removePlaceholder];
    [draggedController_ layoutTabs];
  }
  [sourceController_ removePlaceholder];
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  // Support middle-click-to-close.
  if ([theEvent buttonNumber] == 2) {
    [controller_ closeTab:self];
  }
}

// Called when the user hits the right mouse button (or control-clicks) to
// show a context menu.
- (void)rightMouseDown:(NSEvent*)theEvent {
  [NSMenu popUpContextMenu:[self menu] withEvent:theEvent forView:self];
}

@end
