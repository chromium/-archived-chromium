// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PREFS_H__
#define CHROME_BROWSER_BROWSER_PREFS_H__

class PrefService;

namespace browser {

// Makes the PrefService objects aware of all the prefs.
void RegisterAllPrefs(PrefService* user_prefs, PrefService* local_state);

}

#endif  // CHROME_BROWSER_BROWSER_PREFS_H__
