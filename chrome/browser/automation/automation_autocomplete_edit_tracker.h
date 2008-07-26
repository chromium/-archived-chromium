// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
