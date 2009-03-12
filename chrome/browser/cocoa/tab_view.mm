// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_view.h"

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
- (void)mouseDown:(NSEvent *)theEvent {
  // fire the action to select the tab
  if ([[controller_ target] respondsToSelector:[controller_ action]])
    [[controller_ target] performSelector:[controller_ action]
                               withObject:self];

  // TODO(alcor): handle dragging...
}

@end
