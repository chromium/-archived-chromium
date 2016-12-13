// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/options_page_base.h"

#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"

///////////////////////////////////////////////////////////////////////////////
// OptionsPageBase

OptionsPageBase::OptionsPageBase(Profile* profile)
    : profile_(profile) {
}

OptionsPageBase::~OptionsPageBase() {
}

void OptionsPageBase::UserMetricsRecordAction(const wchar_t* action,
                                              PrefService* prefs) {
  UserMetrics::RecordComputedAction(action, profile());
  if (prefs)
    prefs->ScheduleSavePersistentPrefs();
}

///////////////////////////////////////////////////////////////////////////////
// OptionsPageBase, NotificationObserver implementation:

void OptionsPageBase::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::wstring>(details).ptr());
}
