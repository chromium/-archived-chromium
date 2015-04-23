// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CHROMIUM_BUTTON_CELL_H_
#define CHROME_BROWSER_COCOA_CHROMIUM_BUTTON_CELL_H_

#import <Cocoa/Cocoa.h>

// Base class for button cells for toolbar and bookmark bar.
//
// This is a button cell that handles drawing/highlighting of buttons.
// The appearance is determined by setting the cell's tag (not the
// view's) to one of the constants below (ButtonType).

enum {
  kLeftButtonType = -1,
  kLeftButtonWithShadowType = -2,
  kStandardButtonType = 0,
  kRightButtonType = 1,
};
typedef NSInteger ButtonType;

@interface GradientButtonCell : NSButtonCell {
}
@end

#endif  // CHROME_BROWSER_COCOA_CHROMIUM_BUTTON_CELL_H_
