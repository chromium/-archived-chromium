// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/session_startup_pref.h"

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "base/string_util.h"

// static
void SessionStartupPref::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kRestoreOnStartup, 0);
  prefs->RegisterListPref(prefs::kURLsToRestoreOnStartup);
}

// static
void SessionStartupPref::SetStartupPref(
    Profile* profile,
    const SessionStartupPref& pref) {
  DCHECK(profile);
  SetStartupPref(profile->GetPrefs(), pref);
}

//static
void SessionStartupPref::SetStartupPref(PrefService* prefs,
                                        const SessionStartupPref& pref) {
  DCHECK(prefs);
  int type = 0;
  switch(pref.type) {
    case LAST:
      type = 1;
      break;

    case URLS:
      type = 4;
      break;

    default:
      break;
  }
  prefs->SetInteger(prefs::kRestoreOnStartup, type);

  // Always save the URLs, that way the UI can remain consistent even if the
  // user changes the startup type pref.
  // Ownership of the ListValue retains with the pref service.
  ListValue* url_pref_list =
      prefs->GetMutableList(prefs::kURLsToRestoreOnStartup);
  DCHECK(url_pref_list);
  url_pref_list->Clear();
  for (size_t i = 0; i < pref.urls.size(); ++i) {
    url_pref_list->Set(static_cast<int>(i),
                       new StringValue(UTF8ToWide(pref.urls[i].spec())));
  }
}

// static
SessionStartupPref SessionStartupPref::GetStartupPref(Profile* profile) {
  DCHECK(profile);
  return GetStartupPref(profile->GetPrefs());
}

// static
SessionStartupPref SessionStartupPref::GetStartupPref(PrefService* prefs) {
  DCHECK(prefs);
  SessionStartupPref pref;
  switch (prefs->GetInteger(prefs::kRestoreOnStartup)) {
    case 1: {
      pref.type = LAST;
      break;
    }

    case 4: {
      pref.type = URLS;
      break;
    }

    // default case or bogus type are treated as not doing anything special
    // on startup.
  }

  ListValue* url_pref_list = prefs->GetMutableList(
      prefs::kURLsToRestoreOnStartup);
  DCHECK(url_pref_list);
  for (size_t i = 0; i < url_pref_list->GetSize(); ++i) {
    Value* value = NULL;
    if (url_pref_list->Get(i, &value)) {
      std::string url_text;
      if (value->GetAsString(&url_text))
        pref.urls.push_back(GURL(url_text));
    }
  }

  return pref;
}

