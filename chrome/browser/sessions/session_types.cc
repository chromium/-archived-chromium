// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_types.h"

#include "base/string_util.h"
#include "chrome/browser/tab_contents/navigation_entry.h"

// TabNavigation --------------------------------------------------------------

// static
NavigationEntry* TabNavigation::ToNavigationEntry(int page_id) const {
  NavigationEntry* entry = new NavigationEntry(
      NULL,  // The site instance for restored tabs is sent on navigation
             // (TabContents::GetSiteInstanceForEntry).
      page_id,
      url_,
      referrer_,
      title_,
      // Use a transition type of reload so that we don't incorrectly
      // increase the typed count.
      PageTransition::RELOAD);
  entry->set_display_url(url_);
  entry->set_content_state(state_);
  entry->set_has_post_data(type_mask_ & TabNavigation::HAS_POST_DATA);
  return entry;
}

void TabNavigation::SetFromNavigationEntry(const NavigationEntry& entry) {
  url_ = entry.display_url();
  referrer_ = entry.referrer();
  title_ = entry.title();
  state_ = entry.content_state();
  transition_ = entry.transition_type();
  type_mask_ = entry.has_post_data() ? TabNavigation::HAS_POST_DATA : 0;
}
