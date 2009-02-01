// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_PAGE_VIEW_H__

#include "chrome/browser/options_window.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_observer.h"
#include "chrome/views/link.h"
#include "chrome/views/native_button.h"

class PrefService;

///////////////////////////////////////////////////////////////////////////////
// OptionsPageView
//
//  A base class for Options dialog pages that handles ensuring control
//  initialization is done just once.
//
class OptionsPageView : public views::View,
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

  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

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

