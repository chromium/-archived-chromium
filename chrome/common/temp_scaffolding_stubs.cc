// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "temp_scaffolding_stubs.h"

#include "build/build_config.h"

#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/thread.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/singleton.h"
#include "base/task.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/resource_bundle.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_glue.h"

//--------------------------------------------------------------------------

// static
size_t AutocompleteProvider::max_matches_;

AutocompleteProvider::~AutocompleteProvider() {
  // Though nothing needs this function yet, we want to get the C++ metadata
  // for this class grounded in this file.
  NOTIMPLEMENTED();
}

std::wstring AutocompleteProvider::StringForURLDisplay(const GURL& url,
                                                       bool check_accept_lang) {
  NOTIMPLEMENTED();
  return L"";
}

// static
size_t AutocompleteResult::max_matches_;

//static
std::string AutocompleteInput::TypeToString(AutocompleteInput::Type type) {
  NOTIMPLEMENTED();
  return "";
}

//static
std::string AutocompleteMatch::TypeToString(AutocompleteMatch::Type type) {
  NOTIMPLEMENTED();
  return "";
}

void AutocompleteMatch::ClassifyLocationInString(
    size_t match_location,
    size_t match_length,
    size_t overall_length,
    int style,
    ACMatchClassifications* classification) {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return true;
}

bool ShellIntegration::IsDefaultBrowser() {
  NOTIMPLEMENTED();
  return true;
}

//--------------------------------------------------------------------------

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
  // TODO(port): returning a blank URL here confuses the page IDs so make sure 
  // we load something
  return GURL("http://dev.chromium.org");
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

void UninstallJankometer() {
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

const GURL& TabContents::GetURL() const {
  // We may not have a navigation entry yet
  NavigationEntry* entry = controller_->GetActiveEntry();
  return entry ? entry->display_url() : GURL::EmptyGURL();
}

const std::wstring& TabContents::GetTitle() const {
  // We use the title for the last committed entry rather than a pending
  // navigation entry. For example, when the user types in a URL, we want to
  // keep the old page's title until the new load has committed and we get a new
  // title.
  // The exception is with transient pages, for which we really want to use
  // their title, as they are not committed.
  NavigationEntry* entry = controller_->GetTransientEntry();
  if (entry)
    return entry->GetTitleForDisplay();
  
  entry = controller_->GetLastCommittedEntry();
  if (entry)
    return entry->GetTitleForDisplay();
  else if (controller_->LoadingURLLazily())
    return controller_->GetLazyTitle();
  return EmptyWString();
}

void TabContents::NotifyNavigationStateChanged(unsigned changed_flags) {
  if (delegate_)
    delegate_->NavigationStateChanged(this, changed_flags);
}

void TabContents::OpenURL(const GURL& url, const GURL& referrer,
                          WindowOpenDisposition disposition,
                          PageTransition::Type transition) {
  if (delegate_)
    delegate_->OpenURLFromTab(this, url, referrer, disposition, transition);
}

void TabContents::SetIsLoading(bool is_loading,
                               LoadNotificationDetails* details) {
  // TODO(port): this is a subset of SetIsLoading() as a stub
  is_loading_ = is_loading;
}

bool TabContents::SupportsURL(GURL* url) {
  GURL u(*url);
  if (TabContents::TypeForURL(&u) == type()) {
    *url = u;
    return true;
  }
  return false;
}

int32 TabContents::GetMaxPageID() {
  if (GetSiteInstance())
    return GetSiteInstance()->max_page_id();
  else
    return max_page_id_;
}

void TabContents::UpdateMaxPageID(int32 page_id) {
  // Ensure both the SiteInstance and RenderProcessHost update their max page
  // IDs in sync. Only WebContents will also have site instances, except during
  // testing.
  if (GetSiteInstance())
    GetSiteInstance()->UpdateMaxPageID(page_id);
  
  if (AsWebContents())
    AsWebContents()->process()->UpdateMaxPageID(page_id);
  else
    max_page_id_ = std::max(max_page_id_, page_id);
}

//--------------------------------------------------------------------------

void RLZTracker::CleanupRlz() {
  NOTIMPLEMENTED();
}

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

void DnsPrefetchList(const std::vector<std::string>& hostnames) {
  NOTIMPLEMENTED();
}

void SaveHostNamesForNextStartup(PrefService* local_state) { NOTIMPLEMENTED(); }

void TrimSubresourceReferrers()  { NOTIMPLEMENTED(); }

void SaveSubresourceReferrers(PrefService* local_state) { NOTIMPLEMENTED(); }

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

void ResourceBundle::CleanupSharedInstance() {
  NOTIMPLEMENTED();
}

#endif

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request,
                                MessageLoop* ui_loop) {
  NOTIMPLEMENTED();
  return NULL;
}

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

#ifndef CHROME_DEBUGGER_DISABLED
DebuggerShell::DebuggerShell(DebuggerInputOutput *io) { }
DebuggerShell::~DebuggerShell() { }
void DebuggerShell::Start() { NOTIMPLEMENTED(); }
void DebuggerShell::Debug(TabContents* tab) { NOTIMPLEMENTED(); }
void DebuggerShell::DebugMessage(const std::wstring& msg) { NOTIMPLEMENTED(); }
void DebuggerShell::OnDebugAttach() { NOTIMPLEMENTED(); }
void DebuggerShell::OnDebugDisconnect() { NOTIMPLEMENTED(); }
void DebuggerShell::DidConnect() { NOTIMPLEMENTED(); }
void DebuggerShell::DidDisconnect() { NOTIMPLEMENTED(); }
void DebuggerShell::ProcessCommand(const std::wstring& data) {
  NOTIMPLEMENTED();
}
#endif  // !CHROME_DEBUGGER_DISABLED

void HistoryURLProvider::ExecuteWithDB(history::HistoryBackend*,
                                       history::URLDatabase*,
                                       HistoryURLProviderParams*) {
  NOTIMPLEMENTED();
}

namespace bookmark_utils {

bool DoesBookmarkContainText(BookmarkNode* node, const std::wstring& text) {
  NOTIMPLEMENTED();
  return false;
}

void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<BookmarkNode*>* nodes) {
  NOTIMPLEMENTED();
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

}

ScopableCPRequest::~ScopableCPRequest() {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
namespace gfx {
std::wstring GetCleanStringFromUrl(const GURL& url,
                                   const std::wstring& languages,
                                   url_parse::Parsed* new_parsed,
                                   size_t* prefix_end) {
  NOTIMPLEMENTED();
  return L"";
}
}
#endif

MemoryDetails::MemoryDetails() {
  NOTIMPLEMENTED();
}

void MemoryDetails::StartFetch() {
  NOTIMPLEMENTED();
}

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}

void CPHandleCommand(int command, CPCommandInterface* data,
                     CPBrowsingContext context) {
  NOTIMPLEMENTED();
}

bool CanImportURL(const GURL& url) {
  NOTIMPLEMENTED();
  return false;
}

bool DOMUIContentsCanHandleURL(GURL* url,
                               TabContentsType* result_type) {
  NOTIMPLEMENTED();
  return false;
}

bool NewTabUIHandleURL(GURL* url,
                       TabContentsType* result_type) {
  NOTIMPLEMENTED();
  return false;
}
