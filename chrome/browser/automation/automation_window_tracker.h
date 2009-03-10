// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H__

#include "chrome/browser/automation/automation_resource_tracker.h"
#include "chrome/views/hwnd_notification_source.h"

class AutomationWindowTracker
    : public AutomationResourceTracker<HWND> {
 public:
  AutomationWindowTracker(IPC::Message::Sender* automation)
      : AutomationResourceTracker(automation) { }
  virtual ~AutomationWindowTracker() {
    ClearAllMappings();
  }

  virtual void AddObserver(HWND resource) {
    NotificationService::current()->AddObserver(
        this, NotificationType::WINDOW_CLOSED, Source<HWND>(resource));
  }

  virtual void RemoveObserver(HWND resource) {
    NotificationService::current()->RemoveObserver(
        this, NotificationType::WINDOW_CLOSED, Source<HWND>(resource));
  }
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H__
