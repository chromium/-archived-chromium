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

#include "config.h"

#pragma warning(push, 0)
#include "Document.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "FormData.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLPluginElement.h"
#include "IntRect.h"
#include "KURL.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformString.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "ScrollView.h"
#include "Widget.h"
#pragma warning(pop)
#undef LOG

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/webview_impl.h"
#include "googleurl/src/gurl.h"
#include "webkit/port/platform/cursor.h"

WebPluginContainer::WebPluginContainer(WebPluginImpl* impl) : impl_(impl) { }

WebPluginContainer::~WebPluginContainer() {
  impl_->SetContainer(NULL);
  MessageLoop::current()->DeleteSoon(FROM_HERE, impl_);
}

NPObject* WebPluginContainer::GetPluginScriptableObject() {
  return impl_->GetPluginScriptableObject();
}

WebCore::IntRect WebPluginContainer::windowClipRect() const {
  return impl_->windowClipRect();
}

void WebPluginContainer::geometryChanged() const {
  impl_->geometryChanged();
}

void WebPluginContainer::setFrameGeometry(const WebCore::IntRect& rect) {
  WebCore::Widget::setFrameGeometry(rect);
  impl_->setFrameGeometry(rect);
}

void WebPluginContainer::paint(WebCore::GraphicsContext* gc,
                               const WebCore::IntRect& damage_rect) {
  // In theory, we should call impl_->print(gc); when
  // impl_->webframe_->printing() is true but it still has placement issues so
  // keep that code off for now.
  impl_->paint(gc, damage_rect);
}

void WebPluginContainer::setFocus() {
  WebCore::Widget::setFocus();
  impl_->setFocus();
}

void WebPluginContainer::show() {
  // We don't want to force a geometry update when the plugin widget is
  // already visible as this involves a geometry update which may lead
  // to unnecessary window moves in the plugin process. The only case
  // where this does not apply is if the force_geometry_update_ flag
  // is set, which occurs when a plugin is created and does not have
  // a parent. We can send out geometry updates only when the plugin
  // widget has a parent.
  if (!impl_->visible_ || impl_->force_geometry_update_) {
    impl_->show();
    WebCore::Widget::show();
    // This is to force an updategeometry call to the plugin process
    // where the plugin window can be hidden or shown.
    geometryChanged();
  }
}

void WebPluginContainer::hide() {
  // Please refer to WebPluginContainer::show for the reasoning behind
  // the if check below.
  if (impl_->visible_ || impl_->force_geometry_update_) {
    impl_->hide();
    WebCore::Widget::hide();
    // This is to force an updategeometry call to the plugin process
    // where the plugin window can be hidden or shown.
    geometryChanged();
  }
}

void WebPluginContainer::handleEvent(WebCore::Event* event) {
  impl_->handleEvent(event);
}

void WebPluginContainer::attachToWindow() {
  Widget::attachToWindow();
  show();
}

void WebPluginContainer::detachFromWindow() {
  Widget::detachFromWindow();
  hide();
}

void WebPluginContainer::didReceiveResponse(
    const WebCore::ResourceResponse& response) {

  std::wstring url = webkit_glue::StringToStdWString(response.url().string());
  std::string ascii_url = WideToASCII(url);

  std::wstring mime_type(webkit_glue::StringToStdWString(response.mimeType()));

  uint32 last_modified = static_cast<uint32>(response.lastModifiedDate());
  uint32 expected_length =
      static_cast<uint32>(response.expectedContentLength());
  WebCore::String content_encoding =
      response.httpHeaderField("Content-Encoding");
  if (!content_encoding.isNull() && content_encoding != "identity") {
    // Don't send the compressed content length to the plugin, which only
    // cares about the decoded length.
    expected_length = 0;
  }

  impl_->delegate_->DidReceiveManualResponse(
      ascii_url, WideToNativeMB(mime_type),
      WideToNativeMB(impl_->GetAllHeaders(response)),
      expected_length, last_modified);
}

void WebPluginContainer::didReceiveData(const char *buffer, int length) {
  impl_->delegate_->DidReceiveManualData(buffer, length);
}

void WebPluginContainer::didFinishLoading() {
  impl_->delegate_->DidFinishManualLoading();
}

void WebPluginContainer::didFail(const WebCore::ResourceError&) {
  impl_->delegate_->DidManualLoadFail();
}

WebCore::Widget* WebPluginImpl::Create(const GURL& url,
                                       char** argn,
                                       char** argv,
                                       int argc,
                                       WebCore::Element *element,
                                       WebFrameImpl *frame,
                                       WebPluginDelegate* delegate,
                                       bool load_manually) {
  WebPluginImpl* webplugin = new WebPluginImpl(element, frame, delegate);

  if (!delegate->Initialize(url, argn, argv, argc, webplugin, load_manually)) {
    delegate->PluginDestroyed();
    delegate = NULL;
    delete webplugin;
    return NULL;
  }

  WebPluginContainer* container = new WebPluginContainer(webplugin);
  webplugin->SetContainer(container);
  return container;
}

WebPluginImpl::WebPluginImpl(WebCore::Element* element,
                             WebFrameImpl* webframe,
                             WebPluginDelegate* delegate)
    : element_(element),
      webframe_(webframe),
      delegate_(delegate),
      windowless_(false),
      window_(NULL),
      force_geometry_update_(false),
      visible_(false),
      widget_(NULL),
      received_first_paint_notification_(false) {
}

WebPluginImpl::~WebPluginImpl() {
}

void WebPluginImpl::SetWindow(HWND window, HANDLE pump_messages_event) {
  if (window) {
    DCHECK(!windowless_);  // Make sure not called twice.
    window_ = window;
  } else {
    DCHECK(!window_);  // Make sure not called twice.
    windowless_ = true;
  }
}

bool WebPluginImpl::CompleteURL(const std::string& url_in,
                                std::string* url_out) {
  if (!frame() || !frame()->document()) {
    NOTREACHED();
    return false;
  }

  WebCore::String str(webkit_glue::StdStringToString(url_in));
  WebCore::String url = frame()->document()->completeURL(str);
  std::wstring wurl = webkit_glue::StringToStdWString(url);
  *url_out = WideToUTF8(wurl);
  return true;
}

bool WebPluginImpl::ExecuteScript(const std::string& url,
                                  const std::wstring& script,
                                  bool notify_needed,
                                  int notify_data,
                                  bool popups_allowed) {
  // This could happen if the WebPluginContainer was already deleted.
  if (!frame())
    return false;

  // Pending resource fetches should also not trigger a callback.
  webframe_->set_plugin_delegate(NULL);

  WebCore::String script_str(webkit_glue::StdWStringToString(script));

  // Note: the call to executeScript might result in the frame being
  // deleted, so add an extra reference to it in this scope.
  // For KJS, keeping a pointer to the JSBridge is enough, but for V8
  // we also need to addref the frame.
  WTF::RefPtr<WebCore::Frame> cur_frame(frame());
  WebCore::JSBridge* bridge = cur_frame->scriptBridge();

  bool succ = false;
  WebCore::String result_str = frame()->loader()->executeScript(script_str,
                                                                &succ,
                                                                popups_allowed);
  std::wstring result;
  if (succ)
    result = webkit_glue::StringToStdWString(result_str);

  // delegate_ could be NULL because executeScript caused the container to be
  // deleted.
  if (delegate_)
    delegate_->SendJavaScriptStream(url, result, succ, notify_needed,
                                    notify_data);

  return succ;
}

void WebPluginImpl::CancelResource(int id) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].id == id) {
      if (clients_[i].handle) {
        clients_[i].handle->cancel();
        RemoveClient(i);
      }

      return;
    }
  }
}

bool WebPluginImpl::SetPostData(WebCore::ResourceRequest* request,
                                const char *buf,
                                uint32 length) {
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<char> body;
  bool rv = NPAPI::PluginHost::SetPostData(buf, length, &names, &values, &body);

  for (size_t i = 0; i < names.size(); ++i)
    request->addHTTPHeaderField(webkit_glue::StdStringToString(names[i]),
                                webkit_glue::StdStringToString(values[i]));

  WebCore::FormData *data = new WebCore::FormData();
  if (body.size())
    data->appendData(&body.front(), body.size());

  request->setHTTPBody(data);  // request refcounts FormData

  return rv;
}

RoutingStatus WebPluginImpl::RouteToFrame(const char *method,
                                          bool is_javascript_url,
                                          const char* target, unsigned int len,
                                          const char* buf, bool is_file_data,
                                          bool notify, const char* url,
                                          GURL* completeURL) {
  // If there is no target, there is nothing to do
  if (!target)
    return NOT_ROUTED;

  // This could happen if the WebPluginContainer was already deleted.
  if (!frame())
    return NOT_ROUTED;

  // Take special action for javascript URLs
  WebCore::DeprecatedString str_target = target;
  if (is_javascript_url) {
    WebCore::Frame *frameTarget = frame()->tree()->find(str_target);
    // For security reasons, do not allow javascript on frames
    // other than this frame.
    if (frameTarget != frame()) {
      // FIXME - might be good to log this into a security
      //         log somewhere.
      return ROUTED;
    }

    // Route javascript calls back to the plugin.
    return NOT_ROUTED;
  }

  // If we got this far, we're routing content to a target frame.
  // Go fetch the URL.

  WebCore::String complete_url_str = frame()->document()->completeURL(
      WebCore::String(url));

  WebCore::KURL complete_url_kurl(complete_url_str.deprecatedString());

  if (strcmp(method, "GET") != 0) {
    const WebCore::DeprecatedString& protocol_scheme =
          complete_url_kurl.protocol();
    // We're only going to route HTTP/HTTPS requests
    if ((protocol_scheme != "http") && (protocol_scheme != "https"))
      return INVALID_URL;
  }

  // url.deprecatedString());
  *completeURL = webkit_glue::KURLToGURL(complete_url_kurl);
  WebCore::ResourceRequest request(complete_url_kurl);
  request.setHTTPMethod(method);
  if (len > 0) {
    if (!is_file_data) {
      if (!SetPostData(&request, buf, len)) {
        // Uhoh - we're in trouble.  There isn't a good way
        // to recover at this point.  Break out.
        ASSERT_NOT_REACHED();
        return ROUTED;
      }
    } else {
      // TODO: Support "file" mode.  For now, just break out
      // since proceeding may do something unintentional.
      ASSERT_NOT_REACHED();
      return ROUTED;
    }
  }
  WebCore::FrameLoadRequest load_request(request);
  load_request.setFrameName(str_target);
  WebCore::FrameLoader *loader = frame()->loader();
  // we actually don't know whether usergesture is true or false,
  // passing true since all we can do is assume it is okay.
  loader->load(load_request,
               false,  // lock history
               true,   // user gesture
               0,      // event
               0,      // form element
               HashMap<WebCore::String, WebCore::String>());

  // load() can cause the frame to go away.
  if (webframe_) {
    WebPluginDelegate* last_plugin = webframe_->plugin_delegate();
    if (last_plugin) {
      last_plugin->DidFinishLoadWithReason(NPRES_USER_BREAK);
      webframe_->set_plugin_delegate(NULL);
    }

    if (notify)
      webframe_->set_plugin_delegate(delegate_);
  }

  return ROUTED;
}

NPObject* WebPluginImpl::GetWindowScriptNPObject() {
  if (!frame()) {
    ASSERT_NOT_REACHED();
    return 0;
  }

  return frame()->windowScriptNPObject();
}

NPObject* WebPluginImpl::GetPluginElement() {
  // We don't really know that this is a
  // HTMLPluginElement.  Cast to it and hope?
  WebCore::HTMLPlugInElement *plugin_element =
      static_cast<WebCore::HTMLPlugInElement*>(element_);
  return plugin_element->getNPObject();
}

void WebPluginImpl::SetCookie(const GURL& url,
                              const GURL& policy_url,
                              const std::string& cookie) {
  webkit_glue::SetCookie(url, policy_url, cookie);
}

std::string WebPluginImpl::GetCookies(const GURL& url, const GURL& policy_url) {
  return webkit_glue::GetCookies(url, policy_url);
}

void WebPluginImpl::ShowModalHTMLDialog(const GURL& url, int width, int height,
                                        const std::string& json_arguments,
                                        std::string* json_retval) {
  // TODO(mpcomplete): Figure out how to call out to the RenderView and
  // implement this.  Though, this is never called atm - only the out-of-process
  // version is used.
  NOTREACHED();
}

void WebPluginImpl::OnMissingPluginStatus(int status) {
  NOTREACHED();
}

void WebPluginImpl::Invalidate() {
  if (widget_)
    widget_->invalidate();
}

void WebPluginImpl::InvalidateRect(const gfx::Rect& rect) {
  if (widget_)
    widget_->invalidateRect(WebCore::IntRect(rect.ToRECT()));
}

WebCore::IntRect WebPluginImpl::windowClipRect() const {
  // This is based on the code in WebCore/plugins/win/PluginViewWin.cpp:
  WebCore::IntRect rect(0, 0, widget_->width(), widget_->height());

  // Start by clipping to our bounds.
  WebCore::IntRect clip_rect = widget_->convertToContainingWindow(
      WebCore::IntRect(0, 0, widget_->width(), widget_->height()));

  // Take our element and get the clip rect from the enclosing layer and
  // frame view.
  WebCore::RenderLayer* layer = element_->renderer()->enclosingLayer();

  // document()->renderer() can be NULL when we receive messages from the
  // plugins while we are destroying a frame.
  if (element_->renderer()->document()->renderer()) {
    WebCore::FrameView* parent_view = element_->document()->view();
    clip_rect.intersect(parent_view->windowClipRectForLayer(layer, true));
  }

  return clip_rect;
}

void WebPluginImpl::geometryChanged() const {
  if (!widget_)
    return;

  // This is a hack to tickle re-positioning of the plugin in the case where
  // our parent view was scrolled.
  const_cast<WebPluginImpl*>(this)->widget_->setFrameGeometry(
      widget_->frameGeometry());
  }

void WebPluginImpl::setFrameGeometry(const WebCore::IntRect& rect) {
  // Compute a new position and clip rect for ourselves relative to the
  // containing window.  We ask our delegate to reposition us accordingly.

  // When the plugin is loaded we don't have a parent frame yet. We need
  // to force the plugin window to get created in the plugin process,
  // when the plugin widget position is updated. This occurs just after
  // the plugin is loaded (See http://b/issue?id=892174).
  if (!parent()) {
    force_geometry_update_ = true;
    return;
  }

  WebCore::Frame* frame = element_->document()->frame();
  WebFrameImpl* webframe = WebFrameImpl::FromFrame(frame);
  WebViewImpl* webview = webframe->webview_impl();
  // It is valid for this function to be invoked in code paths where the
  // the webview is closed.
  if (!webview->delegate()) {
    return;
  }

  WebCore::IntRect window_rect;
  WebCore::IntRect clip_rect;
  CalculateBounds(rect, &window_rect, &clip_rect);

  if (window_ && received_first_paint_notification_) {
    // Let the WebViewDelegate know that the plugin window needs to be moved,
    // so that all the HWNDs are moved together.
    WebPluginGeometry move;
    move.window = window_;
    move.window_rect = gfx::Rect(window_rect);
    move.clip_rect = gfx::Rect(clip_rect);
    move.visible = visible_;
    webview->delegate()->DidMove(webview, move);
  }

  delegate_->UpdateGeometry(
      gfx::Rect(window_rect), gfx::Rect(clip_rect),
      received_first_paint_notification_? visible_ : false);

  // delegate_ can go away as a result of above call, so check it first.
  if (force_geometry_update_ && delegate_) {
    force_geometry_update_ = false;
    delegate_->FlushGeometryUpdates();
  }
}

void WebPluginImpl::paint(WebCore::GraphicsContext* gc,
                          const WebCore::IntRect& damage_rect) {
  if (gc->paintingDisabled())
    return;

  if (!parent())
    return;

  // Don't paint anything if the plugin doesn't intersect the damage rect.
  if (!widget_->frameGeometry().intersects(damage_rect))
    return;

  // A windowed plugin starts out by being invisible regardless of the style
  // which webkit tells us. The paint notification from webkit indicates that
  // the plugin widget is being shown and we need to make sure that
  // it becomes visible.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=18901 for more
  // details on this issue.
  // TODO(iyengar): Remove this hack when this issue is fixed in webkit.
  if (!received_first_paint_notification_) {
    received_first_paint_notification_ = true;

    if (!windowless_) {
      WebCore::IntRect window_rect;
      WebCore::IntRect clip_rect;

      CalculateBounds(widget_->frameGeometry(), &window_rect, &clip_rect);

      delegate_->UpdateGeometry(gfx::Rect(window_rect), gfx::Rect(clip_rect),
                                visible_);
      delegate_->FlushGeometryUpdates();
    }
  }

  gc->save();

  DCHECK(parent()->isFrameView());
  WebCore::FrameView* view = static_cast<WebCore::FrameView*>(parent());

  // The plugin is positioned in window coordinates, so it needs to be painted
  // in window coordinates.
  WebCore::IntPoint origin = view->windowToContents(WebCore::IntPoint(0, 0));
  gc->translate(static_cast<float>(origin.x()),
                static_cast<float>(origin.y()));

  // HDC is only used when in windowless mode.
  HDC hdc = gc->getWindowsContext();

  WebCore::IntRect window_rect =
      WebCore::IntRect(view->contentsToWindow(damage_rect.location()),
                       damage_rect.size());

  delegate_->Paint(hdc, gfx::Rect(window_rect));

  gc->releaseWindowsContext(hdc);
  gc->restore();
}

void WebPluginImpl::print(WebCore::GraphicsContext* gc) {
  if (gc->paintingDisabled())
    return;

  if (!parent())
    return;

  gc->save();
  HDC hdc = gc->getWindowsContext();
  delegate_->Print(hdc);
  gc->releaseWindowsContext(hdc);
  gc->restore();
}

void WebPluginImpl::setFocus() {
  if (windowless_)
    delegate_->SetFocus();
}

void WebPluginImpl::show() {
  visible_ = true;
}

void WebPluginImpl::hide() {
  visible_ = false;
}

void WebPluginImpl::handleEvent(WebCore::Event* event) {
  if (!windowless_)
    return;

  // Pass events to the plugin.
  // The events we pass are defined at:
  //    http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/structures5.html#1000000
  // Don't take the documentation as truth, however.  I've found
  // many cases where mozilla behaves differently than the spec.
  if (event->isMouseEvent())
    handleMouseEvent(static_cast<WebCore::MouseEvent*>(event));
  else if (event->isKeyboardEvent())
    handleKeyboardEvent(static_cast<WebCore::KeyboardEvent*>(event));
}

void WebPluginImpl::handleMouseEvent(WebCore::MouseEvent* event) {
  DCHECK(parent()->isFrameView());
  WebCore::IntPoint p =
      static_cast<WebCore::FrameView*>(parent())->contentsToWindow(
          WebCore::IntPoint(event->pageX(), event->pageY()));
  NPEvent np_event;
  np_event.lParam = static_cast<uint32>(MAKELPARAM(p.x(), p.y()));
  np_event.wParam = 0;

  if (event->ctrlKey())
    np_event.wParam |= MK_CONTROL;
  if (event->shiftKey())
    np_event.wParam |= MK_SHIFT;

  if ((event->type() == WebCore::EventNames::mousemoveEvent) ||
      (event->type() == WebCore::EventNames::mouseoutEvent) ||
      (event->type() == WebCore::EventNames::mouseoverEvent)) {
    np_event.event = WM_MOUSEMOVE;
    if (event->buttonDown()) {
      switch (event->button()) {
        case WebCore::LeftButton:
          np_event.wParam |= MK_LBUTTON;
          break;
        case WebCore::MiddleButton:
          np_event.wParam |= MK_MBUTTON;
          break;
        case WebCore::RightButton:
          np_event.wParam |= MK_RBUTTON;
          break;
      }
    }
  } else if (event->type() == WebCore::EventNames::mousedownEvent) {
    // Ensure that the frame containing the plugin has focus.
    WebCore::Frame* containing_frame = webframe_->frame();
    if (WebCore::Page* current_page = containing_frame->page()) {
      current_page->focusController()->setFocusedFrame(containing_frame);
    }
    // Give focus to our containing HTMLPluginElement.
    containing_frame->document()->setFocusedNode(element_);

    // Ideally we'd translate to WM_xBUTTONDBLCLK here if the click count were
    // a multiple of 2.  But there seems to be no way to get at the click count
    // or the original Windows message from the WebCore::Event.
    switch (event->button()) {
      case WebCore::LeftButton:
        np_event.event = WM_LBUTTONDOWN;
        np_event.wParam |= MK_LBUTTON;
        break;
      case WebCore::MiddleButton:
        np_event.event = WM_MBUTTONDOWN;
        np_event.wParam |= MK_MBUTTON;
        break;
      case WebCore::RightButton:
        np_event.event = WM_RBUTTONDOWN;
        np_event.wParam |= MK_RBUTTON;
        break;
    }
  } else if (event->type() == WebCore::EventNames::mouseupEvent) {
    switch (event->button()) {
      case WebCore::LeftButton:
        np_event.event = WM_LBUTTONUP;
        break;
      case WebCore::MiddleButton:
        np_event.event = WM_MBUTTONUP;
        break;
      case WebCore::RightButton:
        np_event.event = WM_RBUTTONUP;
        break;
    }
  } else {
    // Skip all other mouse events.
    return;
  }

  // TODO(pkasting): http://b/1119691 This conditional seems exactly backwards,
  // but it matches Safari's code, and if I reverse it, giving focus to a
  // transparent (windowless) plugin fails.
  WebCursor current_web_cursor;
  if (!delegate_->HandleEvent(&np_event, &current_web_cursor))
    event->setDefaultHandled();
  // A windowless plugin can change the cursor in response to the WM_MOUSEMOVE
  // event. We need to reflect the changed cursor in the frame view as the
  // the mouse is moved in the boundaries of the windowless plugin.
  parent()->setCursor(current_web_cursor);
}

void WebPluginImpl::handleKeyboardEvent(WebCore::KeyboardEvent* event) {
  NPEvent np_event;
  np_event.wParam = event->keyCode();

  if (event->type() == WebCore::EventNames::keydownEvent) {
    np_event.event = WM_KEYDOWN;
    np_event.lParam = 0;
  } else if (event->type() == WebCore::EventNames::keyupEvent) {
    np_event.event = WM_KEYUP;
    np_event.lParam = 0x8000;
  } else {
    // Skip all other keyboard events.
    return;
  }

  // TODO(pkasting): http://b/1119691 See above.
  WebCursor current_web_cursor;
  if (!delegate_->HandleEvent(&np_event, &current_web_cursor))
    event->setDefaultHandled();
}

NPObject* WebPluginImpl::GetPluginScriptableObject() {
  return delegate_->GetPluginScriptableObject();
}

WebPluginResourceClient* WebPluginImpl::GetClientFromHandle(
    WebCore::ResourceHandle* handle) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].handle.get() == handle)
      return clients_[i].client;
  }

  NOTREACHED();
  return 0;
}


void WebPluginImpl::willSendRequest(WebCore::ResourceHandle* handle,
                                    WebCore::ResourceRequest& request,
                                    const WebCore::ResourceResponse&) {
  WebPluginResourceClient* client = GetClientFromHandle(handle);
  if (client) {
    GURL gurl(webkit_glue::KURLToGURL(request.url()));
    client->WillSendRequest(gurl);
  }
}

std::wstring WebPluginImpl::GetAllHeaders(
    const WebCore::ResourceResponse& response) {
  std::wstring result;
  const WebCore::String& status = response.httpStatusText();
  if (status.isEmpty())
    return result;

  result.append(L"HTTP ");
  result.append(FormatNumber(response.httpStatusCode()));
  result.append(L" ");
  result.append(status.characters(), status.length());
  result.append(L"\n");

  WebCore::HTTPHeaderMap::const_iterator it =
      response.httpHeaderFields().begin();
  for (; it != response.httpHeaderFields().end(); ++it) {
    if (!it->first.isEmpty() && !it->second.isEmpty()) {
      result.append(std::wstring(it->first.characters(), it->first.length()));
      result.append(L": ");
      result.append(std::wstring(it->second.characters(), it->second.length()));
      result.append(L"\n");
    }
  }

  return result;
}

void WebPluginImpl::didReceiveResponse(WebCore::ResourceHandle* handle,
    const WebCore::ResourceResponse& response) {
  WebPluginResourceClient* client = GetClientFromHandle(handle);
  if (!client)
    return;

  bool cancel = false;
  std::wstring mime_type(webkit_glue::StringToStdWString(response.mimeType()));

  uint32 last_modified = static_cast<uint32>(response.lastModifiedDate());
  uint32 expected_length =
      static_cast<uint32>(response.expectedContentLength());
  WebCore::String content_encoding =
      response.httpHeaderField("Content-Encoding");
  if (!content_encoding.isNull() && content_encoding != "identity") {
    // Don't send the compressed content length to the plugin, which only
    // cares about the decoded length.
    expected_length = 0;
  }

  client->DidReceiveResponse(WideToNativeMB(mime_type),
                             WideToNativeMB(GetAllHeaders(response)),
                             expected_length,
                             last_modified,
                             &cancel);
  if (cancel) {
    handle->cancel();
    RemoveClient(handle);
    return;
  }

  // Bug http://b/issue?id=925559. The flash plugin would not handle the HTTP
  // error codes in the stream header and as a result, was unaware of the
  // fate of the HTTP requests issued via NPN_GetURLNotify. Webkit and FF
  // destroy the stream and invoke the NPP_DestroyStream function on the
  // plugin if the HTTP request fails.
  const WebCore::DeprecatedString& protocol_scheme =
      response.url().protocol();
  if ((protocol_scheme == "http") || (protocol_scheme == "https")) {
    if (response.httpStatusCode() < 100 || response.httpStatusCode() >= 400) {
      // The plugin instance could be in the process of deletion here.
      // Verify if the WebPluginResourceClient instance still exists before
      // use.
      WebPluginResourceClient* resource_client = GetClientFromHandle(handle);
      if (resource_client) {
        handle->cancel();
        resource_client->DidFail();
        RemoveClient(handle);
      }
    }
  }
}

void WebPluginImpl::didReceiveData(WebCore::ResourceHandle* handle,
                                   const char *buffer,
                                   int length, int) {
  WebPluginResourceClient* client = GetClientFromHandle(handle);
  if (client)
    client->DidReceiveData(buffer, length);
}

void WebPluginImpl::didFinishLoading(WebCore::ResourceHandle* handle) {
  WebPluginResourceClient* client = GetClientFromHandle(handle);
  if (client)
    client->DidFinishLoading();

  RemoveClient(handle);
}

void WebPluginImpl::didFail(WebCore::ResourceHandle* handle,
                            const WebCore::ResourceError&) {
  WebPluginResourceClient* client = GetClientFromHandle(handle);
  if (client)
    client->DidFail();

  RemoveClient(handle);
}

void WebPluginImpl::RemoveClient(size_t i) {
  clients_.erase(clients_.begin() + i);
}

void WebPluginImpl::RemoveClient(WebCore::ResourceHandle* handle) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].handle.get() == handle) {
      RemoveClient(i);
      return;
    }
  }
}

void WebPluginImpl::SetContainer(WebPluginContainer* container) {
  if (container == NULL) {
    // The frame maintains a list of JSObjects which are related to this
    // plugin.  Tell the frame we're gone so that it can invalidate all
    // of those sub JSObjects.
    if (frame()) {
      ASSERT(widget_ != NULL);
      frame()->cleanupScriptObjectsForPlugin(widget_);
    }

    // Call PluginDestroyed() first to prevent the plugin from calling us back
    // in the middle of tearing down the render tree.
    delegate_->PluginDestroyed();
    delegate_ = NULL;

    // Cancel any pending requests because otherwise this deleted object will be
    // called by the ResourceDispatcher.
    int int_offset = 0;
    while (!clients_.empty()) {
      if (clients_[int_offset].handle)
        clients_[int_offset].handle->cancel();
      WebPluginResourceClient* resource_client = clients_[int_offset].client;
      RemoveClient(int_offset);
      if (resource_client)
        resource_client->DidFail();
    }

    // This needs to be called now and not in the destructor since the
    // webframe_ might not be valid anymore.
    webframe_->set_plugin_delegate(NULL);
    webframe_ = NULL;
  }
  widget_ = container;
}

WebCore::ScrollView* WebPluginImpl::parent() const {
  if (widget_)
    return widget_->parent();

  return NULL;
}

void WebPluginImpl::CalculateBounds(const WebCore::IntRect& frame_rect,
                                    WebCore::IntRect* window_rect,
                                    WebCore::IntRect* clip_rect) {
  DCHECK(parent()->isFrameView());
  WebCore::FrameView* view = static_cast<WebCore::FrameView*>(parent());

  *window_rect =
      WebCore::IntRect(view->contentsToWindow(frame_rect.location()),
                                              frame_rect.size());
  // Calculate a clip-rect so that we don't overlap the scrollbars, etc.
  *clip_rect = widget_->windowClipRect();
  clip_rect->move(-window_rect->x(), -window_rect->y());
}

void WebPluginImpl::HandleURLRequest(const char *method,
                                     bool is_javascript_url,
                                     const char* target, unsigned int len,
                                     const char* buf, bool is_file_data,
                                     bool notify, const char* url,
                                     void* notify_data, bool popups_allowed) {
  // For this request, we either route the output to a frame
  // because a target has been specified, or we handle the request
  // here, i.e. by executing the script if it is a javascript url
  // or by initiating a download on the URL, etc. There is one special
  // case in that the request is a javascript url and the target is "_self",
  // in which case we route the output to the plugin rather than routing it
  // to the plugin's frame.
  GURL complete_url;
  int routing_status = RouteToFrame(method, is_javascript_url, target, len,
                                    buf, is_file_data, notify, url,
                                    &complete_url);
  if (routing_status == ROUTED) {
    // The delegate could have gone away because of this call.
    if (delegate_)
      delegate_->URLRequestRouted(url, notify, notify_data);
    return;
  }

  if (is_javascript_url) {
    std::string original_url = url;

    // Convert the javascript: URL to javascript by unescaping. WebCore uses
    // decode_string for this, so we do, too.
    std::string escaped_script = original_url.substr(strlen("javascript:"));
    WebCore::DeprecatedString script = WebCore::KURL::decode_string(
        WebCore::DeprecatedString(escaped_script.data(),
                                  static_cast<int>(escaped_script.length())));

    ExecuteScript(original_url,
                  webkit_glue::DeprecatedStringToStdWString(script), notify,
                  reinterpret_cast<int>(notify_data), popups_allowed);
  } else {
    std::string complete_url_string;
    CompleteURL(url, &complete_url_string);

    int resource_id = GetNextResourceId();
    WebPluginResourceClient* resource_client =
        delegate_->CreateResourceClient(resource_id, complete_url_string,
                                        notify, notify_data);

    // If the RouteToFrame call returned a failure then inform the result
    // back to the plugin asynchronously.
    if ((routing_status == INVALID_URL) ||
        (routing_status == GENERAL_FAILURE)) {
      resource_client->DidFail();
      return;
    }

    InitiateHTTPRequest(resource_id, resource_client, method, buf, len,
                        GURL(complete_url_string));
  }
}

int WebPluginImpl::GetNextResourceId() {
  static int next_id = 0;
  return ++next_id;
}

bool WebPluginImpl::InitiateHTTPRequest(int resource_id,
                                        WebPluginResourceClient* client,
                                        const char* method, const char* buf,
                                        int buf_len,
                                        const GURL& complete_url_string) {
  if (!client) {
    NOTREACHED();
    return false;
  }

  ClientInfo info;
  info.id = resource_id;
  info.client = client;
  info.request.setFrame(frame());
  info.request.setURL(webkit_glue::GURLToKURL(complete_url_string));
  info.request.setOriginPid(delegate_->GetProcessId());
  info.request.setResourceType(ResourceType::OBJECT);
  info.request.setHTTPMethod(method);

  const WebCore::String& referrer =  frame()->loader()->outgoingReferrer();
  if (!WebCore::FrameLoader::shouldHideReferrer(
          complete_url_string.spec().c_str(), referrer)) {
    info.request.setHTTPReferrer(referrer);
  }

  if (lstrcmpA(method, "POST") == 0) {
    // Adds headers or form data to a request.  This must be called before
    // we initiate the actual request.
    SetPostData(&info.request, buf, buf_len);
  }

  info.handle = WebCore::ResourceHandle::create(info.request, this, frame(),
                                                false, false);
  if (!info.handle) {
    return false;
  }

  clients_.push_back(info);
  return true;
}
