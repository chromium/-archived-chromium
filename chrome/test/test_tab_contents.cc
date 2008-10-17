// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_tab_contents.h"

#include "chrome/browser/navigation_entry.h"

// static
SiteInstance* TestTabContents::site_instance_ = NULL;

TestTabContents::TestTabContents(TabContentsType type)
    : TabContents(type),
      commit_on_navigate_(false),
      next_page_id_(1) {
}

bool TestTabContents::NavigateToPendingEntry(bool reload) {
  if (commit_on_navigate_) {
    CompleteNavigationAsRenderer(next_page_id_++,
                                 controller()->GetPendingEntry()->url());
  }
  return true;
}

void TestTabContents::CompleteNavigationAsRenderer(int page_id,
                                                   const GURL& url) {
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = page_id;
  params.url = url;
  params.transition = PageTransition::LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;

  NavigationController::LoadCommittedDetails details;
  controller()->RendererDidNavigate(params, false, &details);
}
