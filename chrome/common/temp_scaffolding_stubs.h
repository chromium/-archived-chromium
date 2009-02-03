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
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/gfx/rect.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/search_engines/template_url.h"
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
class ProfileManager;
class Profile;
class RenderProcessHost;
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
  bool NotifyOtherProcess() { return false; }
  void HuntForZombieChromeProcesses() { }
  void Create() { }
  void Lock() { }
  void Unlock() { }
};

class GoogleUpdateSettings {
 public:
  static bool GetCollectStatsConsent() { return false; }
  static bool SetCollectStatsConsent(bool consented) { return false; }
  static bool GetBrowser(std::wstring* browser) { return false; }
  static bool GetLanguage(std::wstring* language) { return false; }
  static bool GetBrand(std::wstring* brand) { return false; }
  static bool GetReferral(std::wstring* referral) { return false; }
  static bool ClearReferral() { return false; }
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GoogleUpdateSettings);
};

class AutomationProviderList {
 public:
  static AutomationProviderList* GetInstance() { return NULL; }
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
  void WindowClosed(const SessionID &) { }
  void SetWindowBounds(const SessionID&, const gfx::Rect&, bool) { }
  void ResetFromCurrentBrowsers() { }
  void TabRestored(NavigationController*) { }
};

class SessionRestore {
 public:
  static void RestoreSession(Profile* profile,
                             Browser* browser,
                             bool clobber_existing_window,
                             bool always_create_tabbed_browser,
                             const std::vector<GURL>& urls_to_open) {}
  static void RestoreSessionSynchronously(
      Profile* profile,
      const std::vector<GURL>& urls_to_open) {}
  
  static size_t num_tabs_to_load_;
};

class TabRestoreService : public base::RefCountedThreadSafe<TabRestoreService> {
 public:
  explicit TabRestoreService(Profile* profile) { }
  void BrowserClosing(Browser*) { }
  void BrowserClosed(Browser*) { }
  void CreateHistoricalTab(NavigationController*) { }
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
  bool Init(const FilePath& history_dir, BookmarkService* bookmark_service)
      { return false; }
  void SetOnBackendDestroyTask(Task*) {}
  void AddPage(GURL const&, void const*, int, GURL const&,
               int, std::vector<GURL> const&) {}
  void AddPage(const GURL& url) {}
  void SetPageContents(const GURL& url, const std::wstring& contents) {}
  void IterateURLs(URLEnumerator* iterator) {}
  void Cleanup() {}
  void AddRef() {}
  void Release() {}
  void SetFavIconOutOfDateForPage(const GURL&) {}
  void SetPageThumbnail(const GURL&, const SkBitmap&, const ThumbnailScore&) {}
  void SetPageTitle(const GURL&, const std::wstring&) { }
};

namespace history {
class HistoryDatabase {
 public:
  static std::string GURLToDatabaseURL(const GURL& url) { return ""; }
};
}

class MetricsService {
 public:
  MetricsService() { }
  ~MetricsService() { }
  void Start() { }
  void StartRecordingOnly() { }
  void Stop() { }
  void SetUserPermitsUpload(bool enabled) { }
  void RecordCleanShutdown() {}
  void RecordStartOfSessionEnd() {}
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
  void OnQuit() {}
};

}  // namespace printing

class SafeBrowsingService
    : public base::RefCountedThreadSafe<SafeBrowsingService> {
 public:
  void ShutDown() {}
};

class DownloadFileManager
    : public base::RefCountedThreadSafe<DownloadFileManager> {
 public:
  void Shutdown() {}
};

class SaveFileManager : public base::RefCountedThreadSafe<SaveFileManager> {
 public:
  void Shutdown() {}
};

namespace sandbox {

class BrokerServices {
 public:
  void Init() {}
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
  
  void Initialize() {}
  void Shutdown() {}
  
  SafeBrowsingService* safe_browsing_service() {
    return const_cast<SafeBrowsingService*>(&safe_browsing_service_);
  }
  
  DownloadFileManager* download_file_manager() {
    return const_cast<DownloadFileManager*>(&download_file_manager_);
  }
  
  SaveFileManager* save_file_manager() {
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
  static bool IsSavableContents(const std::string&) { return false; }
  static bool IsSavableURL(const GURL& url) { return false; }
  static std::wstring GetSuggestNameForSaveAs(PrefService*,
                                              const std::wstring&) {
    return L"";
  }
  static bool GetSaveInfo(const std::wstring&, void*,
                          SavePackageParam*, DownloadManager*) { return false; }
  SavePackage(WebContents*, SavePackageType, const std::wstring&,
              const std::wstring&) { }
  bool Init() { return true; }
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

class NavigationEntry {
 public:
  const GURL& url() const { return url_; }
  PageTransition::Type transition_type() const {
    return PageTransition::LINK;
  }
  int page_id() { return 0; }
  SiteInstance* site_instance() const { return NULL; }
  std::string content_state() const { return ""; }
  void set_content_state(const std::string&) { }
  void set_display_url(const GURL&) { }
  bool has_display_url() const { return false; }
  const GURL& display_url() const { return url_; }
  void set_url(const GURL& url) { url_ = url; }
  TabContentsType tab_type() const { return TAB_CONTENTS_WEB; }
  const GURL& user_typed_url() const { return url_; }
  const FaviconStatus& favicon() const { return favicon_status_; }
  std::wstring title() { return L""; }
  void set_title(const std::wstring&) { }
 private:
  GURL url_;
  FaviconStatus favicon_status_;
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
  NavigationController() : entry_(new NavigationEntry()) { }
  virtual ~NavigationController() { }
  bool CanGoBack() const { return false; }
  bool CanGoForward() const { return false; }
  void GoBack() { }
  void GoForward() { }
  void Reload(bool) { }
  NavigationController* Clone() { return new NavigationController(); }
  TabContents* active_contents() const { return NULL; }
  NavigationEntry* GetLastCommittedEntry() const { return entry_.get(); }
  NavigationEntry* GetActiveEntry() const { return entry_.get(); }
  void LoadURL(const GURL& url, const GURL& referrer,
               PageTransition::Type type) { }
  NavigationEntry* GetPendingEntry() { return entry_.get(); }
  bool RendererDidNavigate(const ViewHostMsg_FrameNavigate_Params&,
                           LoadCommittedDetails*) { return true; }
  int GetEntryIndexWithPageID(TabContentsType, SiteInstance*,
                              int32) const { return 0; }
  NavigationEntry* GetEntryWithPageID(TabContentsType, SiteInstance*,
                                      int32) const { return entry_.get(); }
  NavigationEntry* GetEntryAtIndex(int index) const { return entry_.get(); }
  NavigationEntry* GetEntryAtOffset(int index) const { return entry_.get(); }
  int GetLastCommittedEntryIndex() const { return 0; }
  int GetEntryCount() const { return 1; }
  int GetCurrentEntryIndex() const { return 0; }
  void NotifyEntryChanged(NavigationEntry*, int) { }
  bool IsURLInPageNavigation(const GURL&) { return false; }
  void DiscardNonCommittedEntries() { }
  void GoToOffset(int) { }
  static void DisablePromptOnRepost() { }
  int max_restored_page_id() { return 0; }
 private:
  scoped_ptr<NavigationEntry> entry_;
};

class TabContentsDelegate {
 public:
  virtual void OpenURL(const GURL&, const GURL&, WindowOpenDisposition,
                      PageTransition::Type) { }
  virtual void UpdateTargetURL(TabContents*, const GURL&) { }
  virtual void CloseContents(TabContents*) { }
  virtual void MoveContents(TabContents*, const gfx::Rect&) { }
  virtual bool IsPopup(TabContents*) { return false; }
  virtual void ForwardMessageToExternalHost(const std::string&,
                                           const std::string&) { }
  virtual TabContents* GetConstrainingContents(TabContents*) { return NULL; }
  virtual void ShowHtmlDialog(ModalHtmlDialogDelegate*, void*) { }
  virtual bool CanBlur() { return true; }
  virtual bool IsExternalTabContainer() { return false; }
  virtual void BeforeUnloadFired(WebContents*, bool, bool*) { }
  virtual void URLStarredChanged(WebContents*, bool) { }
  virtual void ConvertContentsToApplication(WebContents*) { }
};

class InterstitialPage {
 public:
  virtual void DontProceed() { }
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
  virtual void DidBecomeSelected() { }
  virtual bool WasHidden() { return false; }
  virtual void SetSize(gfx::Size) { }
};

class RenderWidgetHost {
 public:
  RenderWidgetHost() : process_(), view_() { }
  RenderProcessHost* process() const { return process_; }
  RenderWidgetHostView* view() const { return view_; }
 private:
  RenderProcessHost* process_;
  RenderWidgetHostView* view_;
};

class RenderViewHost : public RenderWidgetHost {
 public:
  bool HasUnloadListener() const { return false; }
  void FirePageBeforeUnload() { }
  void SetPageEncoding(const std::wstring& encoding) { }
  SiteInstance* site_instance() const { return NULL; }
  void NavigateToEntry(const NavigationEntry& entry, bool is_reload) { }
  void Cut() { }
  void Copy() { }
  void Paste() { }
  void DisassociateFromPopupCount() { }
  void PopupNotificationVisibilityChanged(bool) { }
  void GetApplicationInfo(int32 page_id) { }
  bool PrintPages() { return false; }
  void SetInitialFocus(bool) { }
  void UnloadListenerHasFired() { }
  bool IsRenderViewLive() { return true; }
  void FileSelected(const std::wstring&) { }
  void MultiFilesSelected(const std::vector<std::wstring>&) { }
  bool CreateRenderView() { return true; }
  void SetAlternateErrorPageURL(const GURL&) { }
  void UpdateWebPreferences(WebPreferences) { }
  void ReservePageIDRange(int) { }
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
  NavigationController* controller() const { return controller_.get(); }
  virtual WebContents* AsWebContents() const { return NULL; }
  virtual SkBitmap GetFavIcon() const { return SkBitmap(); }
  const GURL& GetURL() const { return url_; }
  virtual const std::wstring& GetTitle() const { return title_; }
  TabContentsType type() const { return TAB_CONTENTS_WEB; }
  virtual void Focus() { }
  virtual void Stop() { }
  bool is_loading() const { return false; }
  Profile* profile() const { return NULL; }
  void CloseContents() { };
  void SetupController(Profile* profile) { }
  bool WasHidden() { return false; }
  void RestoreFocus() { }
  static TabContentsType TypeForURL(GURL* url) { return TAB_CONTENTS_WEB; }
  static TabContents* CreateWithType(TabContentsType type,
                                     Profile* profile,
                                     SiteInstance* instance);
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) { }
  virtual void DidBecomeSelected() { }
  virtual void SetDownloadShelfVisible(bool) { }
  void Destroy() { }
  virtual void SetIsLoading(bool, LoadNotificationDetails*) { }
  virtual void SetIsCrashed(bool) { }
  bool capturing_contents() const { return false; }
  void set_capturing_contents(bool) { }
  bool is_active() { return false; }
  void SetNotWaitingForResponse() { }
  void NotifyNavigationStateChanged(int) { }
  TabContentsDelegate* delegate() const { return NULL; }
  void AddInfoBar(InfoBarDelegate* delegate) {}
  void OpenURL(const GURL&, const GURL&, WindowOpenDisposition,
               PageTransition::Type) { }
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
  void ListenerDestroyed() { }
  void SelectFile(Type, const std::wstring&, const std::wstring&,
                  const std::wstring&, const std::wstring&, gfx::NativeView,
                  void*) {}
  static SelectFileDialog* Create(WebContents*) { return new SelectFileDialog; }
};

class SiteInstance {
 public:
  bool has_site() { return false; }
  void SetSite(const GURL&) { }
  int max_page_id() { return 0; }
  void UpdateMaxPageID(int) { }
};

class DockInfo {
 public:
  bool GetNewWindowBounds(gfx::Rect*, bool*) const { return false; }
  void AdjustOtherWindowBounds() const { }
};

class ToolbarModel {
 public:
};

class WindowSizer {
 public:
  static void GetBrowserWindowBounds(const std::wstring& app_name,
                                     const gfx::Rect& specified_bounds,
                                     gfx::Rect* window_bounds,
                                     bool* maximized) { }
};

//---------------------------------------------------------------------------
// These stubs are for Profile

class DownloadManager : public base::RefCountedThreadSafe<DownloadManager> {
 public:
  bool Init(Profile* profile) { return true; }
  void DownloadUrl(const GURL& url, const GURL& referrer,
                   WebContents* web_contents) { }
  int in_progress_count() { return 0; }
};

class TemplateURLFetcher {
 public:
  explicit TemplateURLFetcher(Profile* profile) { }
  bool Init(Profile* profile) { return true; }
  void ScheduleDownload(const std::wstring&, const GURL&, const GURL&,
                        const gfx::NativeView, bool) { }
};

namespace base {
class SharedMemory;
}

class Encryptor {
 public:
  static bool EncryptWideString(const std::wstring& plaintext,
                                std::string* ciphertext) { return false; }

  static bool DecryptWideString(const std::string& ciphertext,
                                std::wstring* plaintext) { return false; }
};

class BookmarkModel : public BookmarkService {
 public:
  explicit BookmarkModel(Profile* profile) { }
  virtual ~BookmarkModel() { }
  void Load() { }
  virtual bool IsBookmarked(const GURL& url) { return false; }
  virtual void GetBookmarks(std::vector<GURL>* urls) { }
  virtual void BlockTillLoaded() { }
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
  }
};

class AutocompleteResult {
 public:
  static void set_max_matches(int) { }
};

class AutocompleteProvider {
 public:
  static void set_max_matches(int) {}
};

class URLFixerUpper {
 public:
  static std::wstring FixupRelativeFile(const std::wstring& base_dir,
                                        const std::wstring& text) {
    return L"about:blank";
  }
};

class TemplateURLModel {
 public:
  explicit TemplateURLModel(Profile* profile) { }
  static std::wstring GenerateKeyword(const GURL& url, bool autodetected) {
    return L"";
  }
  static GURL GenerateSearchURL(const TemplateURL* t_url) { return GURL(); }
  TemplateURL* GetDefaultSearchProvider() { return NULL; }
  bool loaded() const { return false; }
  void Load() { }
  TemplateURL* GetTemplateURLForKeyword(const std::wstring&) { return NULL; }
  void ScheduleDownload(const std::wstring&, const GURL&, const GURL&,
                        const gfx::NativeView, bool) { }
  bool CanReplaceKeyword(const std::wstring&, const std::wstring&,
                         const TemplateURL**) {
    return false;
  }
  void Remove(const TemplateURL*) { }
  void Add(const TemplateURL*) { }
};

//---------------------------------------------------------------------------
// These stubs are for WebContents

class WebContentsView : public RenderViewHostDelegate::View {
 public:
  void OnContentsDestroy() { }
  void* GetNativeView() { return NULL; }
  void HideFindBar(bool) { }
  void Invalidate() { }
  static WebContentsView* Create(WebContents*) { return new WebContentsView; }
  gfx::NativeView GetTopLevelNativeView() const { return NULL; }
  gfx::Size GetContainerSize() const { return gfx::Size(); }
  void SizeContents(const gfx::Size& size) { }
  RenderWidgetHostView* CreateViewForWidget(RenderWidgetHost*) {
    return NULL;
  }
  void RenderWidgetHostDestroyed(RenderWidgetHost*) { }
  void SetPageTitle(const std::wstring&) { }
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
  RenderViewHost* current_host() const { return render_view_host_; }
  void Init(Profile*, SiteInstance*, int, base::WaitableEvent*) { }
  void Shutdown() { }
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
  void CrossSiteNavigationCanceled() { }
  void ShouldClosePage(bool) { }
  void OnCrossSiteResponse(int, int) { }
  RenderViewHost* Navigate(const NavigationEntry&) { return render_view_host_; }
  void Stop() { }
  void OnJavaScriptMessageBoxClosed(IPC::Message*, bool,
                                    const std::wstring&) { }
  void SetIsLoading(bool) { }
  void DidNavigateMainFrame(RenderViewHost*) { }
  void RendererAbortedProvisionalLoad(RenderViewHost*) { }
  bool ShouldCloseTabOnUnresponsiveRenderer() { return false; }
 private:
  RenderViewHost* render_view_host_;
  InterstitialPage* interstitial_page_;
};

class WebApp : public base::RefCountedThreadSafe<WebApp> {
 public:
  class Observer {
   public:
  };
  void AddObserver(Observer* obs) { }
  void RemoveObserver(Observer* obs) { }
  void SetWebContents(WebContents*) { }
  SkBitmap GetFavIcon() { return SkBitmap(); }
};

class GearsShortcutData {
 public:
};

namespace printing {
class PrintViewManager {
 public:
  PrintViewManager(WebContents&) { }
  void Stop() { }
  void Destroy() { }
  bool OnRendererGone(RenderViewHost*) { return false; }
  void DidGetPrintedPagesCount(int, int) { }
  void DidPrintPage(const ViewHostMsg_DidPrintPage_Params&) { }
};
}

class FavIconHelper {
 public:
  FavIconHelper(WebContents*) { }
  void SetFavIconURL(const GURL&) { }
  void SetFavIcon(int, const GURL&, const SkBitmap&) { }
  void FavIconDownloadFailed(int) { }
  void FetchFavIcon(const GURL&) { }
};

class AutofillManager {
 public:
  AutofillManager(WebContents*) { }
  void AutofillFormSubmitted(const AutofillForm&) { }
  void FetchValuesForName(const std::wstring&, const std::wstring&, int,
                         int64, int) { }
};

class PasswordManager {
 public:
  PasswordManager(WebContents*) { }
  void ClearProvisionalSave() { }
  void DidStopLoading() { }
  void PasswordFormsSeen(const std::vector<PasswordForm>&) { }
  void DidNavigate() { }
  void ProvisionallySavePassword(PasswordForm form) { }
};

class PluginInstaller {
 public:
  PluginInstaller(WebContents*) { }
};

class HungRendererWarning {
 public:
  static void HideForWebContents(WebContents*) { }
  static void ShowForWebContents(WebContents*) { }
};

class ConstrainedWindow {
 public:
  bool WasHidden() { return false; }
  void DidBecomeSelected() { }
  void CloseConstrainedWindow() { }
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
      const std::wstring&) { return L""; }
};

class SimpleAlertInfoBarDelegate : public InfoBarDelegate {
 public:
  SimpleAlertInfoBarDelegate(WebContents*, const std::wstring&, void*) {}
};

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
