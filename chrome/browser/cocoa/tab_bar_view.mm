// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_bar_view.h"

@implementation TabBarView

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Nothing yet to do here...
  }
  return self;
}

- (void)drawRect:(NSRect)rect {
  rect = [self bounds];
  BOOL isKey = [[self window] isKeyWindow];
  if (isKey) {
    NSGradient *gradient =
          [[[NSGradient alloc] initWithStartingColor:
              [NSColor colorWithCalibratedWhite:0.0 alpha:0.0]
                                        endingColor:
              [NSColor colorWithCalibratedWhite:0.0 alpha:0.05]] autorelease];
    [gradient drawInRect:[self bounds] angle:270.0];
  }

  NSRect borderRect, contentRect;
  NSDivideRect(rect, &borderRect, &contentRect, 1, NSMinYEdge);
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.2] set];

  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

@end
