// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APP_LAUNCHER_H__
#define CHROME_BROWSER_WEB_APP_LAUNCHER_H__

#include "chrome/browser/gears_integration.h"
#include "googleurl/src/gurl.h"

class Profile;

// WebAppLauncher is used during startup to launch a web app (aka an installed
// app).
class WebAppLauncher {
 public:
  // Queries Gears for the name of the app, and when Gears callsback with the
  // response creates a WebApp and Browser.
  static void Launch(Profile* profile, const GURL& url, int show_command);

 private:
  WebAppLauncher(Profile* profile, const GURL& url, int show_command);

  // Invoked from the Launch method. Queries Gears for the apps. Gears callback
  // to OnGotApps.
  void Run();

  // Callback from Gears when list of apps is available. Creates WebApp and
  // Browser.
  void OnGotApps(GearsShortcutList* apps);

  Profile* profile_;

  // URL of the app.
  const GURL url_;

  // How to show the app.
  const int show_command_;

  DISALLOW_EVIL_CONSTRUCTORS(WebAppLauncher);
};

#endif  // CHROME_BROWSER_WEB_APP_LAUNCHER_H__

