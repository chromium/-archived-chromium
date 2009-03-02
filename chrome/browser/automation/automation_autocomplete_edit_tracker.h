// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H__

#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/automation/automation_resource_tracker.h"

class AutomationAutocompleteEditTracker:
    public AutomationResourceTracker<AutocompleteEditView*> {
 public:
  explicit AutomationAutocompleteEditTracker(IPC::Message::Sender* automation)
      : AutomationResourceTracker(automation) { }

  virtual ~AutomationAutocompleteEditTracker() {
    ClearAllMappings();
  }

  virtual void AddObserver(AutocompleteEditView* resource) {
    NotificationService::current()->AddObserver(
        this, NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
        Source<AutocompleteEditView>(resource));
  }

  virtual void RemoveObserver(AutocompleteEditView* resource) {
    NotificationService::current()->RemoveObserver(
        this, NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
        Source<AutocompleteEditView>(resource));
  }
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H__

