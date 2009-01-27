// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of TestWebViewDelegate, which serves
// as the WebViewDelegate for the TestShellWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#include "base/file_util.h"
#include "base/gfx/point.h"
#include "base/gfx/native_widget_types.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "net/base/net_errors.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/weberror.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"

#if defined(OS_WIN)
// TODO(port): make these files work everywhere.
#include "base/gfx/gdi_util.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/tools/test_shell/drag_delegate.h"
#include "webkit/tools/test_shell/drop_delegate.h"
#endif

namespace {

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char* kLinkClickedString = "link clicked";
const char* kFormSubmittedString = "form submitted";
const char* kBackForwardString = "back/forward";
const char* kReloadString = "reload";
const char* kFormResubmittedString = "form resubmitted";
const char* kOtherString = "other";
const char* kIllegalString = "illegal value";

int next_page_id_ = 1;

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
std::wstring UrlSuitableForTestResult(const std::wstring& url) {
  if (url.empty() || std::wstring::npos == url.find(L"file://"))
    return url;

  std::wstring filename = file_util::GetFilenameFromPath(url);
  if (filename.empty())
    return L"file:";  // A WebKit test has this in its expected output.
  return filename;
}

// Adds a file called "DRTFakeFile" to |data_object| (CF_HDROP).  Use to fake
// dragging a file.
void AddDRTFakeFileToDataObject(WebDropData* drop_data) {
  drop_data->filenames.push_back(L"DRTFakeFile");
}

// Get a debugging string from a WebNavigationType.
const char* WebNavigationTypeToString(WebNavigationType type) {
  switch (type) {
    case WebNavigationTypeLinkClicked:
      return kLinkClickedString;
    case WebNavigationTypeFormSubmitted:
      return kFormSubmittedString;
    case WebNavigationTypeBackForward:
      return kBackForwardString;
    case WebNavigationTypeReload:
      return kReloadString;
    case WebNavigationTypeFormResubmitted:
      return kFormResubmittedString;
    case WebNavigationTypeOther:
      return kOtherString;
  }
  return kIllegalString;
}

}  // namespace

// WebViewDelegate -----------------------------------------------------------

WebView* TestWebViewDelegate::CreateWebView(WebView* webview,
                                            bool user_gesture) {
  return shell_->CreateWebView(webview);
}

WebWidget* TestWebViewDelegate::CreatePopupWidget(WebView* webview,
                                                  bool activatable) {
  return shell_->CreatePopupWidget(webview);
}

void TestWebViewDelegate::OpenURL(WebView* webview, const GURL& url,
                                  const GURL& referrer,
                                  WindowOpenDisposition disposition) {
  DCHECK_NE(disposition, CURRENT_TAB);  // No code for this
  if (disposition == SUPPRESS_OPEN)
    return;
  TestShell* shell = NULL;
  if (TestShell::CreateNewWindow(UTF8ToWide(url.spec()), &shell))
    shell->Show(shell->webView(), disposition);
}

void TestWebViewDelegate::DidStartLoading(WebView* webview) {
  // Ignored
}

void TestWebViewDelegate::DidStopLoading(WebView* webview) {
  // Ignored
}

void TestWebViewDelegate::WindowObjectCleared(WebFrame* webframe) {
  shell_->BindJSObjectsToWindow(webframe);
}

WindowOpenDisposition TestWebViewDelegate::DispositionForNavigationAction(
    WebView* webview,
    WebFrame* frame,
    const WebRequest* request,
    WebNavigationType type,
    WindowOpenDisposition disposition,
    bool is_redirect) {
  if (is_custom_policy_delegate_) {
    std::wstring frame_name = frame->GetName();
    printf("Policy delegate: attempt to load %s with navigation type '%s'\n",
           request->GetURL().spec().c_str(), WebNavigationTypeToString(type));
    return IGNORE_ACTION;
  } else {
    return WebViewDelegate::DispositionForNavigationAction(
      webview, frame, request, type, disposition, is_redirect);
  }
}


void TestWebViewDelegate::SetCustomPolicyDelegate(bool isCustom) {
  is_custom_policy_delegate_ = isCustom;
}

void TestWebViewDelegate::AssignIdentifierToRequest(WebView* webview,
                                                    uint32 identifier,
                                                    const WebRequest& request) {
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    resource_identifier_map_[identifier] = request.GetURL().possibly_invalid_spec();
  }
}

std::string TestWebViewDelegate::GetResourceDescription(uint32 identifier) {
  ResourceMap::iterator it = resource_identifier_map_.find(identifier);
  return it != resource_identifier_map_.end() ? it->second : "<unknown>";
}

void TestWebViewDelegate::WillSendRequest(WebView* webview,
                                          uint32 identifier,
                                          WebRequest* request) {
  std::string request_url = request->GetURL().possibly_invalid_spec();

  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - willSendRequest <WebRequest URL \"%s\">\n",
           GetResourceDescription(identifier).c_str(),
           request_url.c_str());
  }

  TRACE_EVENT_BEGIN("url.load", identifier, request_url);
  // Set the new substituted URL.
  request->SetURL(GURL(TestShell::RewriteLocalUrl(request_url)));
}

void TestWebViewDelegate::DidFinishLoading(WebView* webview,
                                           uint32 identifier) {
  TRACE_EVENT_END("url.load", identifier, "");
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - didFinishLoading\n",
           GetResourceDescription(identifier).c_str());
  }

  resource_identifier_map_.erase(identifier);
}

void TestWebViewDelegate::DidFailLoadingWithError(WebView* webview,
                                                  uint32 identifier,
                                                  const WebError& error) {
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - didFailLoadingWithError <WebError code %d,"
           " failing URL \"%s\">\n",
           GetResourceDescription(identifier).c_str(),
           error.GetErrorCode(),
           error.GetFailedURL().spec().c_str());
  }

  resource_identifier_map_.erase(identifier);
}

void TestWebViewDelegate::DidStartProvisionalLoadForFrame(
    WebView* webview,
    WebFrame* frame,
    NavigationGesture gesture) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didStartProvisionalLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }

  if (!top_loading_frame_) {
    top_loading_frame_ = frame;
  }

  if (shell_->layout_test_controller()->StopProvisionalFrameLoads()) {
    printf("%S - stopping load in didStartProvisionalLoadForFrame callback\n",
           GetFrameDescription(frame).c_str());
    frame->StopLoading();
  }
  UpdateAddressBar(webview);
}

void TestWebViewDelegate::DidReceiveServerRedirectForProvisionalLoadForFrame(
    WebView* webview,
    WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didReceiveServerRedirectForProvisionalLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }

  UpdateAddressBar(webview);
}

void TestWebViewDelegate::DidFailProvisionalLoadWithError(
    WebView* webview,
    const WebError& error,
    WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFailProvisionalLoadWithError\n",
           GetFrameDescription(frame).c_str());
  }

  LocationChangeDone(frame->GetProvisionalDataSource());

  // Don't display an error page if we're running layout tests, because
  // DumpRenderTree doesn't.
  if (shell_->layout_test_mode())
    return;

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.GetErrorCode() == net::ERR_ABORTED)
    return;

  const WebRequest& failed_request =
      frame->GetProvisionalDataSource()->GetRequest();
  TestShellExtraRequestData* extra_data =
      static_cast<TestShellExtraRequestData*>(failed_request.GetExtraData());
  bool replace = extra_data && extra_data->pending_page_id != -1;

  scoped_ptr<WebRequest> request(failed_request.Clone());

  std::string error_text =
      StringPrintf("Error %d when loading url %s", error.GetErrorCode(),
      request->GetURL().spec().c_str());
  request->SetURL(GURL("testshell-error:"));

  frame->LoadAlternateHTMLString(request.get(), error_text,
                                 error.GetFailedURL(), replace);
}

void TestWebViewDelegate::DidCommitLoadForFrame(WebView* webview,
                                                WebFrame* frame,
                                                bool is_new_navigation) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didCommitLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }

  UpdateForCommittedLoad(frame, is_new_navigation);
}

void TestWebViewDelegate::DidReceiveTitle(WebView* webview,
                                          const std::wstring& title,
                                          WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didReceiveTitle\n",
           GetFrameDescription(frame).c_str());
  }

  if (shell_->ShouldDumpTitleChanges()) {
    printf("TITLE CHANGED: %S\n", title.c_str());
  }

  SetPageTitle(title);
}

void TestWebViewDelegate::DidFinishLoadForFrame(WebView* webview,
                                                WebFrame* frame) {
  TRACE_EVENT_END("frame.load", this, frame->GetURL().spec());
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFinishLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }

  UpdateAddressBar(webview);
  LocationChangeDone(frame->GetDataSource());
}

void TestWebViewDelegate::DidFailLoadWithError(WebView* webview,
                                               const WebError& error,
                                               WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFailLoadWithError\n",
           GetFrameDescription(frame).c_str());
  }

  LocationChangeDone(frame->GetDataSource());
}

void TestWebViewDelegate::DidFinishDocumentLoadForFrame(WebView* webview,
                                                        WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFinishDocumentLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  } else {
    unsigned pending_unload_events = frame->PendingFrameUnloadEventCount();
    if (pending_unload_events) {
      printf("%S - has %u onunload handler(s)\n",
          GetFrameDescription(frame).c_str(), pending_unload_events);
    }
  }
}

void TestWebViewDelegate::DidHandleOnloadEventsForFrame(WebView* webview,
                                                        WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didHandleOnloadEventsForFrame\n",
           GetFrameDescription(frame).c_str());
  }
}

void TestWebViewDelegate::DidChangeLocationWithinPageForFrame(
    WebView* webview, WebFrame* frame, bool is_new_navigation) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didChangeLocationWithinPageForFrame\n",
           GetFrameDescription(frame).c_str());
  }

  UpdateForCommittedLoad(frame, is_new_navigation);
}

void TestWebViewDelegate::DidReceiveIconForFrame(WebView* webview,
                                                 WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didReceiveIconForFrame\n",
           GetFrameDescription(frame).c_str());
  }
}

void TestWebViewDelegate::WillPerformClientRedirect(WebView* webview,
                                                    WebFrame* frame,
                                                    const std::wstring& dest_url,
                                                    unsigned int delay_seconds,
                                                    unsigned int fire_date) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    // FIXME: prettyprint the url?
    printf("%S - willPerformClientRedirectToURL: %S\n",
           GetFrameDescription(frame).c_str(), dest_url.c_str());
  }
}

void TestWebViewDelegate::DidCancelClientRedirect(WebView* webview,
                                                  WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didCancelClientRedirectForFrame\n",
           GetFrameDescription(frame).c_str());
  }
}

void TestWebViewDelegate::AddMessageToConsole(WebView* webview,
                                              const std::wstring& message,
                                              unsigned int line_no,
                                              const std::wstring& source_id) {
  if (!shell_->layout_test_mode()) {
    logging::LogMessage("CONSOLE", 0).stream() << "\"" 
                                               << message.c_str() 
                                               << ",\" source: " 
                                               << source_id.c_str() 
                                               << "(" 
                                               << line_no 
                                               << ")";
  } else {
    // This matches win DumpRenderTree's UIDelegate.cpp.
    std::wstring new_message = message;
    if (!message.empty()) {
      new_message = message;
      size_t file_protocol = new_message.find(L"file://");
      if (file_protocol != std::wstring::npos) {
        new_message = new_message.substr(0, file_protocol) +
            UrlSuitableForTestResult(new_message.substr(file_protocol));
      }
    }

    std::string utf8 = WideToUTF8(new_message);
    printf("CONSOLE MESSAGE: line %d: %s\n", line_no, utf8.c_str());
  }
}

void TestWebViewDelegate::RunJavaScriptAlert(WebView* webview,
                                             const std::wstring& message) {
  if (!shell_->layout_test_mode()) {
    ShowJavaScriptAlert(message);
  } else {
    std::string utf8 = WideToUTF8(message);
    printf("ALERT: %s\n", utf8.c_str());
  }
}

bool TestWebViewDelegate::RunJavaScriptConfirm(WebView* webview,
                                               const std::wstring& message) {
  if (shell_->layout_test_mode()) {
    // When running tests, write to stdout.
    std::string utf8 = WideToUTF8(message);
    printf("CONFIRM: %s\n", utf8.c_str());
    return true;
  }
  return false;
}

bool TestWebViewDelegate::RunJavaScriptPrompt(WebView* webview,
    const std::wstring& message, const std::wstring& default_value,
    std::wstring* result) {
  if (shell_->layout_test_mode()) {
    // When running tests, write to stdout.
    std::string utf8_message = WideToUTF8(message);
    std::string utf8_default_value = WideToUTF8(default_value);
    printf("PROMPT: %s, default text: %s\n", utf8_message.c_str(),
           utf8_default_value.c_str());
    return true;
  }
  return false;
}

void TestWebViewDelegate::StartDragging(WebView* webview,
                                        const WebDropData& drop_data) {
  if (webkit_glue::IsLayoutTestMode()) {
    WebDropData mutable_drop_data = drop_data;
    if (shell_->layout_test_controller()->ShouldAddFileToPasteboard()) {
      // Add a file called DRTFakeFile to the drag&drop clipboard.
      AddDRTFakeFileToDataObject(&mutable_drop_data);
    }

    // When running a test, we need to fake a drag drop operation otherwise
    // Windows waits for real mouse events to know when the drag is over.
    EventSendingController::DoDragDrop(mutable_drop_data);
  } else {
    // TODO(tc): Drag and drop is disabled in the test shell because we need
    // to be able to convert from WebDragData to an IDataObject.
    //if (!drag_delegate_)
    //  drag_delegate_ = new TestDragDelegate(shell_->webViewWnd(),
    //                                        shell_->webView());
    //const DWORD ok_effect = DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE;
    //DWORD effect;
    //HRESULT res = DoDragDrop(drop_data.data_object, drag_delegate_.get(),
    //                         ok_effect, &effect);
    //DCHECK(DRAGDROP_S_DROP == res || DRAGDROP_S_CANCEL == res);
  }
  webview->DragSourceSystemDragEnded();
}

void TestWebViewDelegate::ShowContextMenu(WebView* webview,
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
                                          const std::string& security_info) {
  CapturedContextMenuEvent context(type, x, y);
  captured_context_menu_events_.push_back(context);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests.  See EditingDelegate.m in DumpRenderTree.
bool TestWebViewDelegate::ShouldBeginEditing(WebView* webview, 
                                             std::wstring range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    std::string utf8 = WideToUTF8(range);
    printf("EDITING DELEGATE: shouldBeginEditingInDOMRange:%s\n", 
           utf8.c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::ShouldEndEditing(WebView* webview, 
                                           std::wstring range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    std::string utf8 = WideToUTF8(range);
    printf("EDITING DELEGATE: shouldEndEditingInDOMRange:%s\n", 
           utf8.c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::ShouldInsertNode(WebView* webview, 
                                           std::wstring node, 
                                           std::wstring range,
                                           std::wstring action) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    std::string utf8_node = WideToUTF8(node);
    std::string utf8_range = WideToUTF8(range);
    std::string utf8_action = WideToUTF8(action);
    printf("EDITING DELEGATE: shouldInsertNode:%s "
               "replacingDOMRange:%s givenAction:%s\n", 
           utf8_node.c_str(), utf8_range.c_str(), utf8_action.c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::ShouldInsertText(WebView* webview, 
                                           std::wstring text, 
                                           std::wstring range,
                                           std::wstring action) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    std::string utf8_text = WideToUTF8(text);
    std::string utf8_range = WideToUTF8(range);
    std::string utf8_action = WideToUTF8(action);
    printf("EDITING DELEGATE: shouldInsertText:%s "
               "replacingDOMRange:%s givenAction:%s\n", 
           utf8_text.c_str(), utf8_range.c_str(), utf8_action.c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::ShouldChangeSelectedRange(WebView* webview, 
                                                    std::wstring fromRange, 
                                                    std::wstring toRange, 
                                                    std::wstring affinity, 
                                                    bool stillSelecting) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    std::string utf8_from = WideToUTF8(fromRange);
    std::string utf8_to = WideToUTF8(toRange);
    std::string utf8_affinity = WideToUTF8(affinity);
    printf("EDITING DELEGATE: shouldChangeSelectedDOMRange:%s "
               "toDOMRange:%s affinity:%s stillSelecting:%s\n", 
           utf8_from.c_str(), 
           utf8_to.c_str(), 
           utf8_affinity.c_str(),
           (stillSelecting ? "TRUE" : "FALSE"));
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::ShouldDeleteRange(WebView* webview, 
                                            std::wstring range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    std::string utf8 = WideToUTF8(range);
    printf("EDITING DELEGATE: shouldDeleteDOMRange:%s\n", utf8.c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::ShouldApplyStyle(WebView* webview, 
                                           std::wstring style,
                                           std::wstring range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    std::string utf8_style = WideToUTF8(style);
    std::string utf8_range = WideToUTF8(range);
    printf("EDITING DELEGATE: shouldApplyStyle:%s toElementsInDOMRange:%s\n", 
           utf8_style.c_str(), utf8_range.c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::SmartInsertDeleteEnabled() {
  return smart_insert_delete_enabled_;
}

void TestWebViewDelegate::SetSmartInsertDeleteEnabled(bool enabled) {
  smart_insert_delete_enabled_ = enabled;
  // In upstream WebKit, smart insert/delete is mutually exclusive with select
  // trailing whitespace, however, we allow both because Chromium on Windows
  // allows both.
}

bool TestWebViewDelegate::IsSelectTrailingWhitespaceEnabled() {
  return select_trailing_whitespace_enabled_;
}

void TestWebViewDelegate::SetSelectTrailingWhitespaceEnabled(bool enabled) {
  select_trailing_whitespace_enabled_ = enabled;
  // In upstream WebKit, smart insert/delete is mutually exclusive with select
  // trailing whitespace, however, we allow both because Chromium on Windows
  // allows both.
}

void TestWebViewDelegate::DidBeginEditing() { 
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidBeginEditing:WebViewDidBeginEditingNotification\n");
  }
}

void TestWebViewDelegate::DidChangeSelection(bool is_empty_selection) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
    "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
  }
  UpdateSelectionClipboard(is_empty_selection);
}

void TestWebViewDelegate::DidChangeContents() {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidChange:WebViewDidChangeNotification\n");
  }
}

void TestWebViewDelegate::DidEndEditing() {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidEndEditing:WebViewDidEndEditingNotification\n");
  }
}

WebHistoryItem* TestWebViewDelegate::GetHistoryEntryAtOffset(int offset) {
  TestNavigationEntry* entry = static_cast<TestNavigationEntry*>(
      shell_->navigation_controller()->GetEntryAtOffset(offset));
  if (!entry)
    return NULL;

  return entry->GetHistoryItem();
}

int TestWebViewDelegate::GetHistoryBackListCount() {
  int current_index =
      shell_->navigation_controller()->GetLastCommittedEntryIndex();
  return current_index;
}

int TestWebViewDelegate::GetHistoryForwardListCount() {
  int current_index =
      shell_->navigation_controller()->GetLastCommittedEntryIndex();
  return shell_->navigation_controller()->GetEntryCount() - current_index - 1;
}

void TestWebViewDelegate::SetUserStyleSheetEnabled(bool is_enabled) {
  WebPreferences* prefs = shell_->GetWebPreferences();
  prefs->user_style_sheet_enabled = is_enabled;
  shell_->webView()->SetPreferences(*prefs);
}

void TestWebViewDelegate::SetUserStyleSheetLocation(const GURL& location) {
  WebPreferences* prefs = shell_->GetWebPreferences();
  prefs->user_style_sheet_enabled = true;
  prefs->user_style_sheet_location = location;
  shell_->webView()->SetPreferences(*prefs);
}

// WebWidgetDelegate ---------------------------------------------------------

gfx::NativeViewId TestWebViewDelegate::GetContainingView(WebWidget* webwidget) {
  // For test shell, we pack a NativeView pointer into the NativeViewId since
  // everything is single process.
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    return gfx::IdFromNativeView(host->view_handle());

  return NULL;
}

void TestWebViewDelegate::DidInvalidateRect(WebWidget* webwidget,
                                            const gfx::Rect& rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    host->DidInvalidateRect(rect);
}

void TestWebViewDelegate::DidScrollRect(WebWidget* webwidget, int dx, int dy,
                                        const gfx::Rect& clip_rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    host->DidScrollRect(dx, dy, clip_rect);
}

void TestWebViewDelegate::Focus(WebWidget* webwidget) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    shell_->SetFocus(host, true);
}

void TestWebViewDelegate::Blur(WebWidget* webwidget) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget))
    shell_->SetFocus(host, false);
}


void TestWebViewDelegate::DidMove(WebWidget* webwidget,
                                  const WebPluginGeometry& move) {
#if defined(OS_WIN)
  HRGN hrgn = ::CreateRectRgn(move.clip_rect.x(),
                              move.clip_rect.y(),
                              move.clip_rect.right(),
                              move.clip_rect.bottom());
  gfx::SubtractRectanglesFromRegion(hrgn, move.cutout_rects);

  // Note: System will own the hrgn after we call SetWindowRgn,
  // so we don't need to call DeleteObject(hrgn)
  ::SetWindowRgn(move.window, hrgn, FALSE);

  unsigned long flags = 0;
  if (move.visible)
    flags |= SWP_SHOWWINDOW;
  else
    flags |= SWP_HIDEWINDOW;

  ::SetWindowPos(move.window,
                 NULL,
                 move.window_rect.x(),
                 move.window_rect.y(),
                 move.window_rect.width(),
                 move.window_rect.height(),
                 flags);

#endif
  // TODO(port): add me once plugins work.
}

bool TestWebViewDelegate::IsHidden() {
  return false;
}

void TestWebViewDelegate::RegisterDragDrop() {
#if defined(OS_WIN)
  // TODO(port): add me once drag and drop works.
  DCHECK(!drop_delegate_);
  drop_delegate_ = new TestDropDelegate(shell_->webViewWnd(),
                                        shell_->webView());
#endif
}

// Private methods -----------------------------------------------------------

void TestWebViewDelegate::UpdateAddressBar(WebView* webView) {
  WebFrame* mainFrame = webView->GetMainFrame();

  WebDataSource* dataSource = mainFrame->GetDataSource();
  if (!dataSource)
    dataSource = mainFrame->GetProvisionalDataSource();
  if (!dataSource)
    return;

  SetAddressBarURL(dataSource->GetRequest().GetMainDocumentURL());
}

void TestWebViewDelegate::LocationChangeDone(WebDataSource* data_source) {
  if (data_source->GetWebFrame() == top_loading_frame_) {
    top_loading_frame_ = NULL;

    if (shell_->layout_test_mode())
      shell_->layout_test_controller()->LocationChangeDone();
  }
}

WebWidgetHost* TestWebViewDelegate::GetHostForWidget(WebWidget* webwidget) {
  if (webwidget == shell_->webView())
    return shell_->webViewHost();
  if (webwidget == shell_->popup())
    return shell_->popupHost();
  return NULL;
}

void TestWebViewDelegate::UpdateForCommittedLoad(WebFrame* frame,
                                                 bool is_new_navigation) {
  WebView* webview = shell_->webView();

  // Code duplicated from RenderView::DidCommitLoadForFrame.
  const WebRequest& request =
      webview->GetMainFrame()->GetDataSource()->GetRequest();
  TestShellExtraRequestData* extra_data =
      static_cast<TestShellExtraRequestData*>(request.GetExtraData());

  if (is_new_navigation) {
    // New navigation.
    UpdateSessionHistory(frame);
    page_id_ = next_page_id_++;
  } else if (extra_data && extra_data->pending_page_id != -1 &&
             !extra_data->request_committed) {
    // This is a successful session history navigation!
    UpdateSessionHistory(frame);
    page_id_ = extra_data->pending_page_id;
  }

  // Don't update session history multiple times.
  if (extra_data)
    extra_data->request_committed = true;

  UpdateURL(frame);
}

void TestWebViewDelegate::UpdateURL(WebFrame* frame) {
  WebDataSource* ds = frame->GetDataSource();
  DCHECK(ds);

  const WebRequest& request = ds->GetRequest();

  // Type is unused.
  scoped_ptr<TestNavigationEntry> entry(new TestNavigationEntry);

  // Bug 654101: the referrer will be empty on https->http transitions. It
  // would be nice if we could get the real referrer from somewhere.
  entry->SetPageID(page_id_);
  if (ds->HasUnreachableURL()) {
    entry->SetURL(GURL(ds->GetUnreachableURL()));
  } else {
    entry->SetURL(GURL(request.GetURL()));
  }

  shell_->navigation_controller()->DidNavigateToEntry(entry.release());

  last_page_id_updated_ = std::max(last_page_id_updated_, page_id_);
}

void TestWebViewDelegate::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  TestNavigationEntry* entry = static_cast<TestNavigationEntry*>(
      shell_->navigation_controller()->GetEntryWithPageID(page_id_));
  if (!entry)
    return;

  std::string state;
  if (!shell_->webView()->GetMainFrame()->GetPreviousHistoryState(&state))
    return;
  entry->SetContentState(state);
}

std::wstring TestWebViewDelegate::GetFrameDescription(WebFrame* webframe) {
  std::wstring name = webframe->GetName();

  if (webframe == shell_->webView()->GetMainFrame()) {
    if (name.length())
      return L"main frame \"" + name + L"\"";
    else
      return L"main frame";
  } else {
    if (name.length())
      return L"frame \"" + name + L"\"";
    else
      return L"frame (anonymous)";
  }
}

