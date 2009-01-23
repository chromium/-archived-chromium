// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_cell.h"

#define INSET_MULTIPLIER 2.0/3.0
#define CP1_MULTIPLIER 1.0/3.0
#define CP2_MULTIPLIER 3.0/8.0

@implementation TabCell

// Override drawing the button so that it looks like a Chromium tab instead
// of just a normal MacOS button.
// TODO(pinkerton/alcor): Clearly this is a work in progress. Comments need
// to be added to the flow once we get a better idea of exactly what we
// want and the kinks worked out of the visual appearance/tracking.
// TODO(pinkerton/alcor): Document what most of these constants are.
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  [[NSGraphicsContext currentContext] saveGraphicsState];

  // create a rounded rect path with which we'll stroke the outine of the
  // tab.
  NSRect rect = cellFrame;
  //  rect.origin.y += 20;
  NSBezierPath *path =
      [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 0.5, 0.5)
                                      xRadius:10.0
                                      yRadius:10.0];

  [[NSColor lightGrayColor] setStroke];
  BOOL isKey = [[controlView window] isKeyWindow];
  if (1) { //[self state] || [self isHighlighted]) {
    [[NSColor colorWithCalibratedHue:218.0 / 360.0
                          saturation:0.05
                          brightness:1.0
                               alpha:1.0] setFill];
    // [[NSColor colorWithCalibratedHue:210.0/360.0 saturation:0.36
    //     brightness:0.73 alpha:1.0] setStroke];
    path = [NSBezierPath bezierPath];
    rect = NSInsetRect(rect, 0.5, 0.5);
    float radius = 4.0;
    if (radius > 0.0) {
      // Clamp radius to be no larger than half the rect's width or height.
      radius = MIN(radius, 0.5 * MIN(rect.size.width, rect.size.height));

      NSPoint bottomLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
      NSPoint bottomRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
      NSPoint topRight =
          NSMakePoint(NSMaxX(rect) - INSET_MULTIPLIER * NSHeight(rect),
                      NSMinY(rect));
      NSPoint topLeft =
          NSMakePoint(NSMinX(rect)  + INSET_MULTIPLIER * NSHeight(rect),
                      NSMinY(rect));

      float baseControlPointOutset = NSHeight(rect) * CP1_MULTIPLIER;
      float bottomControlPointInset = NSHeight(rect) * CP2_MULTIPLIER;

      [path moveToPoint:NSMakePoint(bottomLeft.x - 2, bottomLeft.y + 2)];
      [path lineToPoint:NSMakePoint(bottomLeft.x - 2, bottomLeft.y)];
      [path lineToPoint:bottomLeft];

      [path curveToPoint:topLeft
           controlPoint1:NSMakePoint(bottomLeft.x + baseControlPointOutset,
                                     bottomLeft.y)
           controlPoint2:NSMakePoint(topLeft.x - bottomControlPointInset,
                                     topLeft.y)];

      [path lineToPoint:topRight];

      [path curveToPoint:bottomRight
           controlPoint1:NSMakePoint(topRight.x + bottomControlPointInset,
                                     topRight.y)
           controlPoint2:NSMakePoint(bottomRight.x - baseControlPointOutset,
                                     bottomRight.y)];

      [path lineToPoint:NSMakePoint(bottomRight.x + 1, bottomRight.y)];
      [path lineToPoint:NSMakePoint(bottomRight.x + 1, bottomRight.y + 1)];
    }
    BOOL selected = [(NSButton *)controlView state];

    NSGradient *gradient = nil;

    if (!selected)
      [[NSBezierPath bezierPathWithRect:NSOffsetRect(cellFrame, 0, -1)]
          addClip];
    if (selected) {
      NSColor* startingColor = [NSColor colorWithCalibratedWhite:1.0 alpha:1.0];
      NSColor* endingColor =
          [NSColor colorWithCalibratedWhite:isKey ? 0.95 : 0.98 alpha:1.0];
      gradient =
          [[[NSGradient alloc]
              initWithStartingColor:startingColor endingColor:endingColor]
                  autorelease];
      [[NSColor colorWithCalibratedWhite:0.0 alpha:isKey ? 0.5 : 0.3] set];
    } else {
      NSColor* startingColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.5];
      NSColor* endingColor = [NSColor colorWithCalibratedWhite:0.95 alpha:0.5];
      gradient =
          [[[NSGradient alloc]
              initWithStartingColor:startingColor endingColor:endingColor]
                  autorelease];
      [[NSColor colorWithCalibratedWhite:0.0 alpha:isKey ? 0.3 : 0.1] set];
    }

    if (isKey || selected)
      [gradient drawInBezierPath:path angle:90.0];

    [path stroke];
  }

  [[NSGraphicsContext currentContext] restoreGraphicsState];
  [self drawInteriorWithFrame:NSInsetRect(cellFrame, 12.0, 0)
                       inView:controlView];
}

- (void)drawImage:(NSImage*)image withFrame:(NSRect)frame
    inView:(NSView*)controlView {
  NSSize size = [image size];
  [image setFlipped: YES];

  float opacity = [self isEnabled] ? 1.0 : 0.25;

  [image drawInRect:NSMakeRect(frame.origin.x, frame.origin.y, 16.0, 16.0)
           fromRect:NSMakeRect(0, 0, size.width, size.height)
          operation:NSCompositeSourceOver fraction:opacity];
}

- (void)highlight:(BOOL)flag withFrame:(NSRect)cellFrame
    inView:(NSView *)controlView {
  // Don't do normal highlighting
}

@end
