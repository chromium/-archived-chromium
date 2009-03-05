// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Cursor.h"
#include "FloatRect.h"
#include "FileChooser.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "Page.h"
#include "ScriptController.h"
#include "WindowFeatures.h"
#if USE(V8)
#include "v8_proxy.h"
#endif
MSVC_POP_WARNING();

#include "WebKit.h"

#undef LOG

#include "webkit/glue/chrome_client_impl.h"

#include "base/gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/weburlrequest_impl.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webwidget_impl.h"

struct IWebURLResponse;

// Callback class that's given to the WebViewDelegate during a file choose
// operation.
class WebFileChooserCallbackImpl : public WebFileChooserCallback {
 public:
  WebFileChooserCallbackImpl(PassRefPtr<WebCore::FileChooser> file_chooser)
      : file_chooser_(file_chooser) {
  }

  void OnFileChoose(const std::vector<std::wstring>& file_names) {
    if (file_names.empty()) {
      file_chooser_->chooseFile(WebCore::String(""));
    } else if (file_names.size() == 1) {
      file_chooser_->chooseFile(
        webkit_glue::StdWStringToString(file_names.front()));
    } else {
      Vector<WebCore::String> paths;
      for (std::vector<std::wstring>::const_iterator filename =
             file_names.begin(); filename != file_names.end(); ++filename) {
        paths.append(webkit_glue::StdWStringToString(*filename));
      }
      file_chooser_->chooseFiles(paths);
    }
  }

 private:
  RefPtr<WebCore::FileChooser> file_chooser_;
  DISALLOW_COPY_AND_ASSIGN(WebFileChooserCallbackImpl);
};

ChromeClientImpl::ChromeClientImpl(WebViewImpl* webview)
    : webview_(webview),
      toolbars_visible_(true),
      statusbar_visible_(true),
      scrollbars_visible_(true),
      menubar_visible_(true),
      resizable_(true),
      ignore_next_set_cursor_(false) {
}

ChromeClientImpl::~ChromeClientImpl() {
}

void ChromeClientImpl::chromeDestroyed() {
  delete this;
}

void ChromeClientImpl::setWindowRect(const WebCore::FloatRect& r) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    WebCore::IntRect ir(r);
    d->SetWindowRect(webview_,
                     gfx::Rect(ir.x(), ir.y(), ir.width(), ir.height()));
  }
}

WebCore::FloatRect ChromeClientImpl::windowRect() {
  if (webview_->delegate()) {
    gfx::Rect rect;
    webview_->delegate()->GetRootWindowRect(webview_, &rect);
    return WebCore::FloatRect(
        static_cast<float>(rect.x()),
        static_cast<float>(rect.y()),
        static_cast<float>(rect.width()),
        static_cast<float>(rect.height()));
  } else {
    // These numbers will be fairly wrong. The window's x/y coordinates will
    // be the top left corner of the screen and the size will be the content
    // size instead of the window size.
    gfx::Point origin;
    const gfx::Size size = webview_->size();
    return WebCore::FloatRect(
        static_cast<float>(origin.x()),
        static_cast<float>(origin.y()),
        static_cast<float>(size.width()),
        static_cast<float>(size.height()));
  }
}

WebCore::FloatRect ChromeClientImpl::pageRect() {
  // We hide the details of the window's border thickness from the web page by
  // simple re-using the window position here.  So, from the point-of-view of
  // the web page, the window has no border.
  return windowRect();
}

float ChromeClientImpl::scaleFactor() {
  // This is supposed to return the scale factor of the web page. It looks like
  // the implementor of the graphics layer is responsible for doing most of the
  // operations associated with scaling. However, this value is used ins some
  // cases by WebCore. For example, this is used as a scaling factor in canvas
  // so that things drawn in it are scaled just like the web page is.
  //
  // We don't currently implement scaling, so just return 1.0 (no scaling).
  return 1.0;
}

void ChromeClientImpl::focus() {
  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->Focus(webview_);
}

void ChromeClientImpl::unfocus() {
  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->Blur(webview_);
}

bool ChromeClientImpl::canTakeFocus(WebCore::FocusDirection) {
  // For now the browser can always take focus if we're not running layout
  // tests.
  return !WebKit::layoutTestMode();
}

void ChromeClientImpl::takeFocus(WebCore::FocusDirection direction) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    d->TakeFocus(webview_,
                 direction == WebCore::FocusDirectionBackward);
  }
}

WebCore::Page* ChromeClientImpl::createWindow(
    WebCore::Frame* frame, const WebCore::FrameLoadRequest& r,
    const WebCore::WindowFeatures& features) {
  WebViewDelegate* d = webview_->delegate();
  if (!d)
    return NULL;

  bool userGesture = frame->script()->processingUserGesture();
  WebViewImpl* new_view = static_cast<WebViewImpl*>(
      d->CreateWebView(webview_, userGesture));
  if (!new_view)
    return NULL;

  // The request is empty when we are just being asked to open a blank window.
  // This corresponds to window.open(""), for example.
  if (!r.resourceRequest().isEmpty()) {
    WebRequestImpl request(r);
    new_view->main_frame()->LoadRequest(&request);
  }

  WebViewImpl* new_view_impl = static_cast<WebViewImpl*>(new_view);
  return new_view_impl->page();
}

static inline bool CurrentEventShouldCauseBackgroundTab(
    const WebInputEvent* input_event) {
  if (!input_event)
    return false;

  if (input_event->type != WebInputEvent::MOUSE_UP)
    return false;

  const WebMouseEvent* mouse_event = static_cast<const WebMouseEvent*>(input_event);
  return (mouse_event->button == WebMouseEvent::BUTTON_MIDDLE);
}

void ChromeClientImpl::show() {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    // If our default configuration was modified by a script or wasn't
    // created by a user gesture, then show as a popup. Else, let this
    // new window be opened as a toplevel window.
    //
    bool as_popup =
        !toolbars_visible_ ||
        !statusbar_visible_ ||
        !scrollbars_visible_ ||
        !menubar_visible_ ||
        !resizable_ ||
        !d->WasOpenedByUserGesture(webview_);

    WindowOpenDisposition disposition = NEW_FOREGROUND_TAB;
    if (as_popup)
      disposition = NEW_POPUP;
    if (CurrentEventShouldCauseBackgroundTab(WebViewImpl::current_input_event()))
      disposition = NEW_BACKGROUND_TAB;

    d->Show(webview_, disposition);
  }
}

bool ChromeClientImpl::canRunModal() {
  return webview_->delegate() != NULL;
}

void ChromeClientImpl::runModal() {
  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->RunModal(webview_);
}

void ChromeClientImpl::setToolbarsVisible(bool value) {
  toolbars_visible_ = value;
}

bool ChromeClientImpl::toolbarsVisible() {
  return toolbars_visible_;
}

void ChromeClientImpl::setStatusbarVisible(bool value) {
  statusbar_visible_ = value;
}

bool ChromeClientImpl::statusbarVisible() {
  return statusbar_visible_;
}

void ChromeClientImpl::setScrollbarsVisible(bool value) {
  scrollbars_visible_ = value;
  WebFrameImpl* web_frame =
      static_cast<WebFrameImpl*>(webview_->GetMainFrame());
  if (web_frame)
    web_frame->SetAllowsScrolling(value);
}

bool ChromeClientImpl::scrollbarsVisible() {
  return scrollbars_visible_;
}

void ChromeClientImpl::setMenubarVisible(bool value) {
  menubar_visible_ = value;
}

bool ChromeClientImpl::menubarVisible() {
  return menubar_visible_;
}

void ChromeClientImpl::setResizable(bool value) {
  resizable_ = value;
}

void ChromeClientImpl::addMessageToConsole(const WebCore::String& message,
                                           unsigned int line_no,
                                           const WebCore::String& source_id) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    std::wstring wstr_message = webkit_glue::StringToStdWString(message);
    std::wstring wstr_source_id = webkit_glue::StringToStdWString(source_id);
    d->AddMessageToConsole(webview_, wstr_message, line_no, wstr_source_id);
  }
}

bool ChromeClientImpl::canRunBeforeUnloadConfirmPanel() {
  return webview_->delegate() != NULL;
}

bool ChromeClientImpl::runBeforeUnloadConfirmPanel(
    const WebCore::String& message,
    WebCore::Frame* frame) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    std::wstring wstr = webkit_glue::StringToStdWString(message);
    return d->RunBeforeUnloadConfirm(WebFrameImpl::FromFrame(frame), wstr);
  }
  return false;
}

void ChromeClientImpl::closeWindowSoon() {
  // Make sure this Page can no longer be found by JS.
  webview_->page()->setGroupName(WebCore::String());

  // Make sure that all loading is stopped.  Ensures that JS stops executing!
  webview_->StopLoading();

  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->CloseWidgetSoon(webview_);
}

// Although a WebCore::Frame is passed in, we don't actually use it, since we
// already know our own webview_.
void ChromeClientImpl::runJavaScriptAlert(WebCore::Frame* frame,
                                          const WebCore::String& message) {
  // Pass the request on to the WebView delegate, for more control.
  WebViewDelegate* d = webview_->delegate();
  if (d) {
#if USE(V8)
    // Before showing the JavaScript dialog, we give the proxy implementation
    // a chance to process any pending console messages.
    WebCore::V8Proxy::ProcessConsoleMessages();
#endif

    std::wstring wstr = webkit_glue::StringToStdWString(message);
    d->RunJavaScriptAlert(WebFrameImpl::FromFrame(frame), wstr);
  }
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptConfirm(WebCore::Frame* frame,
                                            const WebCore::String& message) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    std::wstring wstr = webkit_glue::StringToStdWString(message);
    return d->RunJavaScriptConfirm(WebFrameImpl::FromFrame(frame), wstr);
  }
  return false;
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptPrompt(WebCore::Frame* frame,
                                           const WebCore::String& message,
                                           const WebCore::String& defaultValue,
                                           WebCore::String& result) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    std::wstring wstr_message = webkit_glue::StringToStdWString(message);
    std::wstring wstr_default = webkit_glue::StringToStdWString(defaultValue);
    std::wstring wstr_result;
    bool ok = d->RunJavaScriptPrompt(WebFrameImpl::FromFrame(frame),
                                     wstr_message,
                                     wstr_default,
                                     &wstr_result);
    if (ok)
      result = webkit_glue::StdWStringToString(wstr_result);
    return ok;
  }
  return false;
}

void ChromeClientImpl::setStatusbarText(const WebCore::String& message) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    std::wstring wstr = webkit_glue::StringToStdWString(message);
    d->SetStatusbarText(webview_, wstr);
  }
}

bool ChromeClientImpl::shouldInterruptJavaScript() {
  // TODO(mbelshe): implement me
  return false;
}

bool ChromeClientImpl::tabsToLinks() const {
  // TODO(pamg): Consider controlling this with a user preference, when we have
  // a preference system in place.
  // For now Chrome will allow link to take focus if we're not running layout
  // tests.
  return !WebKit::layoutTestMode();
}

WebCore::IntRect ChromeClientImpl::windowResizerRect() const {
  WebCore::IntRect rv;
  if (webview_->delegate()) {
    gfx::Rect resizer_rect;
    webview_->delegate()->GetRootWindowResizerRect(webview_, &resizer_rect);
    rv = WebCore::IntRect(resizer_rect.x(),
                          resizer_rect.y(),
                          resizer_rect.width(),
                          resizer_rect.height());
  }
  return rv;
}

void ChromeClientImpl::repaint(
    const WebCore::IntRect& paint_rect, bool content_changed, bool immediate,
    bool repaint_content_only) {
  // Ignore spurious calls.
  if (!content_changed || paint_rect.isEmpty())
    return;
  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->DidInvalidateRect(webview_, webkit_glue::FromIntRect(paint_rect));
}

void ChromeClientImpl::scroll(
    const WebCore::IntSize& scroll_delta, const WebCore::IntRect& scroll_rect,
    const WebCore::IntRect& clip_rect) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    int dx = scroll_delta.width();
    int dy = scroll_delta.height();
    d->DidScrollRect(webview_, dx, dy, webkit_glue::FromIntRect(clip_rect));
  }
}

WebCore::IntPoint ChromeClientImpl::screenToWindow(
    const WebCore::IntPoint&) const {
  NOTIMPLEMENTED();
  return WebCore::IntPoint();
}

WebCore::IntRect ChromeClientImpl::windowToScreen(
    const WebCore::IntRect& rect) const {
  WebCore::IntRect screen_rect(rect);

  WebViewDelegate* d = webview_->delegate();
  if (d) {
    gfx::Rect window_rect;
    d->GetWindowRect(webview_, &window_rect);
    screen_rect.move(window_rect.x(), window_rect.y());
  }

  return screen_rect;
}

PlatformWidget ChromeClientImpl::platformWindow() const {
  WebViewDelegate* d = webview_->delegate();
  return d ? d->GetContainingView(webview_) : NULL;
}

void ChromeClientImpl::mouseDidMoveOverElement(
    const WebCore::HitTestResult& result, unsigned modifierFlags) {
  // Find out if the mouse is over a link, and if so, let our UI know... somehow
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    if (result.isLiveLink() && !result.absoluteLinkURL().string().isEmpty()) {
      d->UpdateTargetURL(webview_, webkit_glue::KURLToGURL(result.absoluteLinkURL()));
    } else {
      d->UpdateTargetURL(webview_, GURL());
    }
  }
}

void ChromeClientImpl::setToolTip(const WebCore::String& tooltip_text) {
  if (webview_->delegate()) {
    std::wstring tooltip_text_as_wstring =
      webkit_glue::StringToStdWString(tooltip_text);
    webview_->delegate()->SetTooltipText(webview_, tooltip_text_as_wstring);
  }
}

void ChromeClientImpl::print(WebCore::Frame* frame) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    d->ScriptedPrint(WebFrameImpl::FromFrame(frame));
  }
}

void ChromeClientImpl::exceededDatabaseQuota(WebCore::Frame* frame,
    const WebCore::String& databaseName) {
  // TODO(tc): If we enable the storage API, we need to implement this function.
}

void ChromeClientImpl::runOpenPanel(WebCore::Frame* frame,
  PassRefPtr<WebCore::FileChooser> fileChooser) {
  WebViewDelegate* delegate = webview_->delegate();
  if (!delegate)
    return;

  bool multiple_files = fileChooser->allowsMultipleFiles();

  std::wstring suggestion;
  if (fileChooser->filenames().size() > 0)
    suggestion = webkit_glue::StringToStdWString(fileChooser->filenames()[0]);

  WebFileChooserCallbackImpl* chooser = new WebFileChooserCallbackImpl(fileChooser);
  delegate->RunFileChooser(multiple_files, std::wstring(), suggestion,
                           std::wstring(), chooser);
}

void ChromeClientImpl::popupOpened(WebCore::FramelessScrollView* popup_view,
                                   const WebCore::IntRect& bounds,
                                   bool activatable) {
  WebViewDelegate* d = webview_->delegate();
  if (d) {
    WebWidgetImpl* webwidget =
        static_cast<WebWidgetImpl*>(d->CreatePopupWidget(webview_,
                                                         activatable));
    webwidget->Init(popup_view, webkit_glue::FromIntRect(bounds));
  }
}

void ChromeClientImpl::SetCursor(const WebCursor& cursor) {
  if (ignore_next_set_cursor_) {
    ignore_next_set_cursor_ = false;
    return;
  }

  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->SetCursor(webview_, cursor);
}

void ChromeClientImpl::SetCursorForPlugin(const WebCursor& cursor) {
  SetCursor(cursor);
  // Currently, Widget::setCursor is always called after this function in
  // EventHandler.cpp and since we don't want that we set a flag indicating
  // that the next SetCursor call is to be ignored.
  ignore_next_set_cursor_ = true;
}

void ChromeClientImpl::enableSuddenTermination() {
  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->EnableSuddenTermination();
}

void ChromeClientImpl::disableSuddenTermination() {
  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->DisableSuddenTermination();
}

void ChromeClientImpl::formStateDidChange(const WebCore::Node*) {
  WebViewDelegate* d = webview_->delegate();
  if (d)
    d->OnNavStateChanged(webview_);
}
