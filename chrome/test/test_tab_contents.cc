// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_tab_contents.h"

#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/render_messages.h"

// static
SiteInstance* TestTabContents::site_instance_ = NULL;

TestTabContents::TestTabContents(TabContentsType type)
    : TabContents(type),
      next_page_id_(1) {
}

bool TestTabContents::NavigateToPendingEntry(bool reload) {
  return true;
}
