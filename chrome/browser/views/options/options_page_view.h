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

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_PAGE_VIEW_H__

#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/link.h"
#include "chrome/views/native_button.h"

class PrefService;

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView
//
//  A base class for Options dialog pages that handles ensuring control
//  initialization is done just once.
//
class OptionsPageView : public ChromeViews::View,
                        public NotificationObserver {
 public:
  virtual ~OptionsPageView();

  // Returns true if the window containing this view can be closed, given the
  // current state of this view. This can be used to prevent the window from
  // being closed when a modal dialog box is showing, for example.
  virtual bool CanClose() const { return true; }

  // Highlights the specified group to attract the user's attention.
  virtual void HighlightGroup(OptionsGroup highlight_group) { }

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // This class cannot be instantiated directly, but its constructor must be
  // called by derived classes.
  explicit OptionsPageView(Profile* profile);

  // Returns the Profile associated with this page.
  Profile* profile() const { return profile_; }

  // Records a user action and schedules the prefs file to be saved.
  void UserMetricsRecordAction(const wchar_t* action, PrefService* prefs);

  // Initializes the layout of the controls within the panel.
  virtual void InitControlLayout() = 0;

  // Allows the UI to update when a preference value changes. The parameter is
  // the specific pref that changed, or NULL if all pref UI should be
  // validated. This is also called immediately after InitControlLayout()
  // during setup, but with NULL as the parameter to allow initial state to be
  // set.
  virtual void NotifyPrefChanged(const std::wstring* pref_name) { }

  // ChromeViews::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

  // Returns the HWND on which created windows should be parented.
  HWND GetRootWindow() const;

 private:
  // The Profile associated with this page.
  Profile* profile_;

  // Whether or not the control layout has been initialized for this page.
  bool initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(OptionsPageView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_PAGE_VIEW_H__
