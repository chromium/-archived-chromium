// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

// A FullscreenWindow is a borderless window suitable for going
// fullscreen.  The returned window is NOT release when closed and is
// not initially visible.
@interface FullscreenWindow : NSWindow

// Initialize a FullscreenWindow for the given screen.
// Designated initializer.
- (id)initForScreen:(NSScreen*)screen;

@end



