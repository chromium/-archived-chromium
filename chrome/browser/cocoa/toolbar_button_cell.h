// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TOOLBAR_BUTTON_CELL_H_
#define CHROME_BROWSER_COCOA_TOOLBAR_BUTTON_CELL_H_

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/gradient_button_cell.h"

// A button cell for the toolbar.

// TODO(jrg): Why have a class at all when the base class does it all?
// I anticipate making changes for extensions.  Themes may also
// require changes.  I don't yet know if those will be common across
// the toolbar and bookmark bar or not.  The initial CL which made
// this empty was the use of the base class for both toolbar and
// bookmark bar button cells.  It seems wasteful to remove the files
// then add them back in soon after.
// TODO(jrg): If no differences come up, remove this file and use
// the base class explicitly for both the toolbar and bookmark bar.

@interface ToolbarButtonCell : GradientButtonCell {
}
@end

#endif  // CHROME_BROWSER_COCOA_TOOLBAR_BUTTON_CELL_H_
