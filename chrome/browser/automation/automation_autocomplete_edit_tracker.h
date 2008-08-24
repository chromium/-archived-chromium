// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H__

#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/automation/automation_resource_tracker.h"

class AutomationAutocompleteEditTracker:
    public AutomationResourceTracker<AutocompleteEdit*> {
 public:
  explicit AutomationAutocompleteEditTracker(IPC::Message::Sender* automation)
    : AutomationResourceTracker(automation) { }

  virtual ~AutomationAutocompleteEditTracker() {
    ClearAllMappings();
  }

  virtual void AddObserver(AutocompleteEdit* resource) {
    NotificationService::current()->AddObserver(
        this, NOTIFY_AUTOCOMPLETE_EDIT_DESTROYED,
        Source<AutocompleteEdit>(resource));
  }

  virtual void RemoveObserver(AutocompleteEdit* resource) {
    NotificationService::current()->RemoveObserver(
        this, NOTIFY_AUTOCOMPLETE_EDIT_DESTROYED,
        Source<AutocompleteEdit>(resource));
  }
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H__

