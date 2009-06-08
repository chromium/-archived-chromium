// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_VIEW_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_VIEW_H_

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/background_gradient_view.h"

// A view that handles any special rendering of the bookmark bar.  At
// this time it only draws a gradient like the toolbar.  In the future
// I expect changes for new features (themes, extensions, etc).

@interface BookmarkBarView : BackgroundGradientView {
}
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_VIEW_H_
