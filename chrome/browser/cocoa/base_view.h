// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BASE_VIEW_H_
#define CHROME_BROWSER_COCOA_BASE_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/gfx/rect.h"

// A view that provides common functionality that many views will need:
// - Automatic registration for mouse-moved events.
// - Funneling of mouse and key events to two methods
// - Coordinate conversion utilities

@interface BaseView : NSView {
 @private
  NSTrackingArea *trackingArea_;
}

- (id)initWithFrame:(NSRect)frame;

// Override these methods in a subclass.
- (void)mouseEvent:(NSEvent *)theEvent;
- (void)keyEvent:(NSEvent *)theEvent;

// Useful rect conversions (doing coordinate flipping)
- (gfx::Rect)NSRectToRect:(NSRect)rect;
- (NSRect)RectToNSRect:(gfx::Rect)rect;

@end

#endif  // CHROME_BROWSER_COCOA_BASE_VIEW_H_
