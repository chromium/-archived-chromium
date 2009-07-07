// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/gradient_button_cell.h"

@implementation GradientButtonCell

- (NSBackgroundStyle)interiorBackgroundStyle {
  return [self isHighlighted] ?
      NSBackgroundStyleLowered : NSBackgroundStyleRaised;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  // Constants from Cole.  Will kConstant them once the feedback loop
  // is complete.
  NSRect drawFrame = NSInsetRect(cellFrame, 1.5, 1.5);
  ButtonType type = [[(NSControl*)controlView cell] tag];
  switch (type) {
    case kRightButtonType:
      drawFrame.origin.x -= 20;
    case kLeftButtonType:
    case kLeftButtonWithShadowType:
      drawFrame.size.width += 20;
    default:
      break;
  }

  const float radius = 3.5;
  BOOL highlighted = [self isHighlighted];

  // TODO(jrg): convert to GTMTheme

  NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:drawFrame
                                                       xRadius:radius
                                                       yRadius:radius];
  NSBezierPath *outerPath =
      [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(drawFrame, -1, -1)
                                      xRadius:radius + 1
                                      yRadius:radius + 1];
  NSGradient *gradient = nil;

  if (highlighted) {
    NSColor* start = [NSColor colorWithCalibratedHue:0.6
                                          saturation:1.0
                                          brightness:0.6
                                               alpha:1.0];
    NSColor* end = [NSColor colorWithCalibratedHue:0.6
                                        saturation:1.0
                                        brightness:0.8
                                             alpha:1.0];
    gradient = [[[NSGradient alloc] initWithStartingColor:start
                                              endingColor:end] autorelease];
  } else {
    NSColor* start = [NSColor colorWithCalibratedWhite:1.0 alpha:1.0];
    NSColor* end = [NSColor colorWithCalibratedWhite:0.90 alpha:1.0];
    gradient = [[[NSGradient alloc] initWithStartingColor:start
                                              endingColor:end] autorelease];
  }

  // Stroke the borders and appropriate fill gradient. If we're borderless,
  // the only time we want to draw the inner gradient is if we're highlighted.
  [[NSColor colorWithCalibratedWhite:1.0 alpha:0.25] set];
  if ([self isBordered]) {
    [outerPath stroke];
    [gradient drawInBezierPath:path angle:90.0];
    [[NSColor colorWithCalibratedWhite:0.0 alpha:0.15] set];
    [path stroke];
  } else {
    if (highlighted)
      [gradient drawInBezierPath:path angle:90.0];
  }

  if (type == kLeftButtonWithShadowType) {
    NSRect borderRect, contentRect;
    NSDivideRect(cellFrame, &borderRect, &contentRect, 1.0, NSMaxXEdge);
    [[NSColor colorWithCalibratedWhite:0.0 alpha:0.15] set];
    NSRectFillUsingOperation(NSInsetRect(borderRect, 0, 2),
                             NSCompositeSourceOver);
  }

  [self drawInteriorWithFrame:NSOffsetRect(cellFrame, 0, 1) inView:controlView];
}

@end
