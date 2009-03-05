// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/grow_box_view.h"

@implementation GrowBoxView

- (void)awakeFromNib {
  image_ = [[NSImage imageNamed:@"grow_box"] retain];
}

- (void)dealloc {
  [image_ release];
  [super dealloc];
}

// Draws the "grow_box" image in our bounds.
- (void)drawRect:(NSRect)dirtyRect {
  [image_ drawInRect:[self bounds] fromRect:NSZeroRect
      operation:NSCompositeSourceOver fraction:1.0];
}

// Called when the user clicks and drags within the bounds. Resize the window's
// frame based on the delta of the drag.
- (void)mouseDragged:(NSEvent*)event {
  NSRect frame = [[self window] frame];
  frame.size.width += [event deltaX];
  frame.origin.y -= [event deltaY];
  frame.size.height += [event deltaY];
  [[self window] setFrame:frame display:YES];
}

@end
