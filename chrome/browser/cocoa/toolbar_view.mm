// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/toolbar_view.h"

@implementation ToolbarView

- (void)drawRect:(NSRect)rect {
  BOOL isKey = [[self window] isKeyWindow];
  rect = [self bounds];

  NSColor* start =
      [NSColor colorWithCalibratedWhite: isKey ? 0.95 : 0.98 alpha:1.0];
  NSColor* end = [NSColor colorWithCalibratedWhite:0.90 alpha:1.0];
  NSGradient *gradient =
      [[[NSGradient alloc] initWithStartingColor:start endingColor:end]
          autorelease];
  [gradient drawInRect:[self bounds] angle:270.0];
  NSRect borderRect, contentRect;
  NSDivideRect(rect, &borderRect, &contentRect, 1, NSMinYEdge);

  [[NSColor colorWithDeviceWhite:0.0 alpha:0.3] set];
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

@end
