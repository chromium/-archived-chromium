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
    // We also want to know about navigations so we can keep track of the last
    // navigation time.
    NotificationService::current()->AddObserver(
        this, NOTIFY_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(resource));
  }

  virtual void RemoveObserver(NavigationController* resource) {
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_TAB_CLOSING, Source<NavigationController>(resource));
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_EXTERNAL_TAB_CLOSED,
        Source<NavigationController>(resource));
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(resource));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type) {
      case NOTIFY_NAV_ENTRY_COMMITTED:
        last_navigation_times_[Source<NavigationController>(source).ptr()] =
            base::Time::Now();
        return;
      case NOTIFY_EXTERNAL_TAB_CLOSED:
      case NOTIFY_TAB_CLOSING:
        std::map<NavigationController*, base::Time>::iterator iter =
            last_navigation_times_.find(
            Source<NavigationController>(source).ptr());
        if (iter != last_navigation_times_.end())
          last_navigation_times_.erase(iter);
        break;
    }
    AutomationResourceTracker::Observe(type, source, details);
  }

  base::Time GetLastNavigationTime(int handle) {
    if (ContainsHandle(handle)) {
      NavigationController* controller = GetResource(handle);
      if (controller) {
        std::map<NavigationController*, base::Time>::const_iterator iter =
            last_navigation_times_.find(controller);
        if (iter != last_navigation_times_.end())
          return iter->second;
      }
    }
    return base::Time();
  }

 private:
  // Last time a navigation occurred.
   std::map<NavigationController*, base::Time> last_navigation_times_;

  DISALLOW_COPY_AND_ASSIGN(AutomationTabTracker);
};

#endif // CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H__

