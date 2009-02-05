// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "temp_scaffolding_stubs.h"

#include "base/file_util.h"
#include "base/logging.h"
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
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/resource_message_filter.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/resource_bundle.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webcursor.h"

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
  NOTIMPLEMENTED();
  return true;
}

bool ShellIntegration::IsDefaultBrowser() {
  NOTIMPLEMENTED();
  return true;
}

//--------------------------------------------------------------------------

namespace browser_shutdown {
void ReadLastShutdownInfo()  { NOTIMPLEMENTED(); }
void Shutdown() { NOTIMPLEMENTED(); }
void OnShutdownStarting(ShutdownType type) { NOTIMPLEMENTED(); }
}


// static
bool FirstRun::IsChromeFirstRun() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        const FilePath& master_prefs_path,
                                        int* preference_details) {
  NOTIMPLEMENTED();
  return false;
}

// static
int FirstRun::ImportNow(Profile* profile, const CommandLine& cmdline) {
  NOTIMPLEMENTED();
  return 0;
}

// static
bool Upgrade::IsBrowserAlreadyRunning() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  NOTIMPLEMENTED();
  return true;
}

// static
bool Upgrade::SwapNewChromeExeIfPresent() {
  NOTIMPLEMENTED();
  return true;
}

void OpenFirstRunDialog(Profile* profile) { NOTIMPLEMENTED(); }

GURL NewTabUIURL() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------

void Browser::Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details) {
  NOTIMPLEMENTED();
}

GURL Browser::GetHomePage() {
  NOTIMPLEMENTED();
  return GURL("http://dev.chromium.org");
}

void Browser::LoadingStateChanged(TabContents* source) {
  NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------

TabContents* TabContents::CreateWithType(TabContentsType type,
                                         Profile* profile,
                                         SiteInstance* instance) {
  switch (type) {
    case TAB_CONTENTS_WEB:
      return new WebContents(profile, instance, NULL, MSG_ROUTING_NONE, NULL);
    default:
      NOTIMPLEMENTED();
      return NULL;
  }
}

//--------------------------------------------------------------------------

bool RLZTracker::GetAccessPointRlz(AccessPoint point, std::wstring* rlz) {
  NOTIMPLEMENTED();
  return false;
}

bool RLZTracker::RecordProductEvent(Product product, AccessPoint point,
                                    Event event) {
  NOTIMPLEMENTED();
  return false;
}

// This depends on porting all the plugin IPC messages.
bool IsPluginProcess() {
  NOTIMPLEMENTED();
  return false;
}

#if defined(OS_MACOSX)
// We link this in for now to avoid hauling in all of WebCore (which we will
// have to eventually do).
namespace webkit_glue {
std::string GetUserAgent(const GURL& url) {
  NOTIMPLEMENTED();
  return "";
}
// TODO(pinkerton): when these are removed, mock_webkit_glue.cc
// must be re-added to the unit_test target for tests.
void SetRecordPlaybackMode(bool) { }
void SetJavaScriptFlags(const std::wstring&) { }
void CheckForLeaks() { }
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

void EnableDnsPrefetch(bool) { NOTIMPLEMENTED(); }

}  // namespace chrome_browser_net

//--------------------------------------------------------------------------

void RunJavascriptMessageBox(WebContents* web_contents,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg) {
  NOTIMPLEMENTED();
}

void RunBeforeUnloadDialog(WebContents* web_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg) {
  NOTIMPLEMENTED();
}

bool SSLManager::DeserializeSecurityInfo(const std::string&, int*, int*, int*) {
  NOTIMPLEMENTED();
  return false;
}

//--------------------------------------------------------------------------

ResourceMessageFilter::ResourceMessageFilter(
    ResourceDispatcherHost* resource_dispatcher_host,
    PluginService* plugin_service,
    printing::PrintJobManager* print_job_manager,
    int render_process_host_id,
    Profile* profile,
    RenderWidgetHelper* render_widget_helper,
    SpellChecker* spellchecker)
      : ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_msg_helper_(this, NULL)) {
}
ResourceMessageFilter::~ResourceMessageFilter() { NOTIMPLEMENTED(); }
void ResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  NOTIMPLEMENTED();
}
void ResourceMessageFilter::OnChannelConnected(int32 peer_pid) {
  NOTIMPLEMENTED();
}
void ResourceMessageFilter::OnChannelClosing() { NOTIMPLEMENTED(); }
bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  NOTIMPLEMENTED();
  return false;
}
bool ResourceMessageFilter::Send(IPC::Message* message) {
  NOTIMPLEMENTED();
  return false;
}
void ResourceMessageFilter::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) { }
void ResourceMessageFilter::OnResolveProxyCompleted(
    IPC::Message* reply_msg,int result, const std::string& proxy_list) {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
ResourceBundle* ResourceBundle::g_shared_instance_ = NULL;

// GetBitmapNamed() will leak, but there's no way around it for stubs.
SkBitmap* ResourceBundle::GetBitmapNamed(int) {
  NOTIMPLEMENTED();
  return new SkBitmap();
}
ResourceBundle::ResourceBundle() { }
ResourceBundle& ResourceBundle::GetSharedInstance() {
  NOTIMPLEMENTED();
  if (!g_shared_instance_)
    g_shared_instance_ = new ResourceBundle;
  return *g_shared_instance_;
}
#endif

void ResourceDispatcherHost::OnClosePageACK(int, int) {
  NOTIMPLEMENTED();
}
void ResourceDispatcherHost::CancelRequestsForRenderView(int, int) {
  NOTIMPLEMENTED();
}

void ProcessWatcher::EnsureProcessTerminated(int) {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
WebCursor::WebCursor() {
}
WebCursor::~WebCursor() {
}
bool WebCursor::Deserialize(const Pickle* pickle, void** iter) {
  NOTIMPLEMENTED();
  return false;
}
#endif
