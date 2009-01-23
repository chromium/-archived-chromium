// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/toolbar_button_cell.h"

enum {
  kLeftButtonType = -1,
  kLeftButtonWithShadowType = -2,
  kStandardButtonType = 0,
  kRightButtonType	= 1,
};
typedef NSInteger ButtonType;

@implementation ToolbarButtonCell

- (NSBackgroundStyle)interiorBackgroundStyle {
  return [self isHighlighted] ?
      NSBackgroundStyleLowered : NSBackgroundStyleRaised;
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView{
  NSRect drawFrame = NSInsetRect(cellFrame, 1.5, 1.5);
  ButtonType type = [controlView tag];
  switch (type) {
    case kRightButtonType:
      drawFrame.origin.x -= 20;
    case kLeftButtonType:
    case kLeftButtonWithShadowType:
      drawFrame.size.width += 20;
    default:
      break;
  }

  float radius = 3.5;
  BOOL highlighted = [self isHighlighted];

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

  [[NSColor colorWithCalibratedWhite:1.0 alpha:0.25] set];
  [outerPath stroke];
  [gradient drawInBezierPath:path angle:90.0];
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.15] set];
  [path stroke];

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
