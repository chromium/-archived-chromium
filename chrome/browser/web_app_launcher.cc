// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_app_launcher.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/web_app.h"

// static
void WebAppLauncher::Launch(Profile* profile,
                            const GURL& url,
                            int show_command) {
  (new WebAppLauncher(profile, url, show_command))->Run();
}

WebAppLauncher::WebAppLauncher(Profile* profile,
                               const GURL& url,
                               int show_command)
    : profile_(profile),
      url_(url),
      show_command_(show_command) {
}

void WebAppLauncher::Run() {
  GearsQueryShortcuts(NewCallback(this, &WebAppLauncher::OnGotApps));
}

void WebAppLauncher::OnGotApps(GearsShortcutList* apps) {
  WebApp* app = NULL;

  if (apps) {
    for (size_t i = 0; i < apps->num_shortcuts; ++i) {
      if (apps->shortcuts[i].url && GURL(apps->shortcuts[i].url) == url_) {
        app = new WebApp(profile_, apps->shortcuts[i]);
        break;
      }
    }
  }

  if (!app) {
    // Gears doesn't know about this app. Create one anyway.
    app = new WebApp(profile_, url_, std::wstring());
  }

  Browser::OpenWebApplication(profile_, app, show_command_);

  delete this;
}

