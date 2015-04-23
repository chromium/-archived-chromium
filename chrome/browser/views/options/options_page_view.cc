// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/options_page_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "views/widget/widget.h"

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView

OptionsPageView::OptionsPageView(Profile* profile)
    : OptionsPageBase(profile),
      initialized_(false) {
}

OptionsPageView::~OptionsPageView() {
}

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView, views::View overrides:

void OptionsPageView::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  if (!initialized_ && is_add && GetWidget()) {
    // It is important that this only get done _once_ otherwise we end up
    // duplicating the view hierarchy when tabs are switched.
    initialized_ = true;
    InitControlLayout();
    NotifyPrefChanged(NULL);
  }
}
