// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cross_site_request_manager.h"

bool CrossSiteRequestManager::HasPendingCrossSiteRequest(int renderer_id,
                                                         int render_view_id) {
  AutoLock lock(lock_);

  std::pair<int, int> key(renderer_id, render_view_id);
  return pending_cross_site_views_.find(key) !=
      pending_cross_site_views_.end();
}

void CrossSiteRequestManager::SetHasPendingCrossSiteRequest(int renderer_id,
                                                            int render_view_id,
                                                            bool has_pending) {
  AutoLock lock(lock_);

  std::pair<int, int> key(renderer_id, render_view_id);
  if (has_pending) {
    pending_cross_site_views_.insert(key);
  } else {
    pending_cross_site_views_.erase(key);
  }
}
