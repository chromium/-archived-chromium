// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  static void SetStartupPref(PrefService* prefs,
                             const SessionStartupPref& pref);
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
