// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/page_action.h"

PageAction::PageAction()
  : type_(PERMANENT),
    active_tab_id_(-1) {
}

PageAction::~PageAction() {
}

// TODO(finnur): The tracking of active tab and url probably needs to change
// but it is hard to test while we are hard coding the tab index, so I'll
// leave it for later.
void PageAction::SetActiveTabIdAndUrl(int tab_id, const GURL& url) {
  active_tab_id_ = tab_id;
  active_url_ = url;
}

bool PageAction::IsActive(int tab_id, const GURL& url) const {
  return !active_url_.is_empty() &&
         url == active_url_ &&
         tab_id == active_tab_id_;
}
