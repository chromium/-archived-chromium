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
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/gfx/rect.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class BookmarkService;
class CommandLine;
class ConstrainedWindow;
class DownloadManager;
class HistoryService;
class MetricsService;
class ModalHtmlDialogDelegate;
class NavigationController;
class NavigationEntry;
class NotificationService;
class PluginService;
class ProfileManager;
class Profile;
class RenderProcessHost;
class RenderWidgetHelper;
class SessionID;
class SiteInstance;
class SpellChecker;
class TabContents;
struct ThumbnailScore;
class Task;
class TemplateURL;
class TemplateURLRef;
class URLRequestContext;
class UserScriptMaster;
class VisitedLinkMaster;
class WebContents;

namespace IPC {
class Message;
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

class SessionService : public base::RefCountedThreadSafe<SessionService> {
 public:
  explicit SessionService(Profile* profile) { }
  void WindowClosed(const SessionID &) { NOTIMPLEMENTED(); }
  void SetWindowBounds(const SessionID&, const gfx::Rect&, bool) {
    NOTIMPLEMENTED();
  }
  void ResetFromCurrentBrowsers() { NOTIMPLEMENTED(); }
  void TabRestored(NavigationController*) { NOTIMPLEMENTED(); }
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

class TabRestoreService : public base::RefCountedThreadSafe<TabRestoreService> {
 public:
  explicit TabRestoreService(Profile* profile) { }
  void BrowserClosing(Browser*) { NOTIMPLEMENTED(); }
  void BrowserClosed(Browser*) { NOTIMPLEMENTED(); }
  void CreateHistoricalTab(NavigationController*) { NOTIMPLEMENTED(); }
};

class HistoryService {
 public:
  class URLEnumerator {
   public:
    virtual ~URLEnumerator() {}
    virtual void OnURL(const GURL& url) = 0;
    virtual void OnComplete(bool success) = 0;
  };
  class Handle {
   public:
  };
  HistoryService() {}
  HistoryService(Profile* profile) {}
  bool Init(const FilePath& history_dir, BookmarkService* bookmark_service) {
    NOTIMPLEMENTED();
    return false;
  }
  void SetOnBackendDestroyTask(Task*) { NOTIMPLEMENTED(); }
  void AddPage(GURL const&, void const*, int, GURL const&,
               int, std::vector<GURL> const&) { NOTIMPLEMENTED(); }
  void AddPage(const GURL& url) { NOTIMPLEMENTED(); }
  void SetPageContents(const GURL& url, const std::wstring& contents) {
    NOTIMPLEMENTED();
  }
  void IterateURLs(URLEnumerator* iterator) { NOTIMPLEMENTED(); }
  void Cleanup() { NOTIMPLEMENTED(); }
  void AddRef() { NOTIMPLEMENTED(); }
  void Release() { NOTIMPLEMENTED(); }
  void SetFavIconOutOfDateForPage(const GURL&) { NOTIMPLEMENTED(); }
  void SetPageThumbnail(const GURL&, const SkBitmap&, const ThumbnailScore&) {
    NOTIMPLEMENTED();
  }
  void SetPageTitle(const GURL&, const std::wstring&) {
    NOTIMPLEMENTED();
  }
};

namespace history {
class HistoryDatabase {
 public:
  static std::string GURLToDatabaseURL(const GURL& url) {
    NOTIMPLEMENTED();
    return "";
  }
};
}

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

namespace browser_shutdown {
void ReadLastShutdownInfo();
void Shutdown();
}

namespace browser {
void RegisterAllPrefs(PrefService*, PrefService*);
}

void OpenFirstRunDialog(Profile* profile);

void InstallJankometer(const CommandLine&);

GURL NewTabUIURL();

//---------------------------------------------------------------------------
// These stubs are for BrowserProcessImpl

class ClipboardService {
};

namespace printing {

class PrintJobManager {
 public:
  void OnQuit() { NOTIMPLEMENTED(); }
};

}  // namespace printing

class SafeBrowsingService
    : public base::RefCountedThreadSafe<SafeBrowsingService> {
 public:
  void ShutDown() { NOTIMPLEMENTED(); }
};

class DownloadFileManager
    : public base::RefCountedThreadSafe<DownloadFileManager> {
 public:
  void Shutdown() { NOTIMPLEMENTED(); }
};

class SaveFileManager : public base::RefCountedThreadSafe<SaveFileManager> {
 public:
  void Shutdown() { NOTIMPLEMENTED(); }
};

namespace sandbox {

class BrokerServices {
 public:
  void Init() { NOTIMPLEMENTED(); }
};

}  // namespace sandbox

class IconManager {
};

struct ViewHostMsg_Resource_Request;

class ResourceDispatcherHost {
 public:
  explicit ResourceDispatcherHost(MessageLoop* loop) {}
  class Receiver {
   public:
    virtual bool Send(IPC::Message* message) = 0;
  };
  void OnClosePageACK(int, int);
  void CancelRequestsForRenderView(int, int);
  void Initialize() { NOTIMPLEMENTED(); }
  void Shutdown() { NOTIMPLEMENTED(); }
  SafeBrowsingService* safe_browsing_service() {
    NOTIMPLEMENTED();
    return const_cast<SafeBrowsingService*>(&safe_browsing_service_);
  }
  DownloadFileManager* download_file_manager() {
    NOTIMPLEMENTED();
    return const_cast<DownloadFileManager*>(&download_file_manager_);
  }
  SaveFileManager* save_file_manager() {
    NOTIMPLEMENTED();
    return const_cast<SaveFileManager*>(&save_file_manager_);
  }
 private:
  SafeBrowsingService safe_browsing_service_;
  DownloadFileManager download_file_manager_;
  SaveFileManager save_file_manager_;
};

class DebuggerWrapper : public base::RefCountedThreadSafe<DebuggerWrapper> {
 public:
  explicit DebuggerWrapper(int port) {}
};

namespace views {

class AcceleratorHandler {
};

}  // namespace views

//---------------------------------------------------------------------------
// These stubs are for Browser

class RenderViewHostDelegate {
 public:
  class View {
   public:
  };
  class Save {
   public:
  };
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
    std::wstring saved_main_file_path;
    std::wstring dir;
  };
  static bool IsSavableContents(const std::string&) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool IsSavableURL(const GURL& url) {
    NOTIMPLEMENTED();
    return false;
  }
  static std::wstring GetSuggestNameForSaveAs(PrefService*,
                                              const std::wstring&) {
    NOTIMPLEMENTED();
    return L"";
  }
  static bool GetSaveInfo(const std::wstring&, void*,
                          SavePackageParam*, DownloadManager*) {
    NOTIMPLEMENTED();
    return false;
  }
  SavePackage(WebContents*, SavePackageType, const std::wstring&,
              const std::wstring&) { NOTIMPLEMENTED(); }
  bool Init() {
    NOTIMPLEMENTED();
    return true;
  }
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

class NavigationController {
 public:
  struct LoadCommittedDetails {
    NavigationEntry* entry;
    bool is_main_frame;
    GURL previous_url;
    NavigationType::Type type;
    bool is_auto;
    bool is_in_page;
    bool is_content_filtered;
    std::string serialized_security_info;
    bool is_user_initiated_main_frame_load() const { return true; }
  };
  NavigationController() : entry_(new NavigationEntry(TAB_CONTENTS_WEB)) { }
  virtual ~NavigationController() { }
  bool CanGoBack() const {
    NOTIMPLEMENTED();
    return false;
  }
  bool CanGoForward() const {
    NOTIMPLEMENTED();
    return false;
  }
  void GoBack() { NOTIMPLEMENTED(); }
  void GoForward() { NOTIMPLEMENTED(); }
  void Reload(bool) { NOTIMPLEMENTED(); }
  NavigationController* Clone() {
    NOTIMPLEMENTED();
    return new NavigationController();
  }
  TabContents* active_contents() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  NavigationEntry* GetLastCommittedEntry() const {
    NOTIMPLEMENTED();
    return entry_.get();
  }
  NavigationEntry* GetActiveEntry() const {
    NOTIMPLEMENTED();
    return entry_.get();
  }
  void LoadURL(const GURL& url, const GURL& referrer,
               PageTransition::Type type) { NOTIMPLEMENTED(); }
  NavigationEntry* GetPendingEntry() {
    NOTIMPLEMENTED();
    return entry_.get();
  }
  bool RendererDidNavigate(const ViewHostMsg_FrameNavigate_Params&,
                           LoadCommittedDetails*) {
    NOTIMPLEMENTED();
    return true;
  }
  int GetEntryIndexWithPageID(TabContentsType, SiteInstance*,
                              int32) const {
    NOTIMPLEMENTED();
    return 0;
  }
  NavigationEntry* GetEntryWithPageID(TabContentsType, SiteInstance*,
                                      int32) const {
    NOTIMPLEMENTED();
    return entry_.get();
  }
  NavigationEntry* GetEntryAtIndex(int index) const {
    NOTIMPLEMENTED();
    return entry_.get();
  }
  NavigationEntry* GetEntryAtOffset(int index) const {
    NOTIMPLEMENTED();
    return entry_.get();
  }
  int GetLastCommittedEntryIndex() const {
    NOTIMPLEMENTED();
    return 0;
  }
  int GetEntryCount() const {
    NOTIMPLEMENTED();
    return 1;
  }
  int GetCurrentEntryIndex() const {
    NOTIMPLEMENTED();
    return 0;
  }
  void NotifyEntryChanged(NavigationEntry*, int) { NOTIMPLEMENTED(); }
  bool IsURLInPageNavigation(const GURL&) {
    NOTIMPLEMENTED();
    return false;
  }
  void DiscardNonCommittedEntries() { NOTIMPLEMENTED(); }
  void GoToOffset(int) { NOTIMPLEMENTED(); }
  static void DisablePromptOnRepost() { NOTIMPLEMENTED(); }
  int max_restored_page_id() {
    NOTIMPLEMENTED();
    return 0;
  }
 private:
  scoped_ptr<NavigationEntry> entry_;
};

class TabContentsDelegate {
 public:
  virtual void OpenURL(const GURL&, const GURL&, WindowOpenDisposition,
                       PageTransition::Type) { NOTIMPLEMENTED(); }
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
  virtual bool IsExternalTabContainer() {
    NOTIMPLEMENTED();
    return false;
  }
  virtual void BeforeUnloadFired(WebContents*, bool, bool*) {
    NOTIMPLEMENTED();
  }
  virtual void URLStarredChanged(WebContents*, bool) { NOTIMPLEMENTED(); }
  virtual void ConvertContentsToApplication(WebContents*) { NOTIMPLEMENTED(); }
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

class RenderWidgetHostView {
 public:
  virtual void DidBecomeSelected() { NOTIMPLEMENTED(); }
  virtual bool WasHidden() {
    NOTIMPLEMENTED();
    return false;
  }
  virtual void SetSize(gfx::Size) { NOTIMPLEMENTED(); }
};

class RenderWidgetHost {
 public:
  RenderWidgetHost() : process_(), view_() { }
  RenderProcessHost* process() const {
    NOTIMPLEMENTED();
    return process_;
  }
  RenderWidgetHostView* view() const {
    NOTIMPLEMENTED();
    return view_;
  }
 private:
  RenderProcessHost* process_;
  RenderWidgetHostView* view_;
};

class RenderViewHost : public RenderWidgetHost {
 public:
  bool HasUnloadListener() const {
    NOTIMPLEMENTED();
    return false;
  }
  void FirePageBeforeUnload() { NOTIMPLEMENTED(); }
  void SetPageEncoding(const std::wstring& encoding) { NOTIMPLEMENTED(); }
  SiteInstance* site_instance() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  void NavigateToEntry(const NavigationEntry& entry, bool is_reload) {
    NOTIMPLEMENTED();
  }
  void Cut() { NOTIMPLEMENTED(); }
  void Copy() { NOTIMPLEMENTED(); }
  void Paste() { NOTIMPLEMENTED(); }
  void DisassociateFromPopupCount() { NOTIMPLEMENTED(); }
  void PopupNotificationVisibilityChanged(bool) { NOTIMPLEMENTED(); }
  void GetApplicationInfo(int32 page_id) { NOTIMPLEMENTED(); }
  bool PrintPages() {
    NOTIMPLEMENTED();
    return false;
  }
  void SetInitialFocus(bool) { NOTIMPLEMENTED(); }
  void UnloadListenerHasFired() { NOTIMPLEMENTED(); }
  bool IsRenderViewLive() {
    NOTIMPLEMENTED();
    return true;
  }
  void FileSelected(const std::wstring&) { NOTIMPLEMENTED(); }
  void MultiFilesSelected(const std::vector<std::wstring>&) {
    NOTIMPLEMENTED();
  }
  bool CreateRenderView() {
    NOTIMPLEMENTED();
    return true;
  }
  void SetAlternateErrorPageURL(const GURL&) { NOTIMPLEMENTED(); }
  void UpdateWebPreferences(WebPreferences) { NOTIMPLEMENTED(); }
  void ReservePageIDRange(int) { NOTIMPLEMENTED(); }
};

class LoadNotificationDetails {
 public:
  LoadNotificationDetails(const GURL&, PageTransition::Type,
                          base::TimeDelta, NavigationController*, int) { }
};

class TabContents : public NotificationObserver {
 public:
  enum InvalidateTypes {
    INVALIDATE_URL = 1,
    INVALIDATE_TITLE = 2,
    INVALIDATE_FAVICON = 4,
    INVALIDATE_LOAD = 8,
    INVALIDATE_EVERYTHING = 0xFFFFFFFF
  };
  TabContents(TabContentsType) : controller_(new NavigationController) { }
  virtual ~TabContents() { }
  NavigationController* controller() const {
    NOTIMPLEMENTED();
    return controller_.get();
  }
  virtual WebContents* AsWebContents() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual SkBitmap GetFavIcon() const {
    NOTIMPLEMENTED();
    return SkBitmap();
  }
  const GURL& GetURL() const {
    NOTIMPLEMENTED();
    return url_;
  }
  virtual const std::wstring& GetTitle() const {
    NOTIMPLEMENTED();
    return title_;
  }
  TabContentsType type() const {
    NOTIMPLEMENTED();
    return TAB_CONTENTS_WEB;
  }
  virtual void Focus() { NOTIMPLEMENTED(); }
  virtual void Stop() { NOTIMPLEMENTED(); }
  bool is_loading() const {
    NOTIMPLEMENTED();
    return false;
  }
  Profile* profile() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  void CloseContents() { NOTIMPLEMENTED(); };
  void SetupController(Profile* profile) { NOTIMPLEMENTED(); }
  bool WasHidden() {
    NOTIMPLEMENTED();
    return false;
  }
  void RestoreFocus() { NOTIMPLEMENTED(); }
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
  void Destroy() { NOTIMPLEMENTED(); }
  virtual void SetIsLoading(bool, LoadNotificationDetails*) {
    NOTIMPLEMENTED();
  }
  virtual void SetIsCrashed(bool) { NOTIMPLEMENTED(); }
  bool capturing_contents() const {
    NOTIMPLEMENTED();
    return false;
  }
  void set_capturing_contents(bool) { NOTIMPLEMENTED(); }
  bool is_active() {
    NOTIMPLEMENTED();
    return false;
  }
  void SetNotWaitingForResponse() { NOTIMPLEMENTED(); }
  void NotifyNavigationStateChanged(int) { NOTIMPLEMENTED(); }
  TabContentsDelegate* delegate() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  void AddInfoBar(InfoBarDelegate* delegate) { NOTIMPLEMENTED(); }
  void OpenURL(const GURL&, const GURL&, WindowOpenDisposition,
               PageTransition::Type) { NOTIMPLEMENTED(); }
 protected:
  typedef std::vector<ConstrainedWindow*> ConstrainedWindowList;
  ConstrainedWindowList child_windows_;
 private:
  GURL url_;
  std::wstring title_;
  scoped_ptr<NavigationController> controller_;
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
                  const std::wstring&, const std::wstring&, gfx::NativeView,
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

class BookmarkModel : public BookmarkService {
 public:
  explicit BookmarkModel(Profile* profile) { }
  virtual ~BookmarkModel() { }
  void Load() { NOTIMPLEMENTED(); }
  virtual bool IsBookmarked(const GURL& url) {
    NOTIMPLEMENTED();
    return false;
  }
  virtual void GetBookmarks(std::vector<GURL>* urls) { NOTIMPLEMENTED(); }
  virtual void BlockTillLoaded() { NOTIMPLEMENTED(); }
};

class SpellChecker : public base::RefCountedThreadSafe<SpellChecker> {
 public:
  typedef std::wstring Language;
  SpellChecker(const std::wstring& dict_dir,
               const Language& language,
               URLRequestContext* request_context,
               const std::wstring& custom_dictionary_file_name) {}
};

class WebAppLauncher {
 public:
  static void Launch(Profile* profile, const GURL& url) {
    NOTIMPLEMENTED();
  }
};

class AutocompleteResult {
 public:
  static void set_max_matches(int) { NOTIMPLEMENTED(); }
};

class AutocompleteProvider {
 public:
  static void set_max_matches(int) { NOTIMPLEMENTED(); }
};

class URLFixerUpper {
 public:
  static std::wstring FixupRelativeFile(const std::wstring& base_dir,
                                        const std::wstring& text) {
    NOTIMPLEMENTED();
    return L"about:blank";
  }
};

class TemplateURLModel {
 public:
  explicit TemplateURLModel(Profile* profile) { }
  static std::wstring GenerateKeyword(const GURL& url, bool autodetected) {
    NOTIMPLEMENTED();
    return L"";
  }
  static GURL GenerateSearchURL(const TemplateURL* t_url) {
    NOTIMPLEMENTED();
    return GURL();
  }
  TemplateURL* GetDefaultSearchProvider() {
    NOTIMPLEMENTED();
    return NULL;
  }
  bool loaded() const {
    NOTIMPLEMENTED();
    return false;
  }
  void Load() { NOTIMPLEMENTED(); }
  TemplateURL* GetTemplateURLForKeyword(const std::wstring&) {
    NOTIMPLEMENTED();
    return NULL;
  }
  void ScheduleDownload(const std::wstring&, const GURL&, const GURL&,
                        const gfx::NativeView, bool) { NOTIMPLEMENTED(); }
  bool CanReplaceKeyword(const std::wstring&, const std::wstring&,
                         const TemplateURL**) {
    NOTIMPLEMENTED();
    return false;
  }
  void Remove(const TemplateURL*) { NOTIMPLEMENTED(); }
  void Add(const TemplateURL*) { NOTIMPLEMENTED(); }
};

//---------------------------------------------------------------------------
// These stubs are for WebContents

class WebContentsView : public RenderViewHostDelegate::View {
 public:
  void OnContentsDestroy() { NOTIMPLEMENTED(); }
  void* GetNativeView() {
    NOTIMPLEMENTED();
    return NULL;
  }
  void HideFindBar(bool) { NOTIMPLEMENTED(); }
  void Invalidate() { NOTIMPLEMENTED(); }
  static WebContentsView* Create(WebContents*) {
    NOTIMPLEMENTED();
    return new WebContentsView;
  }
  gfx::NativeView GetTopLevelNativeView() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  gfx::Size GetContainerSize() const {
    NOTIMPLEMENTED();
    return gfx::Size();
  }
  void SizeContents(const gfx::Size& size) { NOTIMPLEMENTED(); }
  RenderWidgetHostView* CreateViewForWidget(RenderWidgetHost*) {
    NOTIMPLEMENTED();
    return NULL;
  }
  void RenderWidgetHostDestroyed(RenderWidgetHost*) { NOTIMPLEMENTED(); }
  void SetPageTitle(const std::wstring&) { NOTIMPLEMENTED(); }
};

class WebContentsViewWin : public WebContentsView {
 public:
  WebContentsViewWin(WebContents*) { }
};

class RenderViewHostFactory {
 public:
};

class RenderViewHostManager {
 public:
  class Delegate {
   public:
  };
  RenderViewHostManager(RenderViewHostFactory*, RenderViewHostDelegate*,
                        Delegate* delegate)
      : render_view_host_(new RenderViewHost), interstitial_page_() { }
  RenderViewHost* current_host() const {
    NOTIMPLEMENTED();
    return render_view_host_;
  }
  void Init(Profile*, SiteInstance*, int, base::WaitableEvent*) {
    NOTIMPLEMENTED();
  }
  void Shutdown() { NOTIMPLEMENTED(); }
  InterstitialPage* interstitial_page() const {
    return interstitial_page_;
  }
  void set_interstitial_page(InterstitialPage* interstitial_page) {
    interstitial_page_ = interstitial_page;
  }
  void remove_interstitial_page() { interstitial_page_ = NULL; }
  RenderWidgetHostView* current_view() const {
    if (!render_view_host_) return NULL;
    return render_view_host_->view();
  }
  void CrossSiteNavigationCanceled() { NOTIMPLEMENTED(); }
  void ShouldClosePage(bool) { NOTIMPLEMENTED(); }
  void OnCrossSiteResponse(int, int) { NOTIMPLEMENTED(); }
  RenderViewHost* Navigate(const NavigationEntry&) {
    NOTIMPLEMENTED();
    return render_view_host_;
  }
  void Stop() { NOTIMPLEMENTED(); }
  void OnJavaScriptMessageBoxClosed(IPC::Message*, bool,
                                    const std::wstring&) { NOTIMPLEMENTED(); }
  void SetIsLoading(bool) { NOTIMPLEMENTED(); }
  void DidNavigateMainFrame(RenderViewHost*) { NOTIMPLEMENTED(); }
  void RendererAbortedProvisionalLoad(RenderViewHost*) { NOTIMPLEMENTED(); }
  bool ShouldCloseTabOnUnresponsiveRenderer() {
    NOTIMPLEMENTED();
    return false;
  }
 private:
  RenderViewHost* render_view_host_;
  InterstitialPage* interstitial_page_;
};

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

class FavIconHelper {
 public:
  FavIconHelper(WebContents*) { }
  void SetFavIconURL(const GURL&) { NOTIMPLEMENTED(); }
  void SetFavIcon(int, const GURL&, const SkBitmap&) { NOTIMPLEMENTED(); }
  void FavIconDownloadFailed(int) { NOTIMPLEMENTED(); }
  void FetchFavIcon(const GURL&) { NOTIMPLEMENTED(); }
};

class AutofillManager {
 public:
  AutofillManager(WebContents*) { }
  void AutofillFormSubmitted(const AutofillForm&) { NOTIMPLEMENTED(); }
  void FetchValuesForName(const std::wstring&, const std::wstring&, int,
                          int64, int) { NOTIMPLEMENTED(); }
};

class PasswordManager {
 public:
  PasswordManager(WebContents*) { }
  void ClearProvisionalSave() { NOTIMPLEMENTED(); }
  void DidStopLoading() { NOTIMPLEMENTED(); }
  void PasswordFormsSeen(const std::vector<PasswordForm>&) { NOTIMPLEMENTED(); }
  void DidNavigate() { NOTIMPLEMENTED(); }
  void ProvisionallySavePassword(PasswordForm form) { NOTIMPLEMENTED(); }
};

class PluginInstaller {
 public:
  PluginInstaller(WebContents*) { }
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
  static bool DeserializeSecurityInfo(const std::string&, int*, int*, int*);
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

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
