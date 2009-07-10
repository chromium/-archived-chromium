// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_prefs.h"

#include "chrome/browser/autofill_manager.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/google_url_tracker.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"

#if defined(TOOLKIT_VIEWS)  // TODO(port): whittle this down as we port
#include "chrome/browser/task_manager.h"
#include "chrome/browser/views/frame/browser_view.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/views/keyword_editor_view.h"
#endif

namespace browser {

void RegisterAllPrefs(PrefService* user_prefs, PrefService* local_state) {
  // Prefs in Local State
  Browser::RegisterPrefs(local_state);
  WebCacheManager::RegisterPrefs(local_state);
  ExternalProtocolHandler::RegisterPrefs(local_state);
  GoogleURLTracker::RegisterPrefs(local_state);
  MetricsLog::RegisterPrefs(local_state);
  MetricsService::RegisterPrefs(local_state);
  SafeBrowsingService::RegisterPrefs(local_state);
  browser_shutdown::RegisterPrefs(local_state);
  chrome_browser_net::RegisterPrefs(local_state);
  bookmark_utils::RegisterPrefs(local_state);
  PageInfoWindow::RegisterPrefs(local_state);
#if defined(TOOLKIT_VIEWS)  // TODO(port): whittle this down as we port
  BrowserView::RegisterBrowserViewPrefs(local_state);
  TaskManager::RegisterPrefs(local_state);
#endif

  // User prefs
  SessionStartupPref::RegisterUserPrefs(user_prefs);
  Browser::RegisterUserPrefs(user_prefs);
  PasswordManager::RegisterUserPrefs(user_prefs);
  chrome_browser_net::RegisterUserPrefs(user_prefs);
  DownloadManager::RegisterUserPrefs(user_prefs);
  SSLManager::RegisterUserPrefs(user_prefs);
  bookmark_utils::RegisterUserPrefs(user_prefs);
  AutofillManager::RegisterUserPrefs(user_prefs);
  TabContents::RegisterUserPrefs(user_prefs);
  TemplateURLPrepopulateData::RegisterUserPrefs(user_prefs);
  NewTabUI::RegisterUserPrefs(user_prefs);
  BlockedPopupContainer::RegisterUserPrefs(user_prefs);
  DevToolsManager::RegisterUserPrefs(user_prefs);
}

}  // namespace browser
