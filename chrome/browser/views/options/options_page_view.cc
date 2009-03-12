// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/options_page_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/widget.h"

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView

OptionsPageView::OptionsPageView(Profile* profile)
    : profile_(profile),
      initialized_(false) {
}

OptionsPageView::~OptionsPageView() {
}

void OptionsPageView::UserMetricsRecordAction(const wchar_t* action,
                                              PrefService* prefs) {
  UserMetrics::RecordComputedAction(action, profile());
  if (prefs)
    prefs->ScheduleSavePersistentPrefs(g_browser_process->file_thread());
}

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView, NotificationObserver implementation:

void OptionsPageView::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::wstring>(details).ptr());
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

HWND OptionsPageView::GetRootWindow() const {
  // Our Widget is the TabbedPane content HWND, which is a child HWND.
  // We need the root HWND for parenting.
  return GetAncestor(GetWidget()->GetNativeView(), GA_ROOT);
}
