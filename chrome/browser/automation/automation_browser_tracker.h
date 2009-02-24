// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_BROWSER_TRACKER_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_BROWSER_TRACKER_H__

#include "chrome/browser/automation/automation_resource_tracker.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"

// Tracks Browser objects.
class AutomationBrowserTracker : public AutomationResourceTracker<Browser*> {
public:
  AutomationBrowserTracker(IPC::Message::Sender* automation)
      : AutomationResourceTracker<Browser*>(automation) { }

  virtual ~AutomationBrowserTracker() {
    ClearAllMappings();
  }

  virtual void AddObserver(Browser* resource) {
    NotificationService::current()->AddObserver(
        this, NotificationType::BROWSER_CLOSED, Source<Browser>(resource));
  }

  virtual void RemoveObserver(Browser* resource) {
    NotificationService::current()->RemoveObserver(
        this, NotificationType::BROWSER_CLOSED, Source<Browser>(resource));
  }
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_BROWSER_TRACKER_H__

