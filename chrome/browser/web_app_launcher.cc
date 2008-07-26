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

#include "chrome/browser/web_app_launcher.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"

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
