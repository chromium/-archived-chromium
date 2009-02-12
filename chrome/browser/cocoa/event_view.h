// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EVENT_VIEW_H_
#define CHROME_BROWSER_COCOA_EVENT_VIEW_H_

#import <Cocoa/Cocoa.h>

// A view that registers for mouse move events, and funnels all events.

@interface EventView : NSView {
 @private
  NSTrackingArea *trackingArea_;
}

- (id)initWithFrame:(NSRect)frame;

// Override these methods in a subclass.
- (void)mouseEvent:(NSEvent *)theEvent;
- (void)keyEvent:(NSEvent *)theEvent;

@end

#endif  // CHROME_BROWSER_COCOA_EVENT_VIEW_H_
