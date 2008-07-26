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
      std::wstring url_text;
      if (value->GetAsString(&url_text))
        pref.urls.push_back(GURL(url_text));
    }
  }

  return pref;
}
