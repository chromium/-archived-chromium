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
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
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
class WebPreferences;

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

namespace history {

class ExpireHistoryBackend {
 public:
  BookmarkService* bookmark_service_;
};

class HistoryBackend : public base::RefCountedThreadSafe<HistoryBackend> {
 public:
  BookmarkService* bookmark_service_;
  ExpireHistoryBackend expirer_;
};

class HistoryDatabase {
 public:
  static std::string GURLToDatabaseURL(const GURL& url) {
    NOTIMPLEMENTED();
    return "";
  }
};

}

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
  void DeleteAllSearchTermsForKeyword(long long) { NOTIMPLEMENTED(); }
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   long long keyword_id,
                                   const std::wstring& term) {
    NOTIMPLEMENTED();
  }
  void NotifyRenderProcessHostDestruction(int) { NOTIMPLEMENTED(); };
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

  scoped_refptr<history::HistoryBackend> history_backend_;
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

struct ViewHostMsg_DidPrintPage_Params;
struct ViewHostMsg_FrameNavigate_Params;

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
  void DebugMessage(const std::wstring&) {}
  void OnDebugAttach() {}
  void OnDebugDisconnect() {}
};

namespace views {

class AcceleratorHandler {
};

}  // namespace views

//---------------------------------------------------------------------------
// These stubs are for Browser

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
  virtual void ReplaceContents(TabContents*, TabContents*) { NOTIMPLEMENTED(); }
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
  virtual void WasHidden() { NOTIMPLEMENTED(); }
  virtual gfx::NativeView GetPluginNativeView() {
    NOTIMPLEMENTED(); 
    return NULL;
  }
  virtual void Hide() { NOTIMPLEMENTED(); }
  virtual void Show() { NOTIMPLEMENTED(); }
  virtual bool HasFocus() { NOTIMPLEMENTED(); return false; }
  virtual void Focus() { NOTIMPLEMENTED(); }
  virtual void UpdateCursorIfOverSelf() { NOTIMPLEMENTED(); }
  virtual void SetTooltipText(const std::wstring& tooltip_text) 
      { NOTIMPLEMENTED(); }
  virtual void SetSize(gfx::Size) { NOTIMPLEMENTED(); }
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
  TabContents(TabContentsType) : controller_() { }
  virtual ~TabContents() { }
  NavigationController* controller() const { return controller_; }
  void set_controller(NavigationController* c) { controller_ = c; }
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
  Profile* profile() const;
  void CloseContents() { NOTIMPLEMENTED(); };
  void SetupController(Profile* profile);
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
    return true;
  }
  void set_is_active(bool) { NOTIMPLEMENTED(); }
  void SetNotWaitingForResponse() { NOTIMPLEMENTED(); }
  void NotifyNavigationStateChanged(int) { NOTIMPLEMENTED(); }
  TabContentsDelegate* delegate() const {
    NOTIMPLEMENTED();
    return NULL;
  }
  void AddInfoBar(InfoBarDelegate*) { NOTIMPLEMENTED(); }
  void OpenURL(const GURL&, const GURL&, WindowOpenDisposition,
               PageTransition::Type) { NOTIMPLEMENTED(); }
  virtual void Activate() { NOTIMPLEMENTED(); }
  virtual bool SupportsURL(GURL*) { NOTIMPLEMENTED(); return false; }
  virtual SiteInstance* GetSiteInstance() const { return NULL; }
  int32 GetMaxPageID() { NOTIMPLEMENTED(); return 0; }
  void UpdateMaxPageID(int32) { NOTIMPLEMENTED(); }
  virtual bool NavigateToPendingEntry(bool) { NOTIMPLEMENTED(); return true; }
  virtual DOMUIHost* AsDOMUIHost() { NOTIMPLEMENTED(); return NULL; }
  static void RegisterUserPrefs(PrefService* prefs) {
    prefs->RegisterBooleanPref(prefs::kBlockPopups, false);
  }
  static void MigrateShelfView(TabContents* from, TabContents* to) {
    NOTIMPLEMENTED();
  }
 protected:
  typedef std::vector<ConstrainedWindow*> ConstrainedWindowList;
  ConstrainedWindowList child_windows_;
 private:
  GURL url_;
  std::wstring title_;
  NavigationController* controller_;
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

class BookmarkNode {
};

class BookmarkModelObserver {
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
  virtual bool IsLoaded() {
    NOTIMPLEMENTED();
    return false;
  }
  virtual void BlockTillLoaded() { NOTIMPLEMENTED(); }
  void AddObserver(BookmarkModelObserver* observer) { NOTIMPLEMENTED(); }
  void RemoveObserver(BookmarkModelObserver* observer) { NOTIMPLEMENTED(); }
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
  RenderWidgetHostView* CreateViewForWidget(RenderWidgetHost*);
  void RenderWidgetHostDestroyed(RenderWidgetHost*) { NOTIMPLEMENTED(); }
  void SetPageTitle(const std::wstring&) { NOTIMPLEMENTED(); }
  virtual void CreateNewWindow(int,
                               base::WaitableEvent*) { NOTIMPLEMENTED(); }
  virtual void CreateNewWidget(int, bool) { NOTIMPLEMENTED(); }
  virtual void ShowCreatedWindow(int, WindowOpenDisposition,
                                 const gfx::Rect&, bool) { NOTIMPLEMENTED(); }
  virtual void ShowCreatedWidget(int, const gfx::Rect&) { NOTIMPLEMENTED(); }
  virtual void ShowContextMenu(const ContextMenuParams&) { NOTIMPLEMENTED(); }
  virtual void StartDragging(const WebDropData&) { NOTIMPLEMENTED(); }
  virtual void UpdateDragCursor(bool) { NOTIMPLEMENTED(); }
  virtual void TakeFocus(bool) { NOTIMPLEMENTED(); }
  virtual void HandleKeyboardEvent(const WebKeyboardEvent&) 
      { NOTIMPLEMENTED(); }
  virtual void OnFindReply(int, int, const gfx::Rect&, int,
                           bool) { NOTIMPLEMENTED(); }
};

class WebContentsViewWin : public WebContentsView {
 public:
  WebContentsViewWin(WebContents*) { }
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

class RepostFormWarningDialog {
 public:
  static void RunRepostFormWarningDialog(NavigationController*) { }
  virtual ~RepostFormWarningDialog() { }
};

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
