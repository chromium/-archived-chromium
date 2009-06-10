// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/grow_box_view.h"

@interface GrowBoxView (PrivateMethods)

// Computes the area in which the resized window can grow.  This is
// the union of the current window frame and the work area of each
// screen.
// NOTE: If the lower right corner of the window is on a screen but
// outside its work area, we use the full area of that screen instead.
// The net effect is to closely mimic the resize behavior of other
// apps: the window is prevented from growing outside the work area,
// unless it is outside the work area to begin with.
- (void)computeClipRect;

@end

@implementation GrowBoxView

- (void)awakeFromNib {
  image_.reset([[NSImage imageNamed:@"grow_box"] retain]);
}

- (void)dealloc {
  [super dealloc];
}

// Draws the "grow_box" image in our bounds.
- (void)drawRect:(NSRect)dirtyRect {
  [image_ drawInRect:[self bounds] fromRect:NSZeroRect
      operation:NSCompositeSourceOver fraction:1.0];
}

- (void)mouseDown:(NSEvent*)event {
  startPoint_ = [NSEvent mouseLocation];
  startFrame_ = [[self window] frame];
  [self computeClipRect];
}

// Called when the user clicks and drags within the bounds. Resize the window's
// frame based on the delta of the drag.
- (void)mouseDragged:(NSEvent*)event {
  NSPoint point = [NSEvent mouseLocation];
  const int deltaX = point.x - startPoint_.x;
  const int deltaY = point.y - startPoint_.y;

  NSRect frame = startFrame_;
  frame.size.width = frame.size.width + deltaX;
  frame.origin.y = frame.origin.y + deltaY;
  frame.size.height = frame.size.height - deltaY;

  // Enforce that the frame is entirely contained within the computed clipRect.
  if (NSMaxX(frame) > NSMaxX(clipRect_))
    frame.size.width = NSMaxX(clipRect_) - frame.origin.x;
  if (NSMinY(frame) < NSMinY(clipRect_)) {
    int maxY = NSMaxY(frame);
    frame.origin.y = NSMinY(clipRect_);
    frame.size.height = maxY - frame.origin.y;
  }

  // Enforce min window size.
  NSSize minSize = [[self window] minSize];
  if (frame.size.width < minSize.width)
    frame.size.width = minSize.width;
  if (frame.size.height < minSize.height) {
    frame.origin.y = NSMaxY(startFrame_) - minSize.height;
    frame.size.height = minSize.height;
  }

  [[self window] setFrame:frame display:YES];
}

@end

@implementation GrowBoxView (PrivateMethods)
- (void)computeClipRect {
  // Start with the current frame.  It makes sense to allow users to
  // resize up to this size.
  clipRect_ = startFrame_;

  // Calculate the union of all the screen work areas.  If the lower
  // right corner of the window is outside a work area, that use that
  // screen's frame instead.
  NSPoint lowerRight = NSMakePoint(NSMaxX(startFrame_), NSMinY(startFrame_));
  for (NSScreen* screen in [NSScreen screens]) {
    if (!NSPointInRect(lowerRight, [screen visibleFrame]) &&
        NSPointInRect(lowerRight, [screen frame]))
      clipRect_ = NSUnionRect(clipRect_, [screen frame]);
    else
      clipRect_ = NSUnionRect(clipRect_, [screen visibleFrame]);
  }
}
@end
