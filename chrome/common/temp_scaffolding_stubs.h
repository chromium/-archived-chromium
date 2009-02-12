// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
#define CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_

// This file provides declarations and stub definitions for classes we encouter
// during the porting effort. It is not meant to be permanent, and classes will
// be removed from here as they are fleshed out more completely.

#include <string>

#include "base/basictypes.h"
#include "base/clipboard.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/download/save_types.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_states.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class BookmarkService;
class CommandLine;
class ConstrainedWindow;
class DOMUIHost;
class DownloadManager;
class HistoryService;
class LoginHandler;
class MetricsService;
class MixedContentHandler;
class ModalHtmlDialogDelegate;
class NavigationController;
class NavigationEntry;
class NotificationService;
class PluginService;
class ProfileManager;
class Profile;
class RenderProcessHost;
class RenderWidgetHelper;
class ResourceMessageFilter;
class SessionCommand;
class SessionID;
class SiteInstance;
class SpellChecker;
class TabContents;
class TabNavigation;
struct ThumbnailScore;
class Task;
class TemplateURL;
class TemplateURLRef;
class URLRequest;
class URLRequestContext;
class UserScriptMaster;
class VisitedLinkMaster;
class WebContents;
class WebContentsView;
struct WebPluginInfo;
struct WebPluginGeometry;
class WebPreferences;

namespace base {
class Thread;
}

namespace IPC {
class Message;
}

namespace net {
class AuthChallengeInfo;
class IOBuffer;
class X509Certificate;
}

//---------------------------------------------------------------------------
// These stubs are for Browser_main()

// TODO(port): MessageWindow is very windows-specific, but provides the concept
//             of singleton browser process per user-data-dir. Investigate how
//             to achieve this on other platforms and see if this API works.
class MessageWindow {
 public:
  explicit MessageWindow(const FilePath& user_data_dir) { }
  ~MessageWindow() { }
  bool NotifyOtherProcess() {
    NOTIMPLEMENTED();
    return false;
  }
  void HuntForZombieChromeProcesses() { NOTIMPLEMENTED(); }
  void Create() { NOTIMPLEMENTED(); }
  void Lock() { NOTIMPLEMENTED(); }
  void Unlock() { NOTIMPLEMENTED(); }
};

class GoogleUpdateSettings {
 public:
  static bool GetCollectStatsConsent() {
    NOTIMPLEMENTED();
    return false;
  }
  static bool SetCollectStatsConsent(bool consented) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool GetBrowser(std::wstring* browser) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool GetLanguage(std::wstring* language) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool GetBrand(std::wstring* brand) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool GetReferral(std::wstring* referral) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool ClearReferral() {
    NOTIMPLEMENTED();
    return false;
  }
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GoogleUpdateSettings);
};

class AutomationProviderList {
 public:
  static AutomationProviderList* GetInstance() {
    NOTIMPLEMENTED();
    return NULL;
  }
};

class UserDataManager {
 public:
  static UserDataManager* Create();
  static UserDataManager* Get();

  explicit UserDataManager(const std::wstring& user_data_root) { }
 private:
  // Shared instance.
  static UserDataManager* instance_;
};

class SessionBackend : public base::RefCountedThreadSafe<SessionBackend> {
 public:
   void MoveCurrentSessionToLastSession() { NOTIMPLEMENTED(); }
};

class BaseSessionService : public CancelableRequestProvider,
    public base::RefCountedThreadSafe<BaseSessionService> {
 public:
  enum SessionType {
    SESSION_RESTORE,
    TAB_RESTORE
  };
  BaseSessionService() { NOTIMPLEMENTED(); }
  BaseSessionService(SessionType type,
                     Profile* profile,
                     const std::wstring& path) {
    NOTIMPLEMENTED();
  }
  Profile* profile() const { NOTIMPLEMENTED(); return NULL; }
  void DeleteLastSession() { NOTIMPLEMENTED(); }
  class InternalGetCommandsRequest;
  typedef Callback2<Handle, scoped_refptr<InternalGetCommandsRequest> >::Type
      InternalGetCommandsCallback;
  class InternalGetCommandsRequest :
      public CancelableRequest<InternalGetCommandsCallback> {
   public:
    explicit InternalGetCommandsRequest(CallbackType* callback)
      : CancelableRequest<InternalGetCommandsCallback>(callback) {
    }
    virtual ~InternalGetCommandsRequest() { }
    std::vector<SessionCommand*> commands;
   private:
    DISALLOW_COPY_AND_ASSIGN(InternalGetCommandsRequest);
  };
 protected:
  SessionBackend* backend() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  base::Thread* backend_thread() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  std::vector<SessionCommand*>&  pending_commands() {
    NOTIMPLEMENTED();
    return pending_commands_;
  }
  void set_pending_reset(bool value) { NOTIMPLEMENTED(); }
  bool pending_reset() const { NOTIMPLEMENTED(); return false; }
  int commands_since_reset() const { NOTIMPLEMENTED(); return 0; }
  virtual void ScheduleCommand(SessionCommand* command) { NOTIMPLEMENTED(); }
  void StartSaveTimer() { NOTIMPLEMENTED(); }
  virtual void Save() { NOTIMPLEMENTED(); }
  SessionCommand* CreateUpdateTabNavigationCommand(
      SessionID::id_type command_id,
      SessionID::id_type tab_id,
      int index,
      const NavigationEntry& entry) {
    NOTIMPLEMENTED();
    return NULL;
  }
  bool RestoreUpdateTabNavigationCommand(const SessionCommand& command,
                                         TabNavigation* navigation,
                                         SessionID::id_type* tab_id) {
    NOTIMPLEMENTED();
    return false;
  }
  bool ShouldTrackEntry(const NavigationEntry& entry) {
    NOTIMPLEMENTED();
    return false;
  }
  bool ShouldTrackEntry(const TabNavigation& navigation) {
    NOTIMPLEMENTED();
    return false;
  }
  Handle ScheduleGetLastSessionCommands(
      InternalGetCommandsRequest* request,
      CancelableRequestConsumerBase* consumer) {
    NOTIMPLEMENTED();
    return 0;
  }
  static const int max_persist_navigation_count = 6;
 private:
   std::vector<SessionCommand*> pending_commands_;
};

class SessionRestore {
 public:
  static void RestoreSession(Profile* profile,
                             Browser* browser,
                             bool clobber_existing_window,
                             bool always_create_tabbed_browser,
                             const std::vector<GURL>& urls_to_open) {
    NOTIMPLEMENTED();
  }
  static void RestoreSessionSynchronously(
      Profile* profile,
      const std::vector<GURL>& urls_to_open) { NOTIMPLEMENTED(); }

  static size_t num_tabs_to_load_;
};

class MetricsService {
 public:
  MetricsService() { }
  ~MetricsService() { }
  void Start() { NOTIMPLEMENTED(); }
  void StartRecordingOnly() { NOTIMPLEMENTED(); }
  void Stop() { NOTIMPLEMENTED(); }
  void SetUserPermitsUpload(bool enabled) { NOTIMPLEMENTED(); }
  void RecordCleanShutdown() { NOTIMPLEMENTED(); }
  void RecordStartOfSessionEnd() { NOTIMPLEMENTED(); }
};

namespace browser {
void RegisterAllPrefs(PrefService*, PrefService*);
}

void OpenFirstRunDialog(Profile* profile);

void InstallJankometer(const CommandLine&);

GURL NewTabUIURL();

//---------------------------------------------------------------------------
// These stubs are for BrowserProcessImpl

class ClipboardService : public Clipboard {
 public:
};

class CancelableTask;
class ViewMsg_Print_Params;

namespace printing {

class PrintingContext {
 public:
  enum Result { OK, CANCEL, FAILED };
};

class PrintSettings {
 public:
  void RenderParams(ViewMsg_Print_Params* params) const { NOTIMPLEMENTED(); }
  int dpi() const { NOTIMPLEMENTED(); return 92; }
};

class PrinterQuery : public base::RefCountedThreadSafe<PrinterQuery> {
 public:
  enum GetSettingsAskParam {
    DEFAULTS,
    ASK_USER,
  };

  void GetSettings(GetSettingsAskParam ask_user_for_settings,
                   int parent_window,
                   int expected_page_count,
                   CancelableTask* callback) { NOTIMPLEMENTED(); }
  PrintingContext::Result last_status() { return PrintingContext::FAILED; }
  const PrintSettings& settings() { NOTIMPLEMENTED(); return settings_; }
  int cookie() { NOTIMPLEMENTED(); return 0; }
  void StopWorker() { NOTIMPLEMENTED(); }

 private:
  PrintSettings settings_;
};

class PrintJobManager {
 public:
  void OnQuit() { NOTIMPLEMENTED(); }
  void PopPrinterQuery(int document_cookie, scoped_refptr<PrinterQuery>* job) {
    NOTIMPLEMENTED();
  }
  void QueuePrinterQuery(PrinterQuery* job) { NOTIMPLEMENTED(); }
};

}  // namespace printing


class SafeBrowsingBlockingPage {
  public:
  static void ShowBlockingPage(
      SafeBrowsingService* service,
      const SafeBrowsingService::UnsafeResource& resource) {
    NOTIMPLEMENTED();
  }
};

class SafeBrowsingProtocolManager {
 public:
  SafeBrowsingProtocolManager(SafeBrowsingService* service,
                              MessageLoop* notify_loop,
                              const std::string& client_key,
                              const std::string& wrapped_key) {
    NOTIMPLEMENTED();
  }

  ~SafeBrowsingProtocolManager() { NOTIMPLEMENTED(); }
  void OnChunkInserted() { NOTIMPLEMENTED(); }
  void OnGetChunksComplete(const std::vector<SBListChunkRanges>& list,
                           bool database_error) {
    NOTIMPLEMENTED();
  }
  void Initialize() { NOTIMPLEMENTED(); }
  base::Time last_update() const { return last_update_; }
  void GetFullHash(SafeBrowsingService::SafeBrowsingCheck* check,
                   const std::vector<SBPrefix>& prefixes) {
    NOTIMPLEMENTED();
  }
 private:
  base::Time last_update_;
};

struct DownloadBuffer {
  Lock lock;
  typedef std::pair<net::IOBuffer*, int> Contents;
  std::vector<Contents> contents;
};

class DownloadItem {
public:
  enum DownloadState {
    IN_PROGRESS,
    COMPLETE,
    CANCELLED,
    REMOVING
  };
};

class DownloadFileManager
    : public base::RefCountedThreadSafe<DownloadFileManager> {
 public:
  DownloadFileManager(MessageLoop* ui_loop, ResourceDispatcherHost* rdh) {
    NOTIMPLEMENTED();
  }
  void Initialize() { NOTIMPLEMENTED(); }
  void Shutdown() { NOTIMPLEMENTED(); }
  MessageLoop* file_loop() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  int GetNextId() {
    NOTIMPLEMENTED();
    return 0;
  }
  void StartDownload(DownloadCreateInfo* info) { NOTIMPLEMENTED(); }
  void UpdateDownload(int id, DownloadBuffer* buffer) { NOTIMPLEMENTED(); }
  void DownloadFinished(int id, DownloadBuffer* buffer) { NOTIMPLEMENTED(); }
};

class DownloadRequestManager
    : public base::RefCountedThreadSafe<DownloadRequestManager> {
 public:
  DownloadRequestManager(MessageLoop* io_loop, MessageLoop* ui_loop) {
    NOTIMPLEMENTED();
  }
  class Callback {
   public:
    virtual void ContinueDownload() = 0;
    virtual void CancelDownload() = 0;
  };
  void CanDownloadOnIOThread(int render_process_host_id,
                             int render_view_id,
                             Callback* callback) {
    NOTIMPLEMENTED();
  }
};

class SaveFileManager : public base::RefCountedThreadSafe<SaveFileManager> {
 public:
  SaveFileManager(MessageLoop* ui_loop,
                  MessageLoop* io_loop,
                  ResourceDispatcherHost* rdh) {
    NOTIMPLEMENTED();
  }
  void Shutdown() { NOTIMPLEMENTED(); }
  void StartSave(SaveFileCreateInfo* info) { NOTIMPLEMENTED(); }
  void UpdateSaveProgress(int save_id, net::IOBuffer* data, int size) {
    NOTIMPLEMENTED();
  }
  void SaveFinished(int save_id, const GURL& save_url,
                    int render_process_id, int is_success) {
    NOTIMPLEMENTED();
  }
  int GetNextId() {
    NOTIMPLEMENTED();
    return 0;
  }
  MessageLoop* GetSaveLoop() {
    NOTIMPLEMENTED();
    return NULL;
  }
};

namespace sandbox {

class BrokerServices {
 public:
  void Init() { NOTIMPLEMENTED(); }
};

}  // namespace sandbox

class IconManager {
};

struct ViewHostMsg_DidPrintPage_Params;

namespace views {

class AcceleratorHandler {
};

}  // namespace views

//---------------------------------------------------------------------------
// These stubs are for Browser

class StatusBubble {
 public:
  void SetStatus(const std::wstring&) { NOTIMPLEMENTED(); }
  void Hide() { NOTIMPLEMENTED(); }
};

class SavePackage : public base::RefCountedThreadSafe<SavePackage>,
                    public RenderViewHostDelegate::Save {
 public:
  enum SavePackageType {
    SAVE_AS_ONLY_HTML = 0,
    SAVE_AS_COMPLETE_HTML = 1
  };
  struct SavePackageParam {
    SavePackageParam(const std::string&) { }
    const std::string current_tab_mime_type;
    PrefService* prefs;
    SavePackageType save_type;
    FilePath saved_main_file_path;
    FilePath dir;
  };
  static bool IsSavableContents(const std::string&) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool IsSavableURL(const GURL& url) {
    NOTIMPLEMENTED();
    return false;
  }
  static FilePath GetSuggestNameForSaveAs(PrefService*,
                                          const FilePath&) {
    NOTIMPLEMENTED();
    return FilePath();
  }
  static bool GetSaveInfo(const FilePath&, void*,
                          SavePackageParam*, DownloadManager*) {
    NOTIMPLEMENTED();
    return false;
  }
  SavePackage(WebContents*, SavePackageType, const FilePath&,
              const FilePath&) { NOTIMPLEMENTED(); }
  bool Init() {
    NOTIMPLEMENTED();
    return true;
  }
  virtual void OnReceivedSavableResourceLinksForCurrentPage(
      const std::vector<GURL>& resources_list,
      const std::vector<GURL>& referrers_list,
      const std::vector<GURL>& frames_list) { NOTIMPLEMENTED(); }
  virtual void OnReceivedSerializedHtmlData(const GURL& frame_url,
                                            const std::string& data,
                                            int32 status) { NOTIMPLEMENTED(); }
};

class DebuggerWindow : public base::RefCountedThreadSafe<DebuggerWindow> {
 public:
};

class FaviconStatus {
 public:
  const GURL& url() const { return url_; }
 private:
  GURL url_;
};

class TabContentsDelegate {
 public:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {
    NOTIMPLEMENTED();
  }
  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    OpenURLFromTab(NULL, url, referrer, disposition, transition);
  }
  virtual void UpdateTargetURL(TabContents*, const GURL&) { NOTIMPLEMENTED(); }
  virtual void CloseContents(TabContents*) { NOTIMPLEMENTED(); }
  virtual void MoveContents(TabContents*, const gfx::Rect&) {
    NOTIMPLEMENTED();
  }
  virtual bool IsPopup(TabContents*) {
    NOTIMPLEMENTED();
    return false;
  }
  virtual void ForwardMessageToExternalHost(const std::string&,
                                            const std::string&) {
    NOTIMPLEMENTED();
  }
  virtual TabContents* GetConstrainingContents(TabContents*) {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual void ShowHtmlDialog(ModalHtmlDialogDelegate*, void*) {
    NOTIMPLEMENTED();
  }
  virtual bool CanBlur() {
    NOTIMPLEMENTED();
    return true;
  }
  virtual gfx::Rect GetRootWindowResizerRect() const {
    return gfx::Rect();
  }

  virtual bool IsExternalTabContainer() {
    NOTIMPLEMENTED();
    return false;
  }
  virtual void BeforeUnloadFired(WebContents*, bool, bool*) {
    NOTIMPLEMENTED();
  }
  virtual void URLStarredChanged(WebContents*, bool) { NOTIMPLEMENTED(); }
  virtual void ConvertContentsToApplication(WebContents*) { NOTIMPLEMENTED(); }
  virtual void ReplaceContents(TabContents*, TabContents*) { NOTIMPLEMENTED(); }
  virtual void NavigationStateChanged(const TabContents*, unsigned int) {
    NOTIMPLEMENTED();
  }
};

class InterstitialPage {
 public:
  virtual void DontProceed() { NOTIMPLEMENTED(); }
};

class InfoBarDelegate {
};

class ConfirmInfoBarDelegate : public InfoBarDelegate {
 public:
  explicit ConfirmInfoBarDelegate(TabContents* contents) {}

  enum InfoBarButton {
    BUTTON_NONE = 0,
    BUTTON_OK,
    BUTTON_CANCEL
  };
};

class LoadNotificationDetails {
 public:
  LoadNotificationDetails(const GURL&, PageTransition::Type,
                          base::TimeDelta, NavigationController*, int) { }
};

class TabContents : public PageNavigator, public NotificationObserver {
 public:
  enum InvalidateTypes {
    INVALIDATE_URL = 1,
    INVALIDATE_TITLE = 2,
    INVALIDATE_FAVICON = 4,
    INVALIDATE_LOAD = 8,
    INVALIDATE_EVERYTHING = 0xFFFFFFFF
  };
  TabContents(TabContentsType type) 
      : type_(type), is_active_(true), is_loading_(false), controller_(), 
        delegate_(), max_page_id_(-1) { }
  virtual ~TabContents() { }
  NavigationController* controller() const { return controller_; }
  void set_controller(NavigationController* c) { controller_ = c; }
  virtual WebContents* AsWebContents() { return NULL; }
  WebContents* AsWebContents() const {
    return const_cast<TabContents*>(this)->AsWebContents();
  }
  virtual SkBitmap GetFavIcon() const {
    NOTIMPLEMENTED();
    return SkBitmap();
  }
  const GURL& GetURL() const;
  virtual const std::wstring& GetTitle() const;
  TabContentsType type() const { return type_; }
  void set_type(TabContentsType type) { type_ = type; }
  virtual void Focus() { NOTIMPLEMENTED(); }
  virtual void Stop() { NOTIMPLEMENTED(); }
  Profile* profile() const;
  virtual void CloseContents();
  virtual void SetupController(Profile* profile);
  bool WasHidden() {
    NOTIMPLEMENTED();
    return false;
  }
  virtual void RestoreFocus() { NOTIMPLEMENTED(); }
  static TabContentsType TypeForURL(GURL* url) {
    NOTIMPLEMENTED();
    return TAB_CONTENTS_WEB;
  }
  static TabContents* CreateWithType(TabContentsType type,
                                     Profile* profile,
                                     SiteInstance* instance);
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) { NOTIMPLEMENTED(); }
  virtual void DidBecomeSelected() { NOTIMPLEMENTED(); }
  virtual void SetDownloadShelfVisible(bool) { NOTIMPLEMENTED(); }
  virtual void Destroy();
  virtual void SetIsLoading(bool, LoadNotificationDetails*);
  virtual void SetIsCrashed(bool) { NOTIMPLEMENTED(); }
  bool capturing_contents() const {
    NOTIMPLEMENTED();
    return false;
  }
  void set_capturing_contents(bool) { NOTIMPLEMENTED(); }
  bool is_active() const { return is_active_; }
  void set_is_active(bool active) { is_active_ = active; }
  bool is_loading() const { return is_loading_; }
  void SetNotWaitingForResponse() { NOTIMPLEMENTED(); }
  void NotifyNavigationStateChanged(unsigned int);
  TabContentsDelegate* delegate() const { return delegate_; }
  void set_delegate(TabContentsDelegate* d) { delegate_ = d; }
  void AddInfoBar(InfoBarDelegate*) { NOTIMPLEMENTED(); }
  virtual void OpenURL(const GURL&, const GURL&, WindowOpenDisposition,
               PageTransition::Type);
  void AddNewContents(TabContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture) { NOTIMPLEMENTED(); }
  virtual void Activate() { NOTIMPLEMENTED(); }
  virtual bool SupportsURL(GURL*);
  virtual SiteInstance* GetSiteInstance() const { return NULL; }
  int32 GetMaxPageID();
  void UpdateMaxPageID(int32);
  virtual bool NavigateToPendingEntry(bool) { NOTIMPLEMENTED(); return true; }
  virtual DOMUIHost* AsDOMUIHost() { NOTIMPLEMENTED(); return NULL; }
  virtual std::wstring GetStatusText() const { return std::wstring(); }
  static void RegisterUserPrefs(PrefService* prefs) {
    prefs->RegisterBooleanPref(prefs::kBlockPopups, false);
  }
  static void MigrateShelfView(TabContents* from, TabContents* to) {
    NOTIMPLEMENTED();
  }
  virtual void CreateView() {}
  virtual gfx::NativeView GetNativeView() const { return NULL; }
 protected:
  typedef std::vector<ConstrainedWindow*> ConstrainedWindowList;
  ConstrainedWindowList child_windows_;
 private:
  TabContentsType type_;
  bool is_active_;
  bool is_loading_;
  GURL url_;
  std::wstring title_;
  NavigationController* controller_;
  TabContentsDelegate* delegate_;
  int32 max_page_id_;
};

class SelectFileDialog : public base::RefCountedThreadSafe<SelectFileDialog> {
 public:
  enum Type {
    SELECT_FOLDER,
    SELECT_SAVEAS_FILE,
    SELECT_OPEN_FILE,
    SELECT_OPEN_MULTI_FILE
  };
  class Listener {
   public:
  };
  void ListenerDestroyed() { NOTIMPLEMENTED(); }
  void SelectFile(Type, const std::wstring&, const std::wstring&,
                  const std::wstring&, const std::wstring&, gfx::NativeWindow,
                  void*) { NOTIMPLEMENTED(); }
  static SelectFileDialog* Create(WebContents*) {
    NOTIMPLEMENTED();
    return new SelectFileDialog;
  }
};

class DockInfo {
 public:
  bool GetNewWindowBounds(gfx::Rect*, bool*) const {
    NOTIMPLEMENTED();
    return false;
  }
  void AdjustOtherWindowBounds() const { NOTIMPLEMENTED(); }
};

class ToolbarModel {
 public:
};

class WindowSizer {
 public:
  static void GetBrowserWindowBounds(const std::wstring& app_name,
                                     const gfx::Rect& specified_bounds,
                                     gfx::Rect* window_bounds,
                                     bool* maximized) { NOTIMPLEMENTED(); }
};

//---------------------------------------------------------------------------
// These stubs are for Profile

class DownloadManager : public base::RefCountedThreadSafe<DownloadManager> {
 public:
  bool Init(Profile* profile) {
    NOTIMPLEMENTED();
    return true;
  }
  void DownloadUrl(const GURL& url, const GURL& referrer,
                   WebContents* web_contents) { NOTIMPLEMENTED(); }
  int RemoveDownloadsBetween(const base::Time remove_begin,
                             const base::Time remove_end) {
    NOTIMPLEMENTED();
    return 0;
  }
  void ClearLastDownloadPath() { NOTIMPLEMENTED(); }
  int in_progress_count() {
    NOTIMPLEMENTED();
    return 0;
  }
};

class TemplateURLFetcher {
 public:
  explicit TemplateURLFetcher(Profile* profile) { }
  bool Init(Profile* profile) {
    NOTIMPLEMENTED();
    return true;
  }
  void ScheduleDownload(const std::wstring&, const GURL&, const GURL&,
                        const gfx::NativeView, bool) { NOTIMPLEMENTED(); }
};

namespace base {
class SharedMemory;
}

class Encryptor {
 public:
  static bool EncryptWideString(const std::wstring& plaintext,
                                std::string* ciphertext) {
    NOTIMPLEMENTED();
    return false;
  }

  static bool DecryptWideString(const std::string& ciphertext,
                                std::wstring* plaintext) {
    NOTIMPLEMENTED();
    return false;
  }
};

class SpellChecker : public base::RefCountedThreadSafe<SpellChecker> {
 public:
  typedef std::wstring Language;
  SpellChecker(const std::wstring& dict_dir,
               const Language& language,
               URLRequestContext* request_context,
               const std::wstring& custom_dictionary_file_name) {}

  bool SpellCheckWord(const wchar_t* in_word,
                     int in_word_len,
                     int* misspelling_start,
                     int* misspelling_len,
                    std::vector<std::wstring>* optional_suggestions) {
    NOTIMPLEMENTED();
    return true;
  }
};

class WebAppLauncher {
 public:
  static void Launch(Profile* profile, const GURL& url) {
    NOTIMPLEMENTED();
  }
};

class URLFixerUpper {
 public:
  static std::wstring FixupRelativeFile(const std::wstring& base_dir,
                                        const std::wstring& text) {
    NOTIMPLEMENTED();
    return L"about:blank";
  }
};

//---------------------------------------------------------------------------
// These stubs are for WebContents

class WebApp : public base::RefCountedThreadSafe<WebApp> {
 public:
  class Observer {
   public:
  };
  void AddObserver(Observer* obs) { NOTIMPLEMENTED(); }
  void RemoveObserver(Observer* obs) { NOTIMPLEMENTED(); }
  void SetWebContents(WebContents*) { NOTIMPLEMENTED(); }
  SkBitmap GetFavIcon() {
    NOTIMPLEMENTED();
    return SkBitmap();
  }
};

class GearsShortcutData {
 public:
};

namespace printing {
class PrintViewManager {
 public:
  PrintViewManager(WebContents&) { }
  void Stop() { NOTIMPLEMENTED(); }
  void Destroy() { NOTIMPLEMENTED(); }
  bool OnRendererGone(RenderViewHost*) {
    NOTIMPLEMENTED();
    return false;
  }
  void DidGetPrintedPagesCount(int, int) { NOTIMPLEMENTED(); }
  void DidPrintPage(const ViewHostMsg_DidPrintPage_Params&) {
    NOTIMPLEMENTED();
  }
};
}

class PasswordManager {
 public:
  PasswordManager(WebContents*) { }
  void ClearProvisionalSave() { NOTIMPLEMENTED(); }
  void DidStopLoading() { NOTIMPLEMENTED(); }
  void PasswordFormsSeen(const std::vector<PasswordForm>&) { NOTIMPLEMENTED(); }
  void DidNavigate() { NOTIMPLEMENTED(); }
  void ProvisionallySavePassword(PasswordForm form) { NOTIMPLEMENTED(); }
  void Autofill(const PasswordForm&, const PasswordFormMap&,
                const PasswordForm* const) const {
    NOTIMPLEMENTED();
  }
};

class PluginInstaller {
 public:
  PluginInstaller(WebContents*) { }
};

class PluginService {
 public:
  static PluginService* GetInstance();
  PluginService();
  ~PluginService();
  void LoadChromePlugins(ResourceDispatcherHost* rdh) {
    NOTIMPLEMENTED();
  }
  bool HavePluginFor(const std::string& mime_type, bool allow_wildcard) {
    NOTIMPLEMENTED();
    return true;
  }
  void SetChromePluginDataDir(const FilePath& data_dir);
  FilePath GetChromePluginDataDir() { return chrome_plugin_data_dir_; }
  void GetPlugins(bool reload, std::vector<WebPluginInfo>* plugins) {}
  FilePath GetPluginPath(const GURL& url,
                         const std::string& mime_type,
                         const std::string& clsid,
                         std::string* actual_mime_type) {
    NOTIMPLEMENTED();
    return FilePath();
  }
  void OpenChannelToPlugin(ResourceMessageFilter* renderer_msg_filter,
                           const GURL& url,
                           const std::string& mime_type,
                           const std::string& clsid,
                           const std::wstring& locale,
                           IPC::Message* reply_msg) { NOTIMPLEMENTED(); }
 private:
  MessageLoop* main_message_loop_;
  ResourceDispatcherHost* resource_dispatcher_host_;
  FilePath chrome_plugin_data_dir_;
  std::wstring ui_locale_;
  Lock lock_;
  class ShutdownHandler : public base::RefCountedThreadSafe<ShutdownHandler> {
  };
  scoped_refptr<ShutdownHandler> plugin_shutdown_handler_;
};

class HungRendererWarning {
 public:
  static void HideForWebContents(WebContents*) { NOTIMPLEMENTED(); }
  static void ShowForWebContents(WebContents*) { NOTIMPLEMENTED(); }
};

class ConstrainedWindow {
 public:
  bool WasHidden() {
    NOTIMPLEMENTED();
    return false;
  }
  void DidBecomeSelected() { NOTIMPLEMENTED(); }
  void CloseConstrainedWindow() { NOTIMPLEMENTED(); }
};

class SSLManager {
 public:
  class Delegate {
   public:
  };
  SSLManager(NavigationController* controller, Delegate* delegate) {
    NOTIMPLEMENTED();
  }
  ~SSLManager() { }
  void NavigationStateChanged() { NOTIMPLEMENTED(); }
  static bool DeserializeSecurityInfo(const std::string&, int*, int*, int*);
  static void OnSSLCertificateError(ResourceDispatcherHost* rdh,
                                                  URLRequest* request,
                                                  int cert_error,
                                                  net::X509Certificate* cert,
                                                  MessageLoop* ui_loop);
  static std::string SerializeSecurityInfo(int cert_id,
                                           int cert_status,
                                           int security_bits) {
    NOTIMPLEMENTED();
    return std::string();
  }
  static void OnMixedContentRequest(ResourceDispatcherHost* rdh,
                                    URLRequest* request,
                                    MessageLoop* ui_loop) { NOTIMPLEMENTED(); }
  void OnMixedContent(MixedContentHandler* mixed_content) { NOTIMPLEMENTED(); }
};

class ModalHtmlDialogDelegate {
 public:
  ModalHtmlDialogDelegate(const GURL&, int, int, const std::string&,
                          IPC::Message*, WebContents*) { }
};

class CharacterEncoding {
 public:
  static std::wstring GetCanonicalEncodingNameByAliasName(
      const std::wstring&) {
    NOTIMPLEMENTED();
    return L"";
  }
};

class SimpleAlertInfoBarDelegate : public InfoBarDelegate {
 public:
  SimpleAlertInfoBarDelegate(WebContents*, const std::wstring&, void*) {}
};

#if defined(OS_MACOSX)
class FindBarMac {
 public:
  FindBarMac(WebContentsView*, gfx::NativeWindow) { }
  void Show() { }
  void Close() { }
  void StartFinding(bool&) { }
  void EndFindSession() { }
  void DidBecomeUnselected() { }
  bool IsVisible() { return false; }
  bool IsAnimating() { return false; }
  gfx::NativeView GetView() { return nil; }
  std::string find_string() { return ""; }
  void OnFindReply(int, int, const gfx::Rect&, int, bool) { }
};
#endif

class LoginHandler {
 public:
  void SetAuth(const std::wstring& username,
               const std::wstring& password) {
    NOTIMPLEMENTED();
  }
  void CancelAuth() { NOTIMPLEMENTED(); }
  void OnRequestCancelled() { NOTIMPLEMENTED(); }
};

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request,
                                MessageLoop* ui_loop);

class ExternalProtocolHandler {
 public:
  static void LaunchUrl(const GURL& url, int render_process_host_id,
                        int tab_contents_id) {
    NOTIMPLEMENTED();
  }
};

class RepostFormWarningDialog {
 public:
  static void RunRepostFormWarningDialog(NavigationController*) { }
  virtual ~RepostFormWarningDialog() { }
};

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
