// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_state_controller.h"
#import "chrome/browser/bookmarks/bookmark_utils.h"
#import "chrome/browser/browser.h"


@implementation BookmarkBarStateController

- (id)initWithBrowser:(Browser *)browser {
  if ((self = [super init])) {
    browser_ = browser;
    // Initial visibility state comes from our preference.
    if (browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar)) {
      visible_ = YES;
    }
  }
  return self;
}

- (BOOL)visible {
  return visible_;
}

// Whack and save a preference change.  On Windows this call
// is made from BookmarkBarView.
- (void)togglePreference {
  bookmark_utils::ToggleWhenVisible(browser_->profile());
}

- (void)toggleBookmarkBar {
  visible_ = visible_ ? NO : YES;
  [self togglePreference];
}

@end  // BookmarkBarStateController
