// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H_

#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/automation/automation_resource_tracker.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

class AutomationAutocompleteEditTracker :
    public AutomationResourceTracker<AutocompleteEditView*> {
 public:
  explicit AutomationAutocompleteEditTracker(IPC::Message::Sender* automation)
      : AutomationResourceTracker<AutocompleteEditView*>(automation) { }

  virtual ~AutomationAutocompleteEditTracker() {
  }

  virtual void AddObserver(AutocompleteEditView* resource) {
    registrar_.Add(this, NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
                   Source<AutocompleteEditView>(resource));
  }

  virtual void RemoveObserver(AutocompleteEditView* resource) {
    registrar_.Remove(this, NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
                      Source<AutocompleteEditView>(resource));
  }
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H_
