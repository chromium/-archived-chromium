// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_strip_view.h"

@implementation TabStripView

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Nothing yet to do here...
  }
  return self;
}

- (void)drawRect:(NSRect)rect {
  NSRect boundsRect = [self bounds];
  NSRect borderRect, contentRect;
  NSDivideRect(boundsRect, &borderRect, &contentRect, 1, NSMinYEdge);
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.2] set];

  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

@end
