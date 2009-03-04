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
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/download/download_request_dialog_delegate.h"
#include "chrome/browser/download/download_request_manager.h"
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
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_glue.h"

//--------------------------------------------------------------------------

WebContents* AutomationProvider::GetWebContentsForHandle(
    int handle, NavigationController** tab) {
  NOTIMPLEMENTED();
  return NULL;
}

void AutomationProvider::GetActiveWindow(int* handle) { NOTIMPLEMENTED(); }

void AutomationProvider::IsWindowActive(int handle, bool* success,
                                        bool* is_active) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::ActivateWindow(int handle) { NOTIMPLEMENTED(); }

void AutomationProvider::SetWindowVisible(int handle, bool visible,
                                          bool* result) { NOTIMPLEMENTED(); }

void AutomationProvider::GetFocusedViewID(int handle, int* view_id) {
  NOTIMPLEMENTED();
}

void AutomationProvider::OpenNewBrowserWindow(int show_command) {
  NOTIMPLEMENTED();
}

void AutomationProvider::GetWindowForBrowser(int browser_handle,
                                             bool* success,
                                             int* handle) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetAutocompleteEditForBrowser(
    int browser_handle,
    bool* success,
    int* autocomplete_edit_handle) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetBrowserForWindow(int window_handle,
                                             bool* success,
                                             int* browser_handle) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetSecurityState(int handle, bool* success,
                                          SecurityStyle* security_style,
                                          int* ssl_cert_status,
                                          int* mixed_content_status) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetPageType(int handle, bool* success,
                                     NavigationEntry::PageType* page_type) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::ActionOnSSLBlockingPage(int handle, bool proceed,
                                                 IPC::Message* reply_message) {
  NOTIMPLEMENTED();
}

void AutomationProvider::PrintNow(int tab_handle,
                                  IPC::Message* reply_message) {
  NOTIMPLEMENTED();
}

void AutomationProvider::SavePage(int tab_handle,
                                  const std::wstring& file_name,
                                  const std::wstring& dir_path,
                                  int type,
                                  bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetAutocompleteEditText(int autocomplete_edit_handle,
                                                 bool* success,
                                                 std::wstring* text) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::SetAutocompleteEditText(int autocomplete_edit_handle,
                                                 const std::wstring& text,
                                                 bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::AutocompleteEditGetMatches(
    int autocomplete_edit_handle,
    bool* success,
    std::vector<AutocompleteMatchData>* matches) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::AutocompleteEditIsQueryInProgress(
    int autocomplete_edit_handle,
    bool* success,
    bool* query_in_progress) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::OnMessageFromExternalHost(
    int handle, const std::string& message) {
  NOTIMPLEMENTED();
}

void AutomationProvider::GetShowingAppModalDialog(bool* showing_dialog,
                                                  int* dialog_button) {
  NOTIMPLEMENTED();
}

void AutomationProvider::ClickAppModalDialogButton(int button, bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------

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

//--------------------------------------------------------------------------

void InstallJankometer(const CommandLine&) {
  // http://code.google.com/p/chromium/issues/detail?id=8077
}

void UninstallJankometer() {
  // http://code.google.com/p/chromium/issues/detail?id=8077
}

//--------------------------------------------------------------------------

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

  is_being_destroyed_ = true;

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

const string16& TabContents::GetTitle() const {
  // We use the title for the last committed entry rather than a pending
  // navigation entry. For example, when the user types in a URL, we want to
  // keep the old page's title until the new load has committed and we get a new
  // title.
  // The exception is with transient pages, for which we really want to use
  // their title, as they are not committed.
  NavigationEntry* entry = controller_->GetTransientEntry();
  if (entry)
    return entry->GetTitleForDisplay(controller_);

  entry = controller_->GetLastCommittedEntry();
  if (entry)
    return entry->GetTitleForDisplay(controller_);
  else if (controller_->LoadingURLLazily())
    return controller_->GetLazyTitle();
  return EmptyString16();
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
  if (is_loading == is_loading_)
    return;

  is_loading_ = is_loading;
  waiting_for_response_ = is_loading;

  // Suppress notifications for this TabContents if we are not active.
  if (!is_active_)
    return;

  if (delegate_)
    delegate_->LoadingStateChanged(this);

  NotificationType type = is_loading ? NotificationType::LOAD_START :
      NotificationType::LOAD_STOP;
  NotificationDetails det = NotificationService::NoDetails();;
  if (details)
      det = Details<LoadNotificationDetails>(details);
  NotificationService::current()->Notify(type, 
      Source<NavigationController>(this->controller()),
      det);
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

void TabContents::SetIsCrashed(bool state) {
  if (state == is_crashed_)
    return;

  is_crashed_ = state;
  if (delegate_)
    delegate_->ContentsStateChanged(this);
}

#if defined(OS_MACOSX)
void TabContents::OnStartDownload(DownloadItem* download){
  NOTIMPLEMENTED();
}

DownloadShelf* TabContents::GetDownloadShelf(){
  NOTIMPLEMENTED();
  return NULL;
}

void TabContents::ReleaseDownloadShelf() {
  NOTIMPLEMENTED();
}

void TabContents::MigrateShelf(TabContents* from, TabContents* to){
  NOTIMPLEMENTED();
}

void TabContents::MigrateShelfFrom(TabContents* tab_contents){
  NOTIMPLEMENTED();
}
#endif

//--------------------------------------------------------------------------

void RLZTracker::CleanupRlz() {
  // http://code.google.com/p/chromium/issues/detail?id=8152
}

bool RLZTracker::GetAccessPointRlz(AccessPoint point, std::wstring* rlz) {
  // http://code.google.com/p/chromium/issues/detail?id=8152
  return false;
}

bool RLZTracker::RecordProductEvent(Product product, AccessPoint point,
                                    Event event) {
  // http://code.google.com/p/chromium/issues/detail?id=8152
  return false;
}

// This depends on porting all the plugin IPC messages.
bool IsPluginProcess() {
  return false;
}

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

bool CanImportURL(const GURL& url) {
  NOTIMPLEMENTED();
  return false;
}

DownloadRequestDialogDelegate* DownloadRequestDialogDelegate::Create(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host) {
  NOTIMPLEMENTED();
  return NULL;
}

views::Window* CreateInputWindow(gfx::NativeWindow parent_hwnd,
                                 InputWindowDelegate* delegate) {
  NOTIMPLEMENTED();
  return new views::Window();
}

