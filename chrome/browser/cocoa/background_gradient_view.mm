// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/background_gradient_view.h"

@implementation BackgroundGradientView

// TODO(jrg): this may be a good spot to add theme changes.
// "Theme changes" may include both GTMTheme and "Chrome Themes".

- (void)drawRect:(NSRect)rect {
  BOOL isKey = [[self window] isKeyWindow];

  NSColor* start =
      [NSColor colorWithCalibratedWhite: isKey ? 0.95 : 0.98 alpha:1.0];
  NSColor* end = [NSColor colorWithCalibratedWhite:0.90 alpha:1.0];
  NSGradient *gradient =
      [[[NSGradient alloc] initWithStartingColor:start endingColor:end]
          autorelease];
  [gradient drawInRect:[self bounds] angle:270.0];
  NSRect borderRect, contentRect;
  NSDivideRect([self bounds], &borderRect, &contentRect, 1, NSMinYEdge);

  [[NSColor colorWithDeviceWhite:0.0 alpha:0.3] set];
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

@end
