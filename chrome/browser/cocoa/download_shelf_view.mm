// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_shelf_view.h"

#include "base/scoped_nsobject.h"

@implementation DownloadShelfView

- (void)drawRect:(NSRect)rect {
  rect = [self bounds];

  // TODO(thakis): Once this has its final look, it also needs an
  // "inactive" state.

#if 0
  // Grey Finder/iCal-like bottom bar with dark gradient, dark/light lines
  NSColor* start =
      [NSColor colorWithCalibratedWhite: 0.75 alpha:1.0];
  NSColor* end = [NSColor colorWithCalibratedWhite:0.59 alpha:1.0];
  scoped_nsobject<NSGradient> gradient(
      [[NSGradient alloc] initWithStartingColor:start endingColor:end]);
  [gradient drawInRect:[self bounds] angle:270.0];

  NSRect borderRect, contentRect;
  NSDivideRect(rect, &borderRect, &contentRect, 1, NSMaxYEdge);
  [[NSColor colorWithDeviceWhite:0.25 alpha:1.0] set];
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);

  NSDivideRect(contentRect, &borderRect, &contentRect, 1, NSMaxYEdge);
  [[NSColor colorWithDeviceWhite:0.85 alpha:1.0] set];
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
#else
  // Glossy two-color bar with only light line at top (Mail.app/HitList-style)
  // Doesn't mesh with the matte look of the toolbar.

  NSRect topRect, bottomRect;
  NSDivideRect(rect, &topRect, &bottomRect, rect.size.height/2, NSMaxYEdge);

  // 1px line at top
  NSRect borderRect, contentRect;
  NSDivideRect(topRect, &borderRect, &contentRect, 1, NSMaxYEdge);
  [[NSColor colorWithDeviceWhite:0.69 alpha:1.0] set];
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);

  // Gradient for upper half
  NSColor* start =
      [NSColor colorWithCalibratedWhite: 1.0 alpha:1.0];
  NSColor* end = [NSColor colorWithCalibratedWhite:0.94 alpha:1.0];
  scoped_nsobject<NSGradient> gradient(
      [[NSGradient alloc] initWithStartingColor:start endingColor:end]);
  [gradient drawInRect:contentRect angle:270.0];

  // Fill lower half with solid color
  [[NSColor colorWithDeviceWhite:0.9 alpha:1.0] set];
  NSRectFillUsingOperation(bottomRect, NSCompositeSourceOver);
#endif
}

@end
