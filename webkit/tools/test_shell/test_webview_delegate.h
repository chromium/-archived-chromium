// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// TestWebViewDelegate class: 
// This class implements the WebViewDelegate methods for the test shell.  One
// instance is owned by each TestShell.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_WEBVIEW_DELEGATE_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEST_WEBVIEW_DELEGATE_H__

#include <windows.h>
#include <map>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webwidget_delegate.h"
#include "webkit/tools/test_shell/drag_delegate.h"
#include "webkit/tools/test_shell/drop_delegate.h"

struct WebPreferences;
class GURL;
class TestShell;
class WebDataSource;
class WebWidgetHost;

class TestWebViewDelegate : public base::RefCounted<TestWebViewDelegate>, public WebViewDelegate {
 public:
  struct CapturedContextMenuEvent {
    CapturedContextMenuEvent(ContextNode::Type in_type,
                             int in_x,
                             int in_y)
      : type(in_type),
        x(in_x),
        y(in_y) {
    }

    ContextNode::Type type;
    int x;
    int y;
  };

  typedef std::vector<CapturedContextMenuEvent> CapturedContextMenuEvents;

  TestWebViewDelegate(TestShell* shell) 
    : shell_(shell),
      top_loading_frame_(NULL),
      page_id_(-1),
      last_page_id_updated_(-1),
      page_is_loading_(false),
      is_custom_policy_delegate_(false),
      custom_cursor_(NULL) { 
  }
  virtual ~TestWebViewDelegate();

  // WebViewDelegate
  virtual WebView* CreateWebView(WebView* webview, bool user_gesture);
  virtual WebWidget* CreatePopupWidget(WebView* webview);
  virtual WebPluginDelegate* CreatePluginDelegate(
    WebView* webview,
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    std::string* actual_mime_type);
  virtual void OpenURL(WebView* webview,
                       const GURL& url,
                       WindowOpenDisposition disposition);
  virtual void RunJavaScriptAlert(WebView* webview,
                                  const std::wstring& message);
  virtual bool RunJavaScriptConfirm(WebView* webview,
                                    const std::wstring& message);
  virtual bool RunJavaScriptPrompt(WebView* webview,
                                   const std::wstring& message,
                                   const std::wstring& default_value,
                                   std::wstring* result);
  virtual void AddMessageToConsole(WebView* webview,
                                   const std::wstring& message,
                                   unsigned int line_no,
                                   const std::wstring& source_id);
  virtual void StartDragging(WebView* webview,
                             const WebDropData& drop_data);
  virtual void ShowContextMenu(WebView* webview,
                               ContextNode::Type type,
                               int x,
                               int y,
                               const GURL& link_url,
                               const GURL& image_url,
                               const GURL& page_url,
                               const GURL& frame_url,
                               const std::wstring& selection_text,
                               const std::wstring& misspelled_word,
                               int edit_flags,
                               const std::string& frame_encoding);
  virtual void DidStartProvisionalLoadForFrame(
    WebView* webview,
    WebFrame* frame,
    NavigationGesture gesture);
  virtual void DidReceiveServerRedirectForProvisionalLoadForFrame(
    WebView* webview, WebFrame* frame);
  virtual void DidFailProvisionalLoadWithError(WebView* webview,
                                               const WebError& error,
                                               WebFrame* frame);
  virtual void DidCommitLoadForFrame(WebView* webview, WebFrame* frame,
                                     bool is_new_navigation);
  virtual void DidReceiveTitle(WebView* webview,
                               const std::wstring& title,
                               WebFrame* frame);
  virtual void DidFinishDocumentLoadForFrame(WebView* webview,
                                             WebFrame* frame);
  virtual void DidHandleOnloadEventsForFrame(WebView* webview,
                                             WebFrame* frame);
  virtual void DidChangeLocationWithinPageForFrame(WebView* webview,
                                                   WebFrame* frame,
                                                   bool is_new_navigation);
  virtual void DidReceiveIconForFrame(WebView* webview, WebFrame* frame);

  virtual void WillPerformClientRedirect(WebView* webview,
                                         WebFrame* frame,
                                         const std::wstring& dest_url,
                                         unsigned int delay_seconds,
                                         unsigned int fire_date);
  virtual void DidCancelClientRedirect(WebView* webview, 
                                       WebFrame* frame);

  virtual void DidFinishLoadForFrame(WebView* webview, WebFrame* frame);
  virtual void DidFailLoadWithError(WebView* webview,
                                    const WebError& error,
                                    WebFrame* forFrame);

  virtual void AssignIdentifierToRequest(WebView* webview,
                                         uint32 identifier,
                                         const WebRequest& request);
  virtual void WillSendRequest(WebView* webview,
                               uint32 identifier,
                               WebRequest* request);
  virtual void DidFinishLoading(WebView* webview, uint32 identifier);
  virtual void DidFailLoadingWithError(WebView* webview,
                                       uint32 identifier,
                                       const WebError& error);

  virtual bool ShouldBeginEditing(WebView* webview, std::wstring range);
  virtual bool ShouldEndEditing(WebView* webview, std::wstring range);
  virtual bool ShouldInsertNode(WebView* webview, 
                                std::wstring node, 
                                std::wstring range,
                                std::wstring action);
  virtual bool ShouldInsertText(WebView* webview, 
                                std::wstring text, 
                                std::wstring range,
                                std::wstring action);
  virtual bool ShouldChangeSelectedRange(WebView* webview, 
                                         std::wstring fromRange, 
                                         std::wstring toRange, 
                                         std::wstring affinity, 
                                         bool stillSelecting);
  virtual bool ShouldDeleteRange(WebView* webview, std::wstring range);
  virtual bool ShouldApplyStyle(WebView* webview, 
                                std::wstring style,
                                std::wstring range);
  virtual bool SmartInsertDeleteEnabled();
  virtual void DidBeginEditing();
  virtual void DidChangeSelection();
  virtual void DidChangeContents();
  virtual void DidEndEditing();

  virtual void DidStartLoading(WebView* webview);
  virtual void DidStopLoading(WebView* webview);

  virtual void WindowObjectCleared(WebFrame* webframe);
  virtual WindowOpenDisposition DispositionForNavigationAction(
    WebView* webview,
    WebFrame* frame,
    const WebRequest* request,
    WebNavigationType type,
    WindowOpenDisposition disposition,
    bool is_redirect);
  void TestWebViewDelegate::SetCustomPolicyDelegate(bool isCustom);
  virtual WebHistoryItem* GetHistoryEntryAtOffset(int offset);
  virtual void GoToEntryAtOffsetAsync(int offset);
  virtual int GetHistoryBackListCount();
  virtual int GetHistoryForwardListCount();

  // WebWidgetDelegate
  virtual HWND GetContainingWindow(WebWidget* webwidget);
  virtual void DidInvalidateRect(WebWidget* webwidget, const gfx::Rect& rect);
  virtual void DidScrollRect(WebWidget* webwidget, int dx, int dy,
                             const gfx::Rect& clip_rect);
  virtual void Show(WebWidget* webview, WindowOpenDisposition disposition);
  virtual void CloseWidgetSoon(WebWidget* webwidget);
  virtual void Focus(WebWidget* webwidget);
  virtual void Blur(WebWidget* webwidget);
  virtual void SetCursor(WebWidget* webwidget, 
                         const WebCursor& cursor);
  virtual void GetWindowRect(WebWidget* webwidget, gfx::Rect* rect);
  virtual void SetWindowRect(WebWidget* webwidget, const gfx::Rect& rect);
  virtual void DidMove(WebWidget* webwidget, const WebPluginGeometry& move);
  virtual void RunModal(WebWidget* webwidget);
  virtual void AddRef() {
    RefCounted<TestWebViewDelegate>::AddRef();
  }
  virtual void Release() {
    RefCounted<TestWebViewDelegate>::Release();
  }

  // Additional accessors
  WebFrame* top_loading_frame() { return top_loading_frame_; }
  IDropTarget* drop_delegate() { return drop_delegate_.get(); }
  IDropSource* drag_delegate() { return drag_delegate_.get(); }
  const CapturedContextMenuEvents& captured_context_menu_events() const { 
    return captured_context_menu_events_; 
  }
  void clear_captured_context_menu_events() { 
    captured_context_menu_events_.clear(); 
  }

  // Methods for modifying WebPreferences
  void SetUserStyleSheetEnabled(bool is_enabled);
  void SetUserStyleSheetLocation(const GURL& location);
  void SetDashboardCompatibilityMode(bool use_mode);

  // Sets the webview as a drop target.
  void RegisterDragDrop();

 protected:
  void UpdateAddressBar(WebView* webView);

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading.  From here, we can generate the dump for the
  // test.
  void LocationChangeDone(WebDataSource* data_source);

  WebWidgetHost* GetHostForWidget(WebWidget* webwidget);

  void UpdateForCommittedLoad(WebFrame* webframe, bool is_new_navigation);
  void UpdateURL(WebFrame* frame);
  void UpdateSessionHistory(WebFrame* frame);

  // Get a string suitable for dumping a frame to the console.
  std::wstring GetFrameDescription(WebFrame* webframe);

 private:
  // True while a page is in the process of being loaded.  This flag should
  // not be necessary, but it helps guard against mismatched messages for 
  // starting and ending loading frames.
  bool page_is_loading_;

  // Causes navigation actions just printout the intended navigation instead 
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  bool is_custom_policy_delegate_;

  // Non-owning pointer.  The delegate is owned by the host.
  TestShell* shell_;

  // This is non-NULL IFF a load is in progress.
  WebFrame* top_loading_frame_;

  // For tracking session history.  See RenderView.
  int page_id_;
  int last_page_id_updated_;

  // Maps resource identifiers to a descriptive string.
  typedef std::map<uint32, std::string> ResourceMap;
  ResourceMap resource_identifier_map_;
  std::string GetResourceDescription(uint32 identifier);

  HCURSOR custom_cursor_;

  // Classes needed by drag and drop.
  scoped_refptr<TestDragDelegate> drag_delegate_;
  scoped_refptr<TestDropDelegate> drop_delegate_;
  
  CapturedContextMenuEvents captured_context_menu_events_;

  DISALLOW_EVIL_CONSTRUCTORS(TestWebViewDelegate);
};

#endif // WEBKIT_TOOLS_TEST_SHELL_TEST_WEBVIEW_DELEGATE_H__
