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

#ifndef CHROME_BROWSER_SESSION_STARTUP_PREF_H__
#define CHROME_BROWSER_SESSION_STARTUP_PREF_H__

#include <vector>

#include "googleurl/src/gurl.h"

class PrefService;
class Profile;

// StartupPref specifies what should happen at startup for a specified profile.
// StartupPref is stored in the preferences for a particular profile.
struct SessionStartupPref {
  enum Type {
    // Indicates the user doesn't want to restore a previous session.
    DEFAULT,

    // Indicates the user wants to restore the last session.
    LAST,

    // Indicates the user wants to restore a specific set of URLs. The URLs
    // are contained in urls.
    URLS
  };

  static void RegisterUserPrefs(PrefService* prefs);

  // What should happen on startup for the specified profile.
  static void SetStartupPref(Profile* profile, const SessionStartupPref& pref);
  static void SetStartupPref(PrefService* prefs, const SessionStartupPref& pref);
  static SessionStartupPref GetStartupPref(Profile* profile);
  static SessionStartupPref GetStartupPref(PrefService* prefs);

  SessionStartupPref() : type(DEFAULT) {}

  explicit SessionStartupPref(Type type) : type(type) {}

  // What to do on startup.
  Type type;

  // The URLs to restore. Only used if type == URLS.
  std::vector<GURL> urls;
};

#endif  // CHROME_BROWSER_SESSION_STARTUP_PREF_H__
