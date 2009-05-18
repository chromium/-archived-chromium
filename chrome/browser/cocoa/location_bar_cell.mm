// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar_cell.h"

const NSInteger kBaselineOffset = 1;

@implementation LocationBarCell

- (void)drawInteriorWithFrame:(NSRect)cellFrame
                       inView:(NSView *)controlView {
  [super drawInteriorWithFrame:NSInsetRect(cellFrame, 0, kBaselineOffset)
                        inView:controlView];
}

// Override these methods so that the field editor shows up in the right place
- (void)editWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
               editor:(NSText *)textObj
             delegate:(id)anObject
                event:(NSEvent *)theEvent {
  [super editWithFrame:NSInsetRect(cellFrame, 0, kBaselineOffset)
                inView:controlView
                editor:textObj
              delegate:anObject
                 event:theEvent];
}


// Override these methods so that the field editor shows up in the right place
- (void)selectWithFrame:(NSRect)cellFrame
                 inView:(NSView *)controlView
                 editor:(NSText *)textObj
               delegate:(id)anObject
                  start:(NSInteger)selStart
                 length:(NSInteger)selLength {
  [super selectWithFrame:NSInsetRect(cellFrame, 0, kBaselineOffset)
                  inView:controlView editor:textObj
                delegate:anObject
                   start:selStart
                  length:selLength];
}

@end
