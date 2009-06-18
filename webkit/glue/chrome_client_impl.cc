// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "AccessibilityObject.h"
#include "AXObjectCache.h"
#include "Console.h"
#include "Cursor.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FloatRect.h"
#include "FileChooser.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "Node.h"
#include "Page.h"
#include "PopupMenuChromium.h"
#include "ScriptController.h"
#include "WindowFeatures.h"
#if USE(V8)
#include "v8_proxy.h"
#endif
MSVC_POP_WARNING();

#undef LOG

#include "webkit/glue/chrome_client_impl.h"

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/api/src/WrappedResourceRequest.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webwidget_impl.h"

using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebRect;
using WebKit::WebURLRequest;
using WebKit::WrappedResourceRequest;

// Callback class that's given to the WebViewDelegate during a file choose
// operation.
class WebFileChooserCallbackImpl : public WebFileChooserCallback {
 public:
  WebFileChooserCallbackImpl(PassRefPtr<WebCore::FileChooser> file_chooser)
      : file_chooser_(file_chooser) {
  }

  virtual void OnFileChoose(const std::vector<FilePath>& file_names) {
    if (file_names.size() == 1) {
      file_chooser_->chooseFile(
          webkit_glue::FilePathStringToString(file_names.front().value()));
    } else {
      // This clause handles a case of file_names.size()==0 too.
      Vector<WebCore::String> paths;
      for (std::vector<FilePath>::const_iterator filename =
             file_names.begin(); filename != file_names.end(); ++filename) {
        paths.append(webkit_glue::FilePathStringToString((*filename).value()));
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
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    WebCore::IntRect ir(r);
    delegate->SetWindowRect(webview_,
                            gfx::Rect(ir.x(), ir.y(), ir.width(), ir.height()));
  }
}

WebCore::FloatRect ChromeClientImpl::windowRect() {
  WebRect rect;
  if (webview_->delegate()) {
    webview_->delegate()->GetRootWindowRect(webview_, &rect);
  } else {
    // These numbers will be fairly wrong. The window's x/y coordinates will
    // be the top left corner of the screen and the size will be the content
    // size instead of the window size.
    rect.width = webview_->size().width;
    rect.height = webview_->size().height;
  }
  return WebCore::FloatRect(static_cast<float>(rect.x),
                            static_cast<float>(rect.y),
                            static_cast<float>(rect.width),
                            static_cast<float>(rect.height));
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
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate)
    delegate->Focus(webview_);

  // If accessibility is enabled, we should notify assistive technology that the
  // active AccessibilityObject changed.
  WebCore::Document* doc = webview_->GetFocusedWebCoreFrame()->document();

  if (doc && doc->axObjectCache()->accessibilityEnabled()) {
    WebCore::Node* focused_node = webview_->GetFocusedNode();

    if (!focused_node) {
      // Could not retrieve focused Node.
      return;
    }

    // Retrieve the focused AccessibilityObject.
    WebCore::AccessibilityObject* focused_acc_obj =
        doc->axObjectCache()->getOrCreate(focused_node->renderer());

    // Alert assistive technology that focus changed.
    if (focused_acc_obj) {
      delegate->FocusAccessibilityObject(focused_acc_obj);
    }
  }
}

void ChromeClientImpl::unfocus() {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate)
    delegate->Blur(webview_);
}

bool ChromeClientImpl::canTakeFocus(WebCore::FocusDirection) {
  // For now the browser can always take focus if we're not running layout
  // tests.
  return !WebKit::layoutTestMode();
}

void ChromeClientImpl::takeFocus(WebCore::FocusDirection direction) {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    delegate->TakeFocus(webview_,
                        direction == WebCore::FocusDirectionBackward);
  }
}

WebCore::Page* ChromeClientImpl::createWindow(
    WebCore::Frame* frame, const WebCore::FrameLoadRequest& r,
    const WebCore::WindowFeatures& features) {
  WebViewDelegate* delegate = webview_->delegate();
  if (!delegate)
    return NULL;

  bool user_gesture = frame->script()->processingUserGesture();

  const std::string security_origin(webkit_glue::StringToStdString(
      frame->document()->securityOrigin()->toString()));
  GURL creator_url(security_origin);
  WebViewImpl* new_view = static_cast<WebViewImpl*>(
      delegate->CreateWebView(webview_, user_gesture,
      (creator_url.is_valid() && creator_url.IsStandard()) ?
      creator_url : GURL()));
  if (!new_view)
    return NULL;

  // The request is empty when we are just being asked to open a blank window.
  // This corresponds to window.open(""), for example.
  if (!r.resourceRequest().isEmpty()) {
    WrappedResourceRequest request(r.resourceRequest());
    new_view->main_frame()->LoadRequest(request);
  }

  WebViewImpl* new_view_impl = static_cast<WebViewImpl*>(new_view);
  return new_view_impl->page();
}

static inline bool CurrentEventShouldCauseBackgroundTab(
    const WebInputEvent* input_event) {
  if (!input_event)
    return false;

  if (input_event->type != WebInputEvent::MouseUp)
    return false;

  const WebMouseEvent* mouse_event =
      static_cast<const WebMouseEvent*>(input_event);
  return (mouse_event->button == WebMouseEvent::ButtonMiddle);
}

void ChromeClientImpl::show() {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
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
        !delegate->WasOpenedByUserGesture();

    WindowOpenDisposition disposition = NEW_FOREGROUND_TAB;
    if (as_popup)
      disposition = NEW_POPUP;
    if (CurrentEventShouldCauseBackgroundTab(WebViewImpl::current_input_event()))
      disposition = NEW_BACKGROUND_TAB;

    delegate->Show(webview_, disposition);
  }
}

bool ChromeClientImpl::canRunModal() {
  return webview_->delegate() != NULL;
}

void ChromeClientImpl::runModal() {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate)
    delegate->RunModal(webview_);
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

void ChromeClientImpl::addMessageToConsole(WebCore::MessageSource source,
                                           WebCore::MessageLevel level,
                                           const WebCore::String& message,
                                           unsigned int line_no,
                                           const WebCore::String& source_id) {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    std::wstring wstr_message = webkit_glue::StringToStdWString(message);
    std::wstring wstr_source_id = webkit_glue::StringToStdWString(source_id);
    delegate->AddMessageToConsole(webview_, wstr_message,
                                  line_no, wstr_source_id);
  }
}

bool ChromeClientImpl::canRunBeforeUnloadConfirmPanel() {
  return webview_->delegate() != NULL;
}

bool ChromeClientImpl::runBeforeUnloadConfirmPanel(
    const WebCore::String& message,
    WebCore::Frame* frame) {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    std::wstring wstr = webkit_glue::StringToStdWString(message);
    return delegate->RunBeforeUnloadConfirm(WebFrameImpl::FromFrame(frame),
                                            wstr);
  }
  return false;
}

void ChromeClientImpl::closeWindowSoon() {
  // Make sure this Page can no longer be found by JS.
  webview_->page()->setGroupName(WebCore::String());

  // Make sure that all loading is stopped.  Ensures that JS stops executing!
  webview_->StopLoading();

  WebViewDelegate* delegate = webview_->delegate();
  if (delegate)
    delegate->CloseWidgetSoon(webview_);
}

// Although a WebCore::Frame is passed in, we don't actually use it, since we
// already know our own webview_.
void ChromeClientImpl::runJavaScriptAlert(WebCore::Frame* frame,
                                          const WebCore::String& message) {
  // Pass the request on to the WebView delegate, for more control.
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
#if USE(V8)
    // Before showing the JavaScript dialog, we give the proxy implementation
    // a chance to process any pending console messages.
    WebCore::V8Proxy::ProcessConsoleMessages();
#endif

    std::wstring wstr = webkit_glue::StringToStdWString(message);
    delegate->RunJavaScriptAlert(WebFrameImpl::FromFrame(frame), wstr);
  }
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptConfirm(WebCore::Frame* frame,
                                            const WebCore::String& message) {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    std::wstring wstr = webkit_glue::StringToStdWString(message);
    return delegate->RunJavaScriptConfirm(WebFrameImpl::FromFrame(frame), wstr);
  }
  return false;
}

// See comments for runJavaScriptAlert().
bool ChromeClientImpl::runJavaScriptPrompt(WebCore::Frame* frame,
                                           const WebCore::String& message,
                                           const WebCore::String& defaultValue,
                                           WebCore::String& result) {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    std::wstring wstr_message = webkit_glue::StringToStdWString(message);
    std::wstring wstr_default = webkit_glue::StringToStdWString(defaultValue);
    std::wstring wstr_result;
    bool ok = delegate->RunJavaScriptPrompt(WebFrameImpl::FromFrame(frame),
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
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    std::wstring wstr = webkit_glue::StringToStdWString(message);
    delegate->SetStatusbarText(webview_, wstr);
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
  WebCore::IntRect result;
  if (webview_->delegate()) {
    WebRect resizer_rect;
    webview_->delegate()->GetRootWindowResizerRect(webview_, &resizer_rect);
    result = webkit_glue::WebRectToIntRect(resizer_rect);
  }
  return result;
}

void ChromeClientImpl::repaint(
    const WebCore::IntRect& paint_rect, bool content_changed, bool immediate,
    bool repaint_content_only) {
  // Ignore spurious calls.
  if (!content_changed || paint_rect.isEmpty())
    return;
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate)
    delegate->DidInvalidateRect(webview_,
                                webkit_glue::IntRectToWebRect(paint_rect));
}

void ChromeClientImpl::scroll(
    const WebCore::IntSize& scroll_delta, const WebCore::IntRect& scroll_rect,
    const WebCore::IntRect& clip_rect) {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    int dx = scroll_delta.width();
    int dy = scroll_delta.height();
    delegate->DidScrollRect(webview_, dx, dy,
                            webkit_glue::IntRectToWebRect(clip_rect));
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

  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    WebRect window_rect;
    delegate->GetWindowRect(webview_, &window_rect);
    screen_rect.move(window_rect.x, window_rect.y);
  }

  return screen_rect;
}

PlatformWidget ChromeClientImpl::platformWindow() const {
  WebViewDelegate* delegate = webview_->delegate();
  return delegate ? delegate->GetContainingView(webview_) : NULL;
}

void ChromeClientImpl::contentsSizeChanged(WebCore::Frame* frame, const
    WebCore::IntSize& size) const {
  WebViewDelegate* delegate = webview_->delegate();

  if (delegate) {
    delegate->DidContentsSizeChange(webview_, size.width(), size.height());
  }
}

void ChromeClientImpl::mouseDidMoveOverElement(
    const WebCore::HitTestResult& result, unsigned modifierFlags) {
  // Find out if the mouse is over a link, and if so, let our UI know... somehow
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    if (result.isLiveLink() && !result.absoluteLinkURL().string().isEmpty()) {
      delegate->UpdateTargetURL(
          webview_, webkit_glue::KURLToGURL(result.absoluteLinkURL()));
    } else {
      delegate->UpdateTargetURL(webview_, GURL());
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
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    delegate->ScriptedPrint(WebFrameImpl::FromFrame(frame));
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

  FilePath suggestion;
  if (fileChooser->filenames().size() > 0)
    suggestion = FilePath(
      webkit_glue::StringToFilePathString(fileChooser->filenames()[0]));

  WebFileChooserCallbackImpl* chooser =
      new WebFileChooserCallbackImpl(fileChooser);
  delegate->RunFileChooser(multiple_files, string16(), suggestion, chooser);
}

void ChromeClientImpl::popupOpened(WebCore::PopupContainer* popup_container,
                                   const WebCore::IntRect& bounds,
                                   bool activatable,
                                   bool handle_external) {
  if (handle_external) {
    // We're going to handle the popup with native controls by the external
    // embedder.
    popupOpenedInternal(popup_container, bounds, activatable);
    return;
  }

  WebViewDelegate* delegate = webview_->delegate();
  if (delegate) {
    WebWidgetImpl* webwidget =
        static_cast<WebWidgetImpl*>(delegate->CreatePopupWidget(webview_,
                                                                activatable));
    webwidget->Init(popup_container, webkit_glue::IntRectToWebRect(bounds));
  }
}

void ChromeClientImpl::popupOpenedInternal(
    WebCore::PopupContainer* popup_container,
    const WebCore::IntRect& bounds,
    bool activatable) {
  WebViewDelegate* delegate = webview_->delegate();
  if (!delegate)
    return;

  WebWidgetImpl* webwidget =
      static_cast<WebWidgetImpl*>(delegate->CreatePopupWidget(webview_,
                                                              activatable));
  // Convert WebKit types for Chromium.
  std::vector<WebMenuItem> popup_items;
  const WTF::Vector<WebCore::PopupItem*>& items = popup_container->popupData();
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    WebMenuItem menu_item;
    menu_item.label = webkit_glue::StringToString16(items[i]->label);
    menu_item.enabled = items[i]->enabled;
    switch (items[i]->type) {
      case WebCore::PopupItem::TypeOption:
        menu_item.type = WebMenuItem::OPTION;
        break;
      case WebCore::PopupItem::TypeGroup:
        menu_item.type = WebMenuItem::GROUP;
        break;
      case WebCore::PopupItem::TypeSeparator:
        menu_item.type = WebMenuItem::SEPARATOR;
        break;
      default:
        NOTIMPLEMENTED();
    }
    popup_items.push_back(menu_item);
  }

  webwidget->InitWithItems(popup_container,
                           webkit_glue::IntRectToWebRect(bounds),
                           popup_container->menuItemHeight(),
                           popup_container->selectedIndex(),
                           popup_items);
}

void ChromeClientImpl::SetCursor(const WebCursor& cursor) {
  if (ignore_next_set_cursor_) {
    ignore_next_set_cursor_ = false;
    return;
  }

  WebViewDelegate* delegate = webview_->delegate();
  if (delegate)
    delegate->SetCursor(webview_, cursor);
}

void ChromeClientImpl::SetCursorForPlugin(const WebCursor& cursor) {
  SetCursor(cursor);
  // Currently, Widget::setCursor is always called after this function in
  // EventHandler.cpp and since we don't want that we set a flag indicating
  // that the next SetCursor call is to be ignored.
  ignore_next_set_cursor_ = true;
}

void ChromeClientImpl::formStateDidChange(const WebCore::Node*) {
  WebViewDelegate* delegate = webview_->delegate();
  if (delegate)
    delegate->OnNavStateChanged(webview_);
}
