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

#include "chrome/browser/browser_prefs.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/download_manager.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/google_url_tracker.h"
#include "chrome/browser/metrics_service.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/password_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/ssl_manager.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/task_manager.h"
#include "chrome/browser/template_url_prepopulate_data.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/keyword_editor_view.h"

namespace browser {

void RegisterAllPrefs(PrefService* user_prefs, PrefService* local_state) {
  // Prefs in Local State
  Browser::RegisterPrefs(local_state);
  CacheManagerHost::RegisterPrefs(local_state);
  chrome_browser_net::RegisterPrefs(local_state);
  GoogleURLTracker::RegisterPrefs(local_state);
  MetricsLog::RegisterPrefs(local_state);
  MetricsService::RegisterPrefs(local_state);
  PageInfoWindow::RegisterPrefs(local_state);
  RenderProcessHost::RegisterPrefs(local_state);
  TaskManager::RegisterPrefs(local_state);
  ExternalProtocolHandler::RegisterPrefs(local_state);
  SafeBrowsingService::RegisterUserPrefs(local_state);
  browser_shutdown::RegisterPrefs(local_state);

  // User prefs
  BookmarkBarView::RegisterUserPrefs(user_prefs);
  Browser::RegisterUserPrefs(user_prefs);
  chrome_browser_net::RegisterUserPrefs(user_prefs);
  DownloadManager::RegisterUserPrefs(user_prefs);
  KeywordEditorView::RegisterUserPrefs(user_prefs);
  PasswordManager::RegisterUserPrefs(user_prefs);
  SessionStartupPref::RegisterUserPrefs(user_prefs);
  SpellChecker::RegisterUserPrefs(user_prefs);
  SSLManager::RegisterUserPrefs(user_prefs);
  TabContents::RegisterUserPrefs(user_prefs);
  TemplateURLPrepopulateData::RegisterUserPrefs(user_prefs);
  WebContents::RegisterUserPrefs(user_prefs);
}
}  // namespace browser
