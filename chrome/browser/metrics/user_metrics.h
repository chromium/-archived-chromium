// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_METRICS_H__
#define CHROME_BROWSER_USER_METRICS_H__

#include <string>

class Profile;

// This module provides some helper functions for logging actions tracked by
// the user metrics system.

class UserMetrics {
 public:
  // Record that the user performed an action.
  // "Action" here means a user-generated event:
  //   good: "Reload", "CloseTab", and "IMEInvoked"
  //   not good: "SSLDialogShown", "PageLoaded", "DiskFull"
  // We use this to gather anonymized information about how users are
  // interacting with the browser.
  // WARNING: Call this function exactly like this, with the string literal
  // inline:
  //   UserMetrics::RecordAction(L"foo bar", profile);
  // because otherwise our processing scripts won't pick up on new actions.
  //
  // For more complicated situations (like when there are many different
  // possible actions), see RecordComputedAction.
  static void RecordAction(const wchar_t* action, Profile* profile);

  // This function has identical input and behavior to RecordAction, but is
  // not automatically found by the action-processing scripts.  It can be used
  // when it's a pain to enumerate all possible actions, but if you use this
  // you need to also update the rules for extracting known actions.
  static void RecordComputedAction(const std::wstring& action,
                                   Profile* profile);
};

#endif  // CHROME_BROWSER_USER_METRICS_H__

