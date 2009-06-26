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
#include "base/gfx/native_widget_types.h"
#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"


class BookmarkContextMenu;
class BookmarkNode;
class Browser;
class CommandLine;
class DownloadItem;
class MessageLoop;
class NavigationController;
class ProcessSingleton;
class Profile;
class RenderViewHostDelegate;
class SiteInstance;
class URLRequest;
class TabContents;
struct ViewHostMsg_DidPrintPage_Params;

namespace gfx {
class Rect;
class Widget;
}

namespace IPC {
class Message;
}

//---------------------------------------------------------------------------
// These stubs are for Browser_main()

void OpenFirstRunDialog(Profile* profile, ProcessSingleton* process_singleton);

void InstallJankometer(const CommandLine&);

//---------------------------------------------------------------------------
// These stubs are for BrowserProcessImpl

class CancelableTask;
class ViewMsg_Print_Params;

// Printing is all (obviously) not implemented.
// http://code.google.com/p/chromium/issues/detail?id=9847
namespace printing {

class PrintViewManager {
 public:
  PrintViewManager(TabContents&) { }
  void Stop() { NOTIMPLEMENTED(); }
  void Destroy() { }
  bool OnRenderViewGone(RenderViewHost*) {
    NOTIMPLEMENTED();
    return true;  // Assume for now that all renderer crashes are important.
  }
  void DidGetPrintedPagesCount(int, int) { NOTIMPLEMENTED(); }
  void DidPrintPage(const ViewHostMsg_DidPrintPage_Params&) {
    NOTIMPLEMENTED();
  }
};

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
                   bool has_selection,
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
  void OnQuit() { }
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

#if !defined(TOOLKIT_VIEWS)
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
#endif

#if !defined(OS_LINUX)
class Window {
 public:
  void Show() { NOTIMPLEMENTED(); }
  virtual void Close() { NOTIMPLEMENTED(); }
};
#endif

}  // namespace views

class InputWindowDelegate {
};

#if !defined(TOOLKIT_VIEWS)
namespace views {

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

}  // namespace view
#endif

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

//---------------------------------------------------------------------------
// These stubs are for Browser

#if !defined(TOOLKIT_VIEWS)
namespace download_util {
void DragDownload(const DownloadItem* download, SkBitmap* icon);
}  // namespace download_util
#endif

class DebuggerWindow : public base::RefCountedThreadSafe<DebuggerWindow> {
 public:
};

class FaviconStatus {
 public:
  const GURL& url() const { return url_; }
 private:
  GURL url_;
};

#if defined(OS_MACOSX)
class DockInfo {
 public:
  bool GetNewWindowBounds(gfx::Rect*, bool*) const;
  void AdjustOtherWindowBounds() const;
};
#else

#endif

//---------------------------------------------------------------------------
// These stubs are for Profile

class WebAppLauncher {
 public:
  static void Launch(Profile* profile, const GURL& url) {
    NOTIMPLEMENTED();
  }
};

//---------------------------------------------------------------------------
// These stubs are for TabContents

class WebApp : public base::RefCountedThreadSafe<WebApp> {
 public:
  class Observer {
   public:
  };
  void AddObserver(Observer* obs) { NOTIMPLEMENTED(); }
  void RemoveObserver(Observer* obs) { NOTIMPLEMENTED(); }
  void SetTabContents(TabContents*) { NOTIMPLEMENTED(); }
  SkBitmap GetFavIcon() {
    NOTIMPLEMENTED();
    return SkBitmap();
  }
};

class ModalHtmlDialogDelegate : public HtmlDialogUIDelegate {
 public:
  ModalHtmlDialogDelegate(const GURL&, int, int, const std::string&,
                          IPC::Message*, TabContents*) { }

   virtual bool IsDialogModal() const { return true; }
   virtual std::wstring GetDialogTitle() const { return std::wstring(); }
   virtual GURL GetDialogContentURL() const { return GURL(); }
   virtual void GetDialogSize(gfx::Size* size) const {}
   virtual std::string GetDialogArgs() const { return std::string(); }
   virtual void OnDialogClosed(const std::string& json_retval) {}
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
class LoginHandler {
 public:
  void SetAuth(const std::wstring& username,
               const std::wstring& password) {
    NOTIMPLEMENTED();
  }
  void CancelAuth() { NOTIMPLEMENTED(); }
  void OnRequestCancelled() { NOTIMPLEMENTED(); }
};
#endif

namespace net {
class AuthChallengeInfo;
}

#if defined(OS_MACOSX)
LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request,
                                MessageLoop* ui_loop);
#endif

class RepostFormWarningDialog {
 public:
  static void RunRepostFormWarningDialog(NavigationController*) { }
  virtual ~RepostFormWarningDialog() { }
};

class FontsLanguagesWindowView {
 public:
  explicit FontsLanguagesWindowView(Profile* profile) { NOTIMPLEMENTED(); }
  void SelectLanguagesTab() { NOTIMPLEMENTED(); }
};

#if !defined(TOOLKIT_VIEWS)
class OSExchangeData {
 public:
  void SetString(const std::wstring& data) { NOTIMPLEMENTED(); }
  void SetURL(const GURL& url, const std::wstring& title) { NOTIMPLEMENTED(); }
};
#endif

class BaseDragSource {
};

//---------------------------------------------------------------------------
// These stubs are for extensions

namespace views {
class HWNDView {
 public:
  int width() { NOTIMPLEMENTED(); return 0; }
  int height() { NOTIMPLEMENTED(); return 0; }
  void InitHidden() { NOTIMPLEMENTED(); }
  void SetPreferredSize(const gfx::Size& size) { NOTIMPLEMENTED(); }
  virtual void SetBackground(const SkBitmap&) { NOTIMPLEMENTED(); }
  virtual void SetVisible(bool flag) { NOTIMPLEMENTED(); }
  void SizeToPreferredSize() { NOTIMPLEMENTED(); }
  bool IsVisible() const { NOTIMPLEMENTED(); return false; }
  void Layout() { NOTIMPLEMENTED(); }
  void SchedulePaint() { NOTIMPLEMENTED(); }
  HWNDView* GetParent() const { NOTIMPLEMENTED(); return NULL; }
  virtual gfx::Size GetPreferredSize() { NOTIMPLEMENTED(); return gfx::Size(); }
  gfx::NativeWindow GetHWND() { NOTIMPLEMENTED(); return 0; }
  void Detach() { NOTIMPLEMENTED(); }
  gfx::Widget* GetWidget() { NOTIMPLEMENTED(); return NULL; }
};
}  // namespace views

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
