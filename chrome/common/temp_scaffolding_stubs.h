// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
#define CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_

// This file provides declarations and stub definitions for classes we encouter
// during the porting effort. It is not meant to be permanent, and classes will
// be removed from here as they are fleshed out more completely.

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/clipboard.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/save_types.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_states.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/window_open_disposition.h"

class BookmarkContextMenu;
class Browser;
class CommandLine;
class ConstrainedWindow;
class CPCommandInterface;
class DOMUIHost;
class DownloadItem;
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
class RenderViewHostDelegate;
class ResourceMessageFilter;
class SessionBackend;
class SessionCommand;
class SessionID;
class SiteInstance;
class SpellChecker;
class TabContentsDelegate;
class TabContentsFactory;
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

#if defined(OS_MACOSX)
// TODO(port): needs an implementation of ProcessSingleton.
class ProcessSingleton {
 public:
  explicit ProcessSingleton(const FilePath& user_data_dir) { }
  ~ProcessSingleton() { }
  bool NotifyOtherProcess() {
    NOTIMPLEMENTED();
    return false;
  }
  void HuntForZombieChromeProcesses() { NOTIMPLEMENTED(); }
  void Create() { NOTIMPLEMENTED(); }
  void Lock() { NOTIMPLEMENTED(); }
  void Unlock() { NOTIMPLEMENTED(); }
};
#endif  // defined(OS_MACOSX)

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

void OpenFirstRunDialog(Profile* profile);

void InstallJankometer(const CommandLine&);

//---------------------------------------------------------------------------
// These stubs are for BrowserProcessImpl

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

namespace sandbox {

enum ResultCode {
  SBOX_ALL_OK = 0,
  SBOX_ERROR_GENERIC = 1,
  SBOX_ERROR_BAD_PARAMS = 2,
  SBOX_ERROR_UNSUPPORTED = 3,
  SBOX_ERROR_NO_SPACE = 4,
  SBOX_ERROR_INVALID_IPC = 5,
  SBOX_ERROR_FAILED_IPC = 6,
  SBOX_ERROR_NO_HANDLE = 7,
  SBOX_ERROR_UNEXPECTED_CALL = 8,
  SBOX_ERROR_WAIT_ALREADY_CALLED = 9,
  SBOX_ERROR_CHANNEL_ERROR = 10,
  SBOX_ERROR_LAST
};

class BrokerServices {
 public:
  void Init() { NOTIMPLEMENTED(); }
};

}  // namespace sandbox

struct ViewHostMsg_DidPrintPage_Params;

namespace views {

class AcceleratorHandler {
};

class TableModelObserver {
 public:
  virtual void OnModelChanged() = 0;
  virtual void OnItemsChanged(int, int) = 0;
  virtual void OnItemsAdded(int, int) = 0;
  virtual void OnItemsRemoved(int, int) = 0;
};

class TableModel {
 public:
  int CompareValues(int row1, int row2, int column_id) {
    NOTIMPLEMENTED();
    return 0;
  }
  virtual int RowCount() = 0;
};

class MenuItemView {
 public:
  enum Type {
    NORMAL,
    SUBMENU,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };
  enum AnchorPosition {
    TOPLEFT,
    TOPRIGHT
  };
  MenuItemView(BookmarkContextMenu*) { NOTIMPLEMENTED(); }
  void RunMenuAt(gfx::NativeWindow parent, const gfx::Rect& bounds,
                 AnchorPosition anchor, bool has_mnemonics) {
    NOTIMPLEMENTED();
  }
  void Cancel() { NOTIMPLEMENTED(); }
  void AppendMenuItem(int item_id, const std::wstring& label, Type type) {
    NOTIMPLEMENTED();
  }
  void AppendMenuItemWithLabel(int item_id, const std::wstring& label) {
    NOTIMPLEMENTED();
  }
  void AppendSeparator() { NOTIMPLEMENTED(); }
};

class MenuDelegate {
};

class Window {
 public:
  void Show() { NOTIMPLEMENTED(); }
  virtual void Close() { NOTIMPLEMENTED(); }
};

}  // namespace views

class InputWindowDelegate {
};

class Menu {
 public:
  enum AnchorPoint {
    TOPLEFT,
    TOPRIGHT
  };
  enum MenuItemType {
    NORMAL,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };
  class Delegate {
  };
  Menu(Delegate* delegate, AnchorPoint anchor, gfx::NativeWindow owner) {
    NOTIMPLEMENTED();
  }
  void AppendMenuItem(int item_id, const std::wstring& label,
                      MenuItemType type) {
    NOTIMPLEMENTED();
  }
  void AppendMenuItemWithLabel(int item_id, const std::wstring& label) {
    NOTIMPLEMENTED();
  }
  Menu* AppendSubMenu(int item_id, const std::wstring& label) {
    NOTIMPLEMENTED();
    return NULL;
  }
  void AppendSeparator() { NOTIMPLEMENTED(); }
  void AppendDelegateMenuItem(int item_id) { NOTIMPLEMENTED(); }
};

views::Window* CreateInputWindow(gfx::NativeWindow parent_hwnd,
                                 InputWindowDelegate* delegate);

class BookmarkManagerView {
 public:
   static BookmarkManagerView* current() {
     NOTIMPLEMENTED();
     return NULL;
   }
   static void Show(Profile* profile) { NOTIMPLEMENTED(); }
   void SelectInTree(BookmarkNode* node) { NOTIMPLEMENTED(); }
   Profile* profile() const {
    NOTIMPLEMENTED();
    return NULL;
  }
};

class BookmarkEditorView {
 public:
  class Handler {
  };
  enum Configuration {
    SHOW_TREE,
    NO_TREE
  };
  static void Show(gfx::NativeWindow parent_window, Profile* profile,
                   BookmarkNode* parent, BookmarkNode* node,
                   Configuration configuration, Handler* handler) {
    NOTIMPLEMENTED();
  }
};

class BookmarkBarView {
 public:
  static void ToggleWhenVisible(Profile* profile) { NOTIMPLEMENTED(); }
};

//---------------------------------------------------------------------------
// These stubs are for Browser

namespace download_util {
void DragDownload(const DownloadItem* download, SkBitmap* icon);
}  // namespace download_util

class IconLoader {
 public:
  enum IconSize {
    SMALL = 0,  // 16x16
    NORMAL,     // 32x32
    LARGE
  };
};

class IconManager : public CancelableRequestProvider {
 public:
  typedef CancelableRequestProvider::Handle Handle;
  typedef Callback2<Handle, SkBitmap*>::Type IconRequestCallback;
  SkBitmap* LookupIcon(const std::wstring&, IconLoader::IconSize)
      { NOTIMPLEMENTED(); return NULL; }
  Handle LoadIcon(const std::wstring&, IconLoader::IconSize,
                  CancelableRequestConsumerBase*, IconRequestCallback*)
      { NOTIMPLEMENTED(); return NULL; }
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
      : type_(type), is_crashed_(false), is_active_(true), is_loading_(false),
        is_being_destroyed_(false), waiting_for_response_(false),
        shelf_visible_(false), controller_(), delegate_(), max_page_id_(-1) { }
  virtual ~TabContents() { }
  NavigationController* controller() const { return controller_; }
  void set_controller(NavigationController* c) { controller_ = c; }
  virtual WebContents* AsWebContents() { return NULL; }
  WebContents* AsWebContents() const {
    return const_cast<TabContents*>(this)->AsWebContents();
  }
  virtual SkBitmap GetFavIcon() const;
  const GURL& GetURL() const;
  virtual const string16& GetTitle() const;
  TabContentsType type() const { return type_; }
  void set_type(TabContentsType type) { type_ = type; }
  virtual void Focus() { NOTIMPLEMENTED(); }
  virtual void Stop() { NOTIMPLEMENTED(); }
  Profile* profile() const;
  virtual void CloseContents();
  virtual void SetupController(Profile* profile);
  virtual void WasHidden() {
    NOTIMPLEMENTED();
  }
  virtual void SetInitialFocus() { NOTIMPLEMENTED(); }
  virtual void SetInitialFocus(bool reverse) { NOTIMPLEMENTED(); }
  virtual void RestoreFocus() { NOTIMPLEMENTED(); }
  static TabContentsType TypeForURL(GURL* url);
  static TabContents* CreateWithType(TabContentsType type,
                                     Profile* profile,
                                     SiteInstance* instance);
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) { NOTIMPLEMENTED(); }
  virtual void DidBecomeSelected() { NOTIMPLEMENTED(); }
  virtual void SetDownloadShelfVisible(bool visible);
  bool IsDownloadShelfVisible() { return shelf_visible_; }
  virtual void Destroy();
  virtual void SetIsLoading(bool, LoadNotificationDetails*);
  bool is_crashed() const { return is_crashed_; }
  virtual void SetIsCrashed(bool);
  bool capturing_contents() const {
    NOTIMPLEMENTED();
    return false;
  }
  void set_capturing_contents(bool) { NOTIMPLEMENTED(); }
  bool is_active() const { return is_active_; }
  void set_is_active(bool active) { is_active_ = active; }
  bool is_loading() const { return is_loading_; }
  bool is_being_destroyed() const { return is_being_destroyed_; }
  bool waiting_for_response() const { return waiting_for_response_; }
  void SetNotWaitingForResponse() { waiting_for_response_ = false; }
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
  virtual DOMUIHost* AsDOMUIHost() { return NULL; }
  virtual std::wstring GetStatusText() const { return std::wstring(); }
  static void RegisterUserPrefs(PrefService* prefs) {
    prefs->RegisterBooleanPref(prefs::kBlockPopups, false);
  }
  virtual void CreateView() {}
  virtual gfx::NativeView GetNativeView() const { return NULL; }
  static TabContentsFactory* RegisterFactory(TabContentsType type,
                                             TabContentsFactory* factory);
  void RemoveInfoBar(InfoBarDelegate* delegate) { NOTIMPLEMENTED(); }
  virtual bool ShouldDisplayURL() { return true; }
  void ToolbarSizeChanged(bool is_animating);
  void OnStartDownload(DownloadItem* download);
  DownloadShelf* GetDownloadShelf();
  static void MigrateShelf(TabContents* from, TabContents* to);
  void MigrateShelfFrom(TabContents* tab_contents);
 protected:
  typedef std::vector<ConstrainedWindow*> ConstrainedWindowList;
  ConstrainedWindowList child_windows_;
 private:
  virtual void ReleaseDownloadShelf();
  friend class AutomationProvider;

  scoped_ptr<DownloadShelf> download_shelf_;
  TabContentsType type_;
  bool is_crashed_;
  bool is_active_;
  bool is_loading_;
  bool is_being_destroyed_;
  bool waiting_for_response_;
  bool shelf_visible_;
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

class WindowSizer {
 public:
  static void GetBrowserWindowBounds(const std::wstring& app_name,
                                     const gfx::Rect& specified_bounds,
                                     gfx::Rect* window_bounds,
                                     bool* maximized) { NOTIMPLEMENTED(); }
};

//---------------------------------------------------------------------------
// These stubs are for Profile

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
  static bool EncryptString16(const string16& plaintext,
                              std::string* ciphertext) {
    NOTIMPLEMENTED();
    return false;
  }

  static bool DecryptString16(const std::string& ciphertext,
                              string16* plaintext) {
    NOTIMPLEMENTED();
    return false;
  }
};

class WebAppLauncher {
 public:
  static void Launch(Profile* profile, const GURL& url) {
    NOTIMPLEMENTED();
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

namespace printing {
class PrintViewManager {
 public:
  PrintViewManager(WebContents&) { }
  void Stop() { NOTIMPLEMENTED(); }
  void Destroy() { NOTIMPLEMENTED(); }
  bool OnRenderViewGone(RenderViewHost*) {
    NOTIMPLEMENTED();
    return true;  // Assume for now that all renderer crashes are important.
  }
  void DidGetPrintedPagesCount(int, int) { NOTIMPLEMENTED(); }
  void DidPrintPage(const ViewHostMsg_DidPrintPage_Params&) {
    NOTIMPLEMENTED();
  }
};
}

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

class HtmlDialogContentsDelegate {
 public:
};

class ModalHtmlDialogDelegate : public HtmlDialogContentsDelegate {
 public:
  ModalHtmlDialogDelegate(const GURL&, int, int, const std::string&,
                          IPC::Message*, WebContents*) { }
};

class HtmlDialogContents {
 public:
  struct HtmlDialogParams {
    GURL url;
    int width;
    int height;
    std::string json_input;
  };
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
  enum BlockState {
    DONT_BLOCK,
    BLOCK,
    UNKNOWN,
  };
  static BlockState GetBlockState(const std::wstring& scheme) {
    NOTIMPLEMENTED();
    return UNKNOWN;
  }
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

class PageInfoWindow {
 public:
  enum TabID {
    GENERAL = 0,
    SECURITY,
  };
  static void CreatePageInfo(Profile* profile, NavigationEntry* nav_entry,
                             gfx::NativeView parent_hwnd, TabID tab) {
    NOTIMPLEMENTED();
  }
  static void CreateFrameInfo(Profile* profile, const GURL& url,
                              const NavigationEntry::SSLStatus& ssl,
                              gfx::NativeView parent_hwnd, TabID tab) {
    NOTIMPLEMENTED();
  }
};

class FontsLanguagesWindowView {
 public:
  explicit FontsLanguagesWindowView(Profile* profile) { NOTIMPLEMENTED(); }
  void SelectLanguagesTab() { NOTIMPLEMENTED(); }
};

class OSExchangeData {
 public:
  void SetString(const std::wstring& data) { NOTIMPLEMENTED(); }
  void SetURL(const GURL& url, const std::wstring& title) { NOTIMPLEMENTED(); }
};

class BaseDragSource {
};

//---------------------------------------------------------------------------
// These stubs are for extensions

class HWNDHtmlView {
 public:
  HWNDHtmlView(const GURL& content_url, RenderViewHostDelegate* delegate,
               bool allow_dom_ui_bindings) {
    NOTIMPLEMENTED();
  }
  virtual ~HWNDHtmlView() {}

  RenderViewHost* render_view_host() { NOTIMPLEMENTED(); return NULL; }
  void InitHidden() { NOTIMPLEMENTED(); }
};

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
