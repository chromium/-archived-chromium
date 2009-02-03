// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "temp_scaffolding_stubs.h"

#include "base/file_util.h"
#include "base/thread.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"

// static
size_t SessionRestore::num_tabs_to_load_ = 0;

//--------------------------------------------------------------------------

UserDataManager* UserDataManager::instance_ = NULL;

UserDataManager* UserDataManager::Create() {
  DCHECK(!instance_);
  std::wstring user_data;
  PathService::Get(chrome::DIR_USER_DATA, &user_data);
  instance_ = new UserDataManager(user_data);
  return instance_;
}

UserDataManager* UserDataManager::Get() {
  DCHECK(instance_);
  return instance_;
}

bool ShellIntegration::SetAsDefaultBrowser() {
  return true;
}

bool ShellIntegration::IsDefaultBrowser() {
  return true;
}

//--------------------------------------------------------------------------

namespace browser_shutdown {
void ReadLastShutdownInfo()  { }
void Shutdown() { }
void OnShutdownStarting(ShutdownType type) { }
}


// static
bool FirstRun::IsChromeFirstRun() { return false; }

// static
bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        const FilePath& master_prefs_path,
                                        int* preference_details) {
  return false;
}

// static
int FirstRun::ImportNow(Profile* profile, const CommandLine& cmdline) {
  return 0;
}

// static
bool Upgrade::IsBrowserAlreadyRunning() { return false; }

// static
bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  return true;
}

// static
bool Upgrade::SwapNewChromeExeIfPresent() { return true; }

void OpenFirstRunDialog(Profile* profile) { }

GURL NewTabUIURL() {
  return GURL();
}

//--------------------------------------------------------------------------

PluginService* PluginService::GetInstance() {
  return Singleton<PluginService>::get();
}

PluginService::PluginService()
    : main_message_loop_(MessageLoop::current()),
      resource_dispatcher_host_(NULL),
      ui_locale_(g_browser_process->GetApplicationLocale()),
      plugin_shutdown_handler_(NULL) {
}

PluginService::~PluginService() {
}

void PluginService::SetChromePluginDataDir(const FilePath& data_dir) {
  AutoLock lock(lock_);
  chrome_plugin_data_dir_ = data_dir;
}

//--------------------------------------------------------------------------

void InstallJankometer(const CommandLine&) {
}

//--------------------------------------------------------------------------

void Browser::Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details) {
}

GURL Browser::GetHomePage() {
  return GURL("http://dev.chromium.org");
}

void Browser::LoadingStateChanged(TabContents* source) {
}

//--------------------------------------------------------------------------

TabContents* TabContents::CreateWithType(TabContentsType type,
                                         Profile* profile,
                                         SiteInstance* instance) {
  switch (type) {
    case TAB_CONTENTS_WEB:
      return new WebContents(profile, instance, NULL, MSG_ROUTING_NONE, NULL);
    default:
      return NULL;
  }
}

//--------------------------------------------------------------------------

bool RLZTracker::GetAccessPointRlz(AccessPoint point, std::wstring* rlz) {
  return false;
}

bool RLZTracker::RecordProductEvent(Product product, AccessPoint point,
                                    Event event) {
  return false;
}

// This depends on porting all the plugin IPC messages.
bool IsPluginProcess() {
  return false;
}

#if defined(OS_MACOSX)
// We link this in for now to avoid hauling in all of WebCore (which we will
// have to eventually do).
namespace webkit_glue {
std::string GetUserAgent(const GURL& url) {
  return "";
}
}
#endif

#if defined(OS_LINUX)
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  NOTIMPLEMENTED() << "CreateBrowserWindow";
  return NULL;
}
#endif

//--------------------------------------------------------------------------

namespace chrome_browser_net {

void EnableDnsPrefetch(bool) {}
  
}  // namespace chrome_browser_net

//--------------------------------------------------------------------------

void RunJavascriptMessageBox(WebContents* web_contents,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg) {
}

CacheManagerHost::CacheManagerHost() : revise_allocation_factory_(this) { }
CacheManagerHost::~CacheManagerHost() { }

void CacheManagerHost::ObserveActivity(int) {
}

CacheManagerHost* CacheManagerHost::GetInstance() {
  return Singleton<CacheManagerHost>::get();
}

void RunBeforeUnloadDialog(WebContents* web_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg) {
}

bool SSLManager::DeserializeSecurityInfo(const std::string&, int*, int*, int*) {
  return false;
}
