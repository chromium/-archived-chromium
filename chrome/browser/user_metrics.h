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

#ifndef CHROME_BROWSER_USER_METRICS_H__
#define CHROME_BROWSER_USER_METRICS_H__

#include <string>

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
