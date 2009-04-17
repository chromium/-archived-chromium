// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/find_bar_view.h"

@implementation FindBarView

- (void)drawRect:(NSRect)rect {
  // TODO(rohitrao): Make this prettier.
  rect = [self bounds];
  NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
  NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
  // Inset the bottom points by 1 so we draw the border entirely
  // inside the frame.
  NSPoint bottomLeft = NSMakePoint(NSMinX(rect) + 15, NSMinY(rect) + 1);
  NSPoint bottomRight = NSMakePoint(NSMaxX(rect) - 15, NSMinY(rect) + 1);

  NSBezierPath *path = [NSBezierPath bezierPath];
  [path moveToPoint:topLeft];
  [path curveToPoint:bottomLeft
        controlPoint1:NSMakePoint(topLeft.x + 15, topLeft.y)
        controlPoint2:NSMakePoint(bottomLeft.x - 15, bottomLeft.y)];
  [path lineToPoint:bottomRight];
  [path curveToPoint:topRight
        controlPoint1:NSMakePoint(bottomRight.x + 15, bottomRight.y)
        controlPoint2:NSMakePoint(topRight.x - 15, topRight.y)];

  [[NSColor colorWithCalibratedWhite:0.90 alpha:1.0] set];
  [path fill];

  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.3] set];
  [path stroke];
}

@end
