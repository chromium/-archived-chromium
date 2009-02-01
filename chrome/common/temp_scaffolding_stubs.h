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
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class BookmarkService;
class CommandLine;
class HistoryService;
class MetricsService;
class NavigationController;
class NavigationEntry;
class NotificationService;
class ProfileManager;
class Profile;
class SessionID;
class SiteInstance;
class SpellChecker;
class TabContents;
class Task;
class TemplateURL;
class TemplateURLRef;
class URLRequestContext;
class UserScriptMaster;
class VisitedLinkMaster;
class WebContents;

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

class SavePackage {
 public:
  static bool IsSavableContents(const std::string& contents_mime_type) {
    return false;
  }
  static bool IsSavableURL(const GURL& url) { return false; }
};

class DebuggerWindow : public base::RefCountedThreadSafe<DebuggerWindow> {
 public:
};

class NavigationEntry {
 public:
  const GURL& url() const { return url_; }
 private:
  GURL url_;
};

class NavigationController {
 public:
  NavigationController() : entry_(new NavigationEntry) { }
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
  static void DisablePromptOnRepost() {}
  
 private:
  scoped_ptr<NavigationEntry> entry_;
};

class TabContentsDelegate {
 public:
   virtual void OpenURL(const GURL& url, const GURL& referrer,
                        WindowOpenDisposition disposition,
                        PageTransition::Type transition) { }
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

class RenderViewHost {
 public:
  bool HasUnloadListener() const { return false; }
  void FirePageBeforeUnload() { }
};

class TabContents {
 public:
  TabContents() : controller_(new NavigationController) { }
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
  void CloseContents() { };
  void SetupController(Profile* profile) { }
  bool WasHidden() { return false; }
  void RestoreFocus() { }
  static TabContentsType TypeForURL(GURL* url) { return TAB_CONTENTS_WEB; }
  static TabContents* CreateWithType(TabContentsType type,
                                     Profile* profile,
                                     SiteInstance* instance);
  void AddInfoBar(InfoBarDelegate* delegate) {}
  
  Profile* profile() const { return NULL; }
 private:
  GURL url_;
  std::wstring title_;
  scoped_ptr<NavigationController> controller_;
};

class WebContents : public TabContents {
 public:
  WebContents* AsWebContents() { return this; }
  bool showing_interstitial_page() const { return false; }
  InterstitialPage* interstitial_page() const { return NULL; }
  bool is_starred() const { return false; }
  const std::string& contents_mime_type() const { return mime_type_; }
  bool notify_disconnection() const { return false; }
  RenderViewHost* render_view_host() const {
    return const_cast<RenderViewHost*>(&render_view_host_);
  }

 private:
  RenderViewHost render_view_host_;

  std::string mime_type_;
};

class SelectFileDialog : public base::RefCountedThreadSafe<SelectFileDialog> {
 public:
  class Listener {
   public:
  };
  void ListenerDestroyed() { }
};

class SiteInstance {
 public:
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
};

class TemplateURLFetcher {
 public:
  explicit TemplateURLFetcher(Profile* profile) { }
  bool Init(Profile* profile) { return true; }
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
};

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
