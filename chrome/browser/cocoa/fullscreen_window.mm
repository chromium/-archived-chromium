// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/fullscreen_window.h"

@implementation FullscreenWindow

// Make sure our designated initializer gets called.
- (id)init {
  return [self initForScreen:[NSScreen mainScreen]];
}

- (id)initForScreen:(NSScreen*)screen {
  NSRect contentRect;
  contentRect.origin = NSZeroPoint;
  contentRect.size = [screen frame].size;

  if ((self = [super initWithContentRect:contentRect
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:YES
                                  screen:screen])) {
    [self setReleasedWhenClosed:NO];
  }
  return self;
}

// According to
// http://www.cocoabuilder.com/archive/message/cocoa/2006/6/19/165953,
// NSBorderlessWindowMask windows cannot become key or main.
// In our case, however, we don't want that behavior, so we override
// canBecomeKeyWindow and canBecomeMainWindow.

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (BOOL)canBecomeMainWindow {
  return YES;
}

@end
