// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_cell.h"
#import "third_party/GTM/AppKit/GTMTheme.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"

#define INSET_MULTIPLIER 2.0/3.0
#define CP1_MULTIPLIER 1.0/3.0
#define CP2_MULTIPLIER 3.0/8.0

@implementation TabCell

- (id)initTextCell:(NSString *)aString {
  self = [super initTextCell:aString];
  if (self != nil) {
    // nothing for now...
  }
  return self;
}

- (NSBackgroundStyle)interiorBackgroundStyle {
  // GTMThemeStyle style =
  //     [self isHighlighted] ? GTMThemeStyleTabBarSelected
  //                          : GTMThemeStyleTabBarDeselected;
  return [[GTMTheme defaultTheme]
              interiorBackgroundStyleForStyle:GTMThemeStyleTabBarSelected
                                       active:YES];
}

// Override drawing the button so that it looks like a Chromium tab instead
// of just a normal MacOS button.
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  [[NSGraphicsContext currentContext] saveGraphicsState];
  NSRect rect = cellFrame;
  BOOL active =
      [[controlView window] isKeyWindow] || [[controlView window] isMainWindow];

  BOOL selected = [(NSButton *)controlView state];

  // Inset by 0.5 in order to draw on pixels rather than on borders (which would
  // cause blurry pixels). Decrease height by 1 in order to move away from the
  // edge for the dark shadow.
  rect = NSInsetRect(rect, 0.5, selected ? 0 : -0.5);
  rect.size.height -= 1;
  rect.origin.y += 1;

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

  // Outset many of these values by 1 to cause the fill to bleed outside the
  // clip area
  NSBezierPath *path = [NSBezierPath bezierPath];
  [path moveToPoint:NSMakePoint(bottomLeft.x - 1, bottomLeft.y + 1)];
  [path lineToPoint:NSMakePoint(bottomLeft.x - 1, bottomLeft.y)];
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

  GTMTheme *theme = [GTMTheme defaultTheme];
  NSGradient *gradient = nil;

  if (selected) {
    gradient = [theme gradientForStyle:GTMThemeStyleTabBarSelected
                                active:active];
    // Stroke with a translucent black
    [[NSColor colorWithCalibratedWhite:0.0 alpha:active ? 0.5 : 0.3] set];
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSShadow *shadow = [[[NSShadow alloc] init] autorelease];
    [shadow setShadowOffset:NSMakeSize(2, -1)];
    [shadow setShadowBlurRadius:2.0];
    [path fill];
    [[NSGraphicsContext currentContext] restoreGraphicsState];
  } else {
    gradient = [theme gradientForStyle:GTMThemeStyleTabBarDeselected
                                active:active];
    // Stroke with a translucent black
    [[NSColor colorWithCalibratedWhite:0.0 alpha:active ? 0.3 : 0.1] set];
    [path fill];
  }

  [[NSGraphicsContext currentContext] saveGraphicsState];
  [[NSBezierPath bezierPathWithRect:NSOffsetRect(cellFrame, 0, -1)] addClip];
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.2] set];
  [path setLineWidth:selected ? 2.0 : 1.0];
  [path stroke];
  [[NSGraphicsContext currentContext] restoreGraphicsState];

  [gradient drawInBezierPath:path angle:90.0];
  [[NSColor darkGrayColor] set];

  [[NSGraphicsContext currentContext] restoreGraphicsState];

  // Inset where the text and favicon are drawn to keep them away from the
  // sloping edges of the tab.
  int kInteriorInset = cellFrame.size.height / 2.0;
  [self drawInteriorWithFrame:NSInsetRect(cellFrame, kInteriorInset, 0)
                       inView:controlView];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  [super drawInteriorWithFrame:cellFrame inView:controlView];

  void* voidContext = [[NSGraphicsContext currentContext] graphicsPort];
  CGContextRef c = static_cast<CGContextRef>(voidContext);

  // Build the rect for where we draw the close (x).
  // TODO(alcor): This should be retrieved from a method that can also be called
  // for hit-testing. Right now it also will overlap content if the page title
  // is wide enough.
  const int kCloseRectSize = 8;
  NSRect closeRect, restRect;
  NSDivideRect(cellFrame, &closeRect, &restRect, NSHeight(cellFrame),
               NSMaxXEdge);
  closeRect = NSInsetRect(closeRect, kCloseRectSize, kCloseRectSize);

  NSGradient *gradient =
      [[GTMTheme defaultTheme] gradientForStyle:GTMThemeStyleTabBarSelected
                                         active:YES];
  NSColor *color = [gradient interpolatedColorAtLocation:0.5];
  CGFloat luminance = [color gtm_luminance];

  // Draw the (x). Fade the images as they move toward the contrasty ends of
  // the spectrum
  CGContextSetAlpha(c, 1.0 - fabsf(0.5 - luminance));
  CGContextBeginTransparencyLayer(c, NULL);
  [self drawImage:[NSImage imageNamed:@"NSStopProgressTemplate"]
        withFrame:closeRect
           inView:controlView];
  CGContextEndTransparencyLayer(c);
}

- (void)highlight:(BOOL)flag
        withFrame:(NSRect)cellFrame
           inView:(NSView *)controlView {
  // Don't do normal highlighting
}

@end
