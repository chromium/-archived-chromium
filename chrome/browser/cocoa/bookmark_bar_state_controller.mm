// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_state_controller.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

@implementation BookmarkBarStateController

- (id)initWithProfile:(Profile*)profile {
  if ((self = [super init])) {
    profile_ = profile;
    // Initial visibility state comes from our preference.
    if (profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar)) {
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
  bookmark_utils::ToggleWhenVisible(profile_);
}

- (void)toggleBookmarkBar {
  visible_ = visible_ ? NO : YES;
  [self togglePreference];
}

@end  // BookmarkBarStateController
