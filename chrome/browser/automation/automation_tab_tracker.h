// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H__

#include "chrome/browser/automation/automation_resource_tracker.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/navigation_controller.h"

class AutomationTabTracker
  : public AutomationResourceTracker<NavigationController*> {
public:
  AutomationTabTracker(IPC::Message::Sender* automation)
    : AutomationResourceTracker(automation) {}

  virtual ~AutomationTabTracker() {
    ClearAllMappings();
  }

  virtual void AddObserver(NavigationController* resource) {
    // This tab could either be a regular tab or an external tab
    // Register for both notifications
    NotificationService::current()->AddObserver(
        this, NOTIFY_TAB_CLOSING, Source<NavigationController>(resource));
    NotificationService::current()->AddObserver(
        this, NOTIFY_EXTERNAL_TAB_CLOSED,
        Source<NavigationController>(resource));
  }

  virtual void RemoveObserver(NavigationController* resource) {
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_TAB_CLOSING, Source<NavigationController>(resource));
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_EXTERNAL_TAB_CLOSED,
        Source<NavigationController>(resource));
  }
};

#endif // CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H__

