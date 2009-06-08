// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_button_cell.h"

@implementation BookmarkButtonCell

- (NSSize)cellSizeForBounds:(NSRect)aRect {
  NSSize size = [super cellSizeForBounds:aRect];
  size.width += 2;
  size.height += 4;
  return size;
}

@end
