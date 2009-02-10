// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "temp_scaffolding_stubs.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/thread.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/singleton.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
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
#include "webkit/glue/webkit_glue.h"


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
bool delete_resources_on_shutdown = true;
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
  TabContents* contents;
  
  switch (type) {
    case TAB_CONTENTS_WEB:
      contents = new WebContents(profile, instance, NULL, MSG_ROUTING_NONE,
                                 NULL);
      break;
    default:
      NOTREACHED() << "Don't know how to create tab contents of type " << type;
      contents = NULL;
  }
  
  if (contents)
    contents->CreateView();
  
  return contents;
}

void TabContents::SetupController(Profile* profile) {
  DCHECK(!controller_);
  controller_ = new NavigationController(this, profile);
}

Profile* TabContents::profile() const {
  return controller_ ? controller_->profile() : NULL;
}

void TabContents::CloseContents() {
  // Destroy our NavigationController, which will Destroy all tabs it owns.
  controller_->Destroy();
  // Note that the controller may have deleted us at this point,
  // so don't touch any member variables here.
}

void TabContents::Destroy() {
  // TODO(pinkerton): this isn't the real version of Destroy(), just enough to
  // get the scaffolding working.

  // Notify any observer that have a reference on this tab contents.
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(this),
      NotificationService::NoDetails());

  // Notify our NavigationController.  Make sure we are deleted first, so
  // that the controller is the last to die.
  NavigationController* controller = controller_;
  TabContentsType type = this->type();

  delete this;

  controller->TabContentsWasDestroyed(type);
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

//--------------------------------------------------------------------------

namespace chrome_browser_net {

void EnableDnsPrefetch(bool) { NOTIMPLEMENTED(); }

void DnsPrefetchList(const std::vector<std::string>& hostnames) { NOTIMPLEMENTED(); }

}  // namespace chrome_browser_net

//--------------------------------------------------------------------------

// This is from chrome_plugin_util.cc.
void CPB_Free(void* memory) { NOTIMPLEMENTED(); }

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

void SSLManager::OnSSLCertificateError(
    ResourceDispatcherHost* resource_dispatcher,
    URLRequest* request,
    int cert_error,
    net::X509Certificate* cert,
    MessageLoop* ui_loop) {
  NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------

void RunRepostFormWarningDialog(NavigationController*) {
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

StringPiece ResourceBundle::GetRawDataResource(int resource_id) {
  NOTIMPLEMENTED();
  return StringPiece();
}

std::string ResourceBundle::GetDataResource(int resource_id) {
  NOTIMPLEMENTED();
  return "";
}

#endif

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request,
                                MessageLoop* ui_loop) {
  NOTIMPLEMENTED();
  return NULL;
}

namespace tab_util {

bool GetTabContentsID(URLRequest* request,
                      int* render_process_host_id,
                      int* routing_id) {
  NOTIMPLEMENTED();
  return true;
}

WebContents* GetWebContentsByID(int render_process_host_id,
                                int render_view_id) {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace

void ProcessWatcher::EnsureProcessTerminated(int) {
  NOTIMPLEMENTED();
}


//--------------------------------------------------------------------------
namespace webkit_glue {

bool IsDefaultPluginEnabled() {
  NOTIMPLEMENTED();
  return false;
}

#if defined(OS_MACOSX)
bool ClipboardIsFormatAvailable(Clipboard::FormatType format) {
  NOTIMPLEMENTED();
  return false;
}
#endif

}  // webkit_glue


//--------------------------------------------------------------------------
size_t AutocompleteProvider::max_matches_ = 42;
size_t AutocompleteResult::max_matches_ = 42;

// static
std::string AutocompleteInput::TypeToString(Type type) {
  NOTIMPLEMENTED();
  return "";
}

AutocompleteMatch::AutocompleteMatch(AutocompleteProvider* provider,
                                     int relevance,
                                     bool deletable,
                                     Type type) {
  NOTIMPLEMENTED();
}

// static
void AutocompleteMatch::ClassifyLocationInString(
    size_t match_location,
    size_t match_length,
    size_t overall_length,
    int style,
    ACMatchClassifications* classifications) {
  NOTIMPLEMENTED();
}

// static
std::string AutocompleteMatch::TypeToString(Type type) {
  NOTIMPLEMENTED();
  return "";
}

AutocompleteProvider::~AutocompleteProvider() {
  NOTIMPLEMENTED();
}

std::wstring AutocompleteProvider::StringForURLDisplay(const GURL& url,
                                                       bool check_accept_lang) {
  NOTIMPLEMENTED();
  return L"";
}

void HistoryURLProvider::ExecuteWithDB(history::HistoryBackend* backend,
                                       history::URLDatabase* db,
                                       HistoryURLProviderParams* params) {
  NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------
namespace bookmark_utils {

struct TitleMatch {
  void* undefined;
};

void GetBookmarksMatchingText(BookmarkModel* model,
                              const std::wstring& text,
                              size_t max_count,
                              std::vector<TitleMatch>* matches) {
  NOTIMPLEMENTED();
}

bool MoreRecentlyAdded(BookmarkNode* n1, BookmarkNode* n2) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<BookmarkNode*> GetMostRecentlyModifiedGroups(BookmarkModel* model,
                                                         size_t max_count) {
  NOTIMPLEMENTED();
  return std::vector<BookmarkNode*>();
}


void GetBookmarksContainingText(BookmarkModel* model,
                                const std::wstring& text,
                                size_t max_count,
                                std::vector<BookmarkNode*>* nodes) {
  NOTIMPLEMENTED();
}

bool DoesBookmarkContainText(BookmarkNode* node, const std::wstring& text) {
  NOTIMPLEMENTED();
  return false;
}

void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<BookmarkNode*>* nodes) {
  NOTIMPLEMENTED();
}

}  // bookmark_utils
