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
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.3] set];

  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

// Called to determine where in our view hierarchy the click should go. We
// want clicks to go to our children (tabs, new tab button, etc), but no click
// should ever go to this view. In fact, returning this view breaks things
// such as the window buttons and double-clicking the title bar since the
// window manager believes there is a view that wants the mouse event. If the
// superclass impl says the click should go here, just cheat and return nil.
- (NSView*)hitTest:(NSPoint)point {
  NSView* hit = [super hitTest:point];
  if ([hit isEqual:self]) 
      hit = nil;
  return hit;
}

@end
