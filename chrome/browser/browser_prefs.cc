// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_prefs.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/google_url_tracker.h"
#include "chrome/browser/metrics_service.h"
#include "chrome/browser/password_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/ssl_manager.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/task_manager.h"
#include "chrome/browser/template_url_prepopulate_data.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/bookmark_manager_view.h"
#include "chrome/browser/views/bookmark_table_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/browser/views/page_info_window.h"
#include "chrome/browser/tab_contents/web_contents.h"

namespace browser {

void RegisterAllPrefs(PrefService* user_prefs, PrefService* local_state) {
  // Prefs in Local State
  BookmarkManagerView::RegisterPrefs(local_state);
  Browser::RegisterPrefs(local_state);
  BrowserView::RegisterBrowserViewPrefs(local_state);
  browser_shutdown::RegisterPrefs(local_state);
  CacheManagerHost::RegisterPrefs(local_state);
  chrome_browser_net::RegisterPrefs(local_state);
  GoogleURLTracker::RegisterPrefs(local_state);
  MetricsLog::RegisterPrefs(local_state);
  MetricsService::RegisterPrefs(local_state);
  PageInfoWindow::RegisterPrefs(local_state);
  RenderProcessHost::RegisterPrefs(local_state);
  TaskManager::RegisterPrefs(local_state);
  ExternalProtocolHandler::RegisterPrefs(local_state);
  SafeBrowsingService::RegisterPrefs(local_state);

  // User prefs
  BookmarkBarView::RegisterUserPrefs(user_prefs);
  BookmarkTableView::RegisterUserPrefs(user_prefs);
  Browser::RegisterUserPrefs(user_prefs);
  chrome_browser_net::RegisterUserPrefs(user_prefs);
  DownloadManager::RegisterUserPrefs(user_prefs);
  PasswordManager::RegisterUserPrefs(user_prefs);
  SessionStartupPref::RegisterUserPrefs(user_prefs);
  SSLManager::RegisterUserPrefs(user_prefs);
  TabContents::RegisterUserPrefs(user_prefs);
  TemplateURLPrepopulateData::RegisterUserPrefs(user_prefs);
  WebContents::RegisterUserPrefs(user_prefs);
}

}  // namespace browser
