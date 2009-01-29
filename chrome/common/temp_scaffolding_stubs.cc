// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "temp_scaffolding_stubs.h"

#include "base/file_util.h"
#include "base/thread.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_service.h"

BrowserProcessImpl::BrowserProcessImpl(const CommandLine& command_line)
    : main_notification_service_(new NotificationService),
      memory_model_(HIGH_MEMORY_MODEL),
      created_local_state_(), created_metrics_service_(),
      created_profile_manager_() {
  g_browser_process = this;
}

BrowserProcessImpl::~BrowserProcessImpl() {
  g_browser_process = NULL;
}

void BrowserProcessImpl::CreateLocalState() {
  DCHECK(!created_local_state_ && local_state_.get() == NULL);
  created_local_state_ = true;

  std::wstring local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  local_state_.reset(new PrefService(local_state_path));
}

void BrowserProcessImpl::CreateMetricsService() {
  DCHECK(!created_metrics_service_ && metrics_service_.get() == NULL);
  created_metrics_service_ = true;

  metrics_service_.reset(new MetricsService);
}

void BrowserProcessImpl::CreateProfileManager() {
  DCHECK(!created_profile_manager_ && profile_manager_.get() == NULL);
  created_profile_manager_ = true;

  profile_manager_.reset(new ProfileManager());
}

MetricsService* BrowserProcessImpl::metrics_service() {
  if (!created_metrics_service_)
    CreateMetricsService();
  return metrics_service_.get();
}

ProfileManager* BrowserProcessImpl::profile_manager() {
  if (!created_profile_manager_)
    CreateProfileManager();
  return profile_manager_.get();
}

PrefService* BrowserProcessImpl::local_state() {
  if (!created_local_state_)
    CreateLocalState();
  return local_state_.get();
}

//--------------------------------------------------------------------------

static bool s_in_startup = false;

bool BrowserInit::ProcessCommandLine(const CommandLine& parsed_command_line,
                                     const std::wstring& cur_dir,
                                     PrefService* prefs, bool process_startup,
                                     Profile* profile, int* return_code) {
  return LaunchBrowser(parsed_command_line, profile, cur_dir,
                       process_startup, return_code);
}

bool BrowserInit::LaunchBrowser(const CommandLine& parsed_command_line,
                                Profile* profile, const std::wstring& cur_dir,
                                bool process_startup, int* return_code) {
  s_in_startup = process_startup;
  bool result = LaunchBrowserImpl(parsed_command_line, profile, cur_dir,
                                  process_startup, return_code);
  s_in_startup = false;
  return result;
}

bool BrowserInit::LaunchBrowserImpl(const CommandLine& parsed_command_line,
                                    Profile* profile,
                                    const std::wstring& cur_dir,
                                    bool process_startup,
                                    int* return_code) {
  DCHECK(profile);

  // this code is a simplification of BrowserInit::LaunchWithProfile::Launch()
  std::vector<GURL> urls_to_open;
  urls_to_open.push_back(GURL("http://dev.chromium.org"));
  urls_to_open.push_back(GURL("http://crbug.com"));
  urls_to_open.push_back(GURL("http://icanhascheezeburger.com"));
  Browser* browser = NULL;
  browser = OpenURLsInBrowser(browser, profile, urls_to_open);

  return true;
}

// a simplification of BrowserInit::LaunchWithProfile::OpenURLsInBrowser
Browser* BrowserInit::OpenURLsInBrowser(
    Browser* browser,
    Profile* profile,
    const std::vector<GURL>& urls) {
  DCHECK(!urls.empty());
  if (!browser || browser->type() != Browser::TYPE_NORMAL)
    browser = Browser::Create(profile);

  for (size_t i = 0; i < urls.size(); ++i) {
    browser->AddTabWithURL(
        urls[i], GURL(), PageTransition::START_PAGE, (i == 0), NULL);
  }
  browser->window()->Show();
  return browser;
}

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

namespace browser {
void RegisterAllPrefs(PrefService*, PrefService*) { }
}  // namespace browser

namespace browser_shutdown {
void ReadLastShutdownInfo()  { }
void Shutdown() { }
void OnShutdownStarting(ShutdownType type) { }
}

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
  return new TabContents;
}

//--------------------------------------------------------------------------

bool RLZTracker::GetAccessPointRlz(AccessPoint point, std::wstring* rlz) {
  return false;
}

bool RLZTracker::RecordProductEvent(Product product, AccessPoint point,
                                    Event event) {
  return false;
}

#if defined(OS_MACOSX)
// We link this in for now to avoid hauling in all of WebCore (which we will
// have to eventually do)
namespace webkit_glue {
std::string GetUserAgent(const GURL& url) {
  return "";
}
#endif

#if defined(OS_LINUX)
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  NOTIMPLEMENTED() << "CreateBrowserWindow";
  return NULL;
}
#endif
