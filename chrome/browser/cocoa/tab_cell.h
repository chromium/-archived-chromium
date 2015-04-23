// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_CELL_H_
#define CHROME_BROWSER_COCOA_TAB_CELL_H_

#import <Cocoa/Cocoa.h>

// A button cell that handles drawing/highlighting of tabs in the tab bar. Text
// drawing leaves room for an icon view on the left of the tab and a close
// button on the right. Technically, though, it doesn't know anything about what
// it's leaving space for, so they could be reversed or even replaced by views
// for other purposes.

@interface TabCell : NSButtonCell {
}
@end

#endif  // CHROME_BROWSER_COCOA_TAB_CELL_H_
