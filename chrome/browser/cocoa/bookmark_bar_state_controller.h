// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_STATE_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_STATE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

class Browser;

// A class to manage bookmark bar state (visible or not).  State is
// shared among all tabs and saved in a preference.
@interface BookmarkBarStateController : NSObject {
 @private
  Browser* browser_;
  BOOL visible_;
}

- (id)initWithBrowser:(Browser *)browser;

// Return YES or NO reflecting visibility state of the bookmark bar.
- (BOOL)visible;

// Toggle (on or off) the bookmark bar visibility.
- (void)toggleBookmarkBar;

@end  // BookmarkBarStateController


@interface BookmarkBarStateController (Private)
// Internal method exposed for unit test convenience.
- (void)togglePreference;
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_STATE_CONTROLLER_H_
