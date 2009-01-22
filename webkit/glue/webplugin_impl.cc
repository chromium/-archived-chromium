// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Cursor.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "FloatPoint.h"
#include "FormData.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "IntRect.h"
#include "KURL.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformContextSkia.h"
#include "PlatformMouseEvent.h"
#include "PlatformString.h"
#include "RenderBox.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "ScriptController.h"
#include "ScriptValue.h"
#include "ScrollView.h"
#include "Widget.h"
MSVC_POP_WARNING();
#undef LOG

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "net/base/escape.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/plugins/plugin_host.h"
#if defined(OS_WIN)
#include "webkit/glue/plugins/plugin_instance.h"
#endif
#include "webkit/glue/stacking_order_iterator.h"
#include "webkit/glue/webview_impl.h"
#include "googleurl/src/gurl.h"

// This class handles individual multipart responses. It is instantiated when
// we receive HTTP status code 206 in the HTTP response. This indicates
// that the response could have multiple parts each separated by a boundary
// specified in the response header.
class MultiPartResponseClient : public WebCore::ResourceHandleClient {
 public:
  MultiPartResponseClient(WebPluginResourceClient* resource_client)
      : resource_client_(resource_client) {
    Clear();
  }

  // Called when the multipart parser encounters an embedded multipart
  // response.
  virtual void didReceiveResponse(WebCore::ResourceHandle* handle,
                                  const WebCore::ResourceResponse& response) {
    if (!MultipartResponseDelegate::ReadContentRanges(
            response, &byte_range_lower_bound_, &byte_range_upper_bound_)) {
      NOTREACHED();
      return;
    }

    resource_response_ = response;
  }

  // Receives individual part data from a multipart response.
  virtual void didReceiveData(WebCore::ResourceHandle* handle,
                              const char* data, int boundary_pos,
                              int length_received) {
    int data_length = byte_range_upper_bound_ - byte_range_lower_bound_ + 1;
    resource_client_->DidReceiveData(
        data, data_length, byte_range_lower_bound_);
  }

  void Clear() {
    resource_response_ = WebCore::ResourceResponse();
    byte_range_lower_bound_ = 0;
    byte_range_upper_bound_ = 0;
  }

 private:
  WebCore::ResourceResponse resource_response_;
  // The lower bound of the byte range.
  int byte_range_lower_bound_;
  // The upper bound of the byte range.
  int byte_range_upper_bound_;
  // The handler for the data.
  WebPluginResourceClient* resource_client_;
};

WebPluginContainer::WebPluginContainer(WebPluginImpl* impl) 
    : impl_(impl),
      ignore_response_error_(false) {
}

WebPluginContainer::~WebPluginContainer() {
  impl_->SetContainer(NULL);
  MessageLoop::current()->DeleteSoon(FROM_HERE, impl_);
}

NPObject* WebPluginContainer::GetPluginScriptableObject() {
  return impl_->GetPluginScriptableObject();
}

#if USE(JSC)
bool WebPluginContainer::isPluginView() const { 
  return true; 
}
#endif


void WebPluginContainer::setFrameRect(const WebCore::IntRect& rect) {
  // WebKit calls move every time it paints (see RenderWidget::paint).  No need
  // to do expensive operations if we didn't actually move.
  if (rect == frameRect())
    return;

  WebCore::Widget::setFrameRect(rect);
  impl_->setFrameRect(rect);
}

void WebPluginContainer::paint(WebCore::GraphicsContext* gc,
                               const WebCore::IntRect& damage_rect) {
  // In theory, we should call impl_->print(gc); when
  // impl_->webframe_->printing() is true but it still has placement issues so
  // keep that code off for now.
  impl_->paint(gc, damage_rect);
}

void WebPluginContainer::invalidateRect(const WebCore::IntRect& rect) {
  if (parent()) {
    WebCore::IntRect damageRect = convertToContainingWindow(rect);

    // Get our clip rect and intersect with it to ensure we don't invalidate too much.
    WebCore::IntRect clipRect = parent()->windowClipRect();
    damageRect.intersect(clipRect);

    parent()->hostWindow()->repaint(damageRect, true);
  }
}

void WebPluginContainer::setFocus() {
  WebCore::Widget::setFocus();
  impl_->setFocus();
}

void WebPluginContainer::show() {
  // We don't want to force a geometry update when the plugin widget is
  // already visible as this involves a geometry update which may lead
  // to unnecessary window moves in the plugin process.
  if (!impl_->visible_) {
    impl_->show();
    WebCore::Widget::show();
    // This is to force an updategeometry call to the plugin process
    // where the plugin window can be hidden or shown.
    frameRectsChanged();
  }
}

void WebPluginContainer::hide() {
  if (impl_->visible_) {
    impl_->hide();
    WebCore::Widget::hide();
    // This is to force an updategeometry call to the plugin process
    // where the plugin window can be hidden or shown.
    frameRectsChanged();
  }
}

void WebPluginContainer::handleEvent(WebCore::Event* event) {
  impl_->handleEvent(event);
}

void WebPluginContainer::frameRectsChanged() {
  WebCore::Widget::frameRectsChanged();
  // This is a hack to tickle re-positioning of the plugin in the case where
  // our parent view was scrolled.
  impl_->setFrameRect(frameRect());
}

// We override this function so that if the plugin is windowed, we can call
// NPP_SetWindow at the first possible moment.  This ensures that NPP_SetWindow
// is called before the manual load data is sent to a plugin.  If this order is
// reversed, Flash won't load videos.
void WebPluginContainer::setParent(WebCore::ScrollView* view) {
  WebCore::Widget::setParent(view);
  if (view) {    
    impl_->setFrameRect(frameRect());
    impl_->delegate_->FlushGeometryUpdates();
  }
}

void WebPluginContainer::windowCutoutRects(const WebCore::IntRect& bounds,
                                           WTF::Vector<WebCore::IntRect>*
                                           cutouts) const {
  impl_->windowCutoutRects(bounds, cutouts);
}

void WebPluginContainer::didReceiveResponse(
    const WebCore::ResourceResponse& response) {
  set_ignore_response_error(false);

  HttpResponseInfo http_response_info;
  ReadHttpResponseInfo(response, &http_response_info);

  impl_->delegate_->DidReceiveManualResponse(
      http_response_info.url,
      base::SysWideToNativeMB(http_response_info.mime_type),
      base::SysWideToNativeMB(impl_->GetAllHeaders(response)),
      http_response_info.expected_length,
      http_response_info.last_modified);
}

void WebPluginContainer::didReceiveData(const char *buffer, int length) {
  impl_->delegate_->DidReceiveManualData(buffer, length);
}

void WebPluginContainer::didFinishLoading() {
  impl_->delegate_->DidFinishManualLoading();
}

void WebPluginContainer::didFail(const WebCore::ResourceError&) {
  if (!ignore_response_error_)
    impl_->delegate_->DidManualLoadFail();
}

void WebPluginContainer::ReadHttpResponseInfo(
    const WebCore::ResourceResponse& response,
    HttpResponseInfo* http_response) {
  std::wstring url = webkit_glue::StringToStdWString(response.url().string());
  http_response->url = WideToASCII(url);

  http_response->mime_type =
      webkit_glue::StringToStdWString(response.mimeType());

  http_response->last_modified =
      static_cast<uint32>(response.lastModifiedDate());
  // If the length comes in as -1, then it indicates that it was not
  // read off the HTTP headers. We replicate Safari webkit behavior here,
  // which is to set it to 0.
  http_response->expected_length =
      static_cast<uint32>(std::max(response.expectedContentLength(), 0LL));
  WebCore::String content_encoding =
      response.httpHeaderField("Content-Encoding");
  if (!content_encoding.isNull() && content_encoding != "identity") {
    // Don't send the compressed content length to the plugin, which only
    // cares about the decoded length.
    http_response->expected_length = 0;
  }
}

WebCore::Widget* WebPluginImpl::Create(const GURL& url,
                                       char** argn,
                                       char** argv,
                                       int argc,
                                       WebCore::Element *element,
                                       WebFrameImpl *frame,
                                       WebPluginDelegate* delegate,
                                       bool load_manually,
                                       const std::string& mime_type) {
  WebPluginImpl* webplugin = new WebPluginImpl(element, frame, delegate, url,
                                               load_manually, mime_type, argc,
                                               argn, argv);

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
                             WebPluginDelegate* delegate,
                             const GURL& plugin_url,
                             bool load_manually,
                             const std::string& mime_type,
                             int arg_count,
                             char** arg_names,
                             char** arg_values)
    : windowless_(false),
      window_(NULL),
      element_(element),
      webframe_(webframe),
      delegate_(delegate),
      visible_(false),
      widget_(NULL),
      plugin_url_(plugin_url),
      load_manually_(load_manually),
      first_geometry_update_(true),
      mime_type_(mime_type) {

  ArrayToVector(arg_count, arg_names, &arg_names_);
  ArrayToVector(arg_count, arg_values, &arg_values_);
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

  WebCore::ScriptValue result = 
      frame()->loader()->executeScript(script_str, popups_allowed);
  WebCore::String script_result;
  std::wstring wresult;
  bool succ = false;
  if (result.getString(script_result)) {
    succ = true;
    wresult = webkit_glue::StringToStdWString(script_result);
  }

  // delegate_ could be NULL because executeScript caused the container to be
  // deleted.
  if (delegate_)
    delegate_->SendJavaScriptStream(url, wresult, succ, notify_needed,
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
#if !defined(OS_LINUX)
  bool rv = NPAPI::PluginHost::SetPostData(buf, length, &names, &values, &body);
#else
  // TODO(port): unstub once we have plugin support
  bool rv = false;
  NOTREACHED();
#endif

  for (size_t i = 0; i < names.size(); ++i)
    request->addHTTPHeaderField(webkit_glue::StdStringToString(names[i]),
                                webkit_glue::StdStringToString(values[i]));

  WebCore::String content_type = request->httpContentType();
  if (content_type.isEmpty())
    request->setHTTPContentType("application/x-www-form-urlencoded");

  RefPtr<WebCore::FormData> data = WebCore::FormData::create();
  if (body.size())
    data->appendData(&body.front(), body.size());

  request->setHTTPBody(data.release());

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

  // Take special action for JavaScript URLs
  WebCore::String str_target = target;
  if (is_javascript_url) {
    WebCore::Frame *frameTarget = frame()->tree()->find(str_target);
    // For security reasons, do not allow JavaScript on frames
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

  WebCore::KURL complete_url_kurl(complete_url_str);

  if (strcmp(method, "GET") != 0) {
    const WebCore::String& protocol_scheme =
          complete_url_kurl.protocol();
    // We're only going to route HTTP/HTTPS requests
    if ((protocol_scheme != "http") && (protocol_scheme != "https"))
      return INVALID_URL;
  }

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
  loader->loadFrameRequestWithFormAndValues(
      load_request,
      false,  // lock history
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

  return frame()->script()->windowScriptNPObject();
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
  if (webframe_ && webframe_->GetView() &&
      webframe_->GetView()->GetDelegate()) {
    webframe_->GetView()->GetDelegate()->ShowModalHTMLDialog(
        url, width, height, json_arguments, json_retval);
  }
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
    widget_->invalidateRect(webkit_glue::ToIntRect(rect));
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

void WebPluginImpl::windowCutoutRects(
    const WebCore::IntRect& bounds,
    WTF::Vector<WebCore::IntRect>* cutouts) const {
  WebCore::RenderObject* plugin_node = element_->renderer();
  ASSERT(plugin_node);

  // Find all iframes that stack higher than this plugin.
  bool higher = false;
  StackingOrderIterator iterator;
  WebCore::RenderLayer* root = element_->document()->renderer()->
                               enclosingLayer();
  iterator.Reset(bounds, root);

  while (WebCore::RenderObject* ro = iterator.Next()) {
    if (ro == plugin_node) {
      // All nodes after this one are higher than plugin.
      higher = true;
    } else if (higher) {
      // Is this a visible iframe?
      WebCore::Node* n = ro->node();
      if (n && n->hasTagName(WebCore::HTMLNames::iframeTag)) {
        if (!ro->style() || ro->style()->visibility() == WebCore::VISIBLE) {
          WebCore::IntPoint point = roundedIntPoint(ro->localToAbsolute());
          WebCore::RenderBox* rbox = WebCore::RenderBox::toRenderBox(ro);
          WebCore::IntSize size(rbox->width(), rbox->height());
          cutouts->append(WebCore::IntRect(point, size));
        }
      }
    }
  }
}

void WebPluginImpl::setFrameRect(const WebCore::IntRect& rect) {
  if (!parent())
    return;

  // Compute a new position and clip rect for ourselves relative to the
  // containing window.  We ask our delegate to reposition us accordingly.
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
  std::vector<gfx::Rect> cutout_rects;
  CalculateBounds(rect, &window_rect, &clip_rect, &cutout_rects);

  delegate_->UpdateGeometry(
      webkit_glue::FromIntRect(window_rect),
      webkit_glue::FromIntRect(clip_rect));

  if (window_) {
    // Let the WebViewDelegate know that the plugin window needs to be moved,
    // so that all the HWNDs are moved together.
    WebPluginGeometry move;
    move.window = window_;
    move.window_rect = webkit_glue::FromIntRect(window_rect);
    move.clip_rect = webkit_glue::FromIntRect(clip_rect);
    move.cutout_rects = cutout_rects;
    move.visible = visible_;

    webview->delegate()->DidMove(webview, move);
  }

  // Initiate a download on the plugin url. This should be done for the
  // first update geometry sequence.
  if (first_geometry_update_) {
    first_geometry_update_ = false;
    // An empty url corresponds to an EMBED tag with no src attribute.
    if (!load_manually_ && plugin_url_.is_valid()) {
      HandleURLRequestInternal("GET", false, NULL, 0, NULL, false, false,
                               plugin_url_.spec().c_str(), NULL, false,
                               false);
    }
  }
}

void WebPluginImpl::paint(WebCore::GraphicsContext* gc,
                          const WebCore::IntRect& damage_rect) {
  if (gc->paintingDisabled())
    return;

  if (!parent())
    return;

  // Don't paint anything if the plugin doesn't intersect the damage rect.
  if (!widget_->frameRect().intersects(damage_rect))
    return;

  gc->save();

  DCHECK(parent()->isFrameView());
  WebCore::FrameView* view = static_cast<WebCore::FrameView*>(parent());

  // The plugin is positioned in window coordinates, so it needs to be painted
  // in window coordinates.
  WebCore::IntPoint origin = view->windowToContents(WebCore::IntPoint(0, 0));
  gc->translate(static_cast<float>(origin.x()),
                static_cast<float>(origin.y()));

#if defined(OS_WIN)
  // HDC is only used when in windowless mode.
  HDC hdc = gc->platformContext()->canvas()->beginPlatformPaint();
#else
  NOTIMPLEMENTED();
#endif

  WebCore::IntRect window_rect =
      WebCore::IntRect(view->contentsToWindow(damage_rect.location()),
                       damage_rect.size());

#if defined(OS_WIN)
  delegate_->Paint(hdc, webkit_glue::FromIntRect(window_rect));

  gc->platformContext()->canvas()->endPlatformPaint();
#endif
  gc->restore();
}

void WebPluginImpl::print(WebCore::GraphicsContext* gc) {
  if (gc->paintingDisabled())
    return;

  if (!parent())
    return;

  gc->save();
#if defined(OS_WIN)
  HDC hdc = gc->platformContext()->canvas()->beginPlatformPaint();
  delegate_->Print(hdc);
  gc->platformContext()->canvas()->endPlatformPaint();
#else
  NOTIMPLEMENTED();
#endif
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
#if defined(OS_WIN)
  DCHECK(parent()->isFrameView());
  // We cache the parent FrameView here as the plugin widget could be deleted
  // in the call to HandleEvent. See http://b/issue?id=1362948
  WebCore::FrameView* parent_view = static_cast<WebCore::FrameView*>(parent());

  WebCore::IntPoint p =
      parent_view->contentsToWindow(WebCore::IntPoint(event->pageX(),
                                                      event->pageY()));
  NPEvent np_event;
  np_event.lParam = static_cast<uint32>(MAKELPARAM(p.x(), p.y()));
  np_event.wParam = 0;

  if (event->ctrlKey())
    np_event.wParam |= MK_CONTROL;
  if (event->shiftKey())
    np_event.wParam |= MK_SHIFT;

  if ((event->type() == WebCore::eventNames().mousemoveEvent) ||
      (event->type() == WebCore::eventNames().mouseoutEvent) ||
      (event->type() == WebCore::eventNames().mouseoverEvent)) {
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
  } else if (event->type() == WebCore::eventNames().mousedownEvent) {
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
  } else if (event->type() == WebCore::eventNames().mouseupEvent) {
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
  WebCursor cursor;
  if (!delegate_->HandleEvent(&np_event, &cursor))
    event->setDefaultHandled();

  ChromeClientImpl* chrome_client =
      static_cast<ChromeClientImpl*>(
          parent_view->frame()->page()->chrome()->client());

  // A windowless plugin can change the cursor in response to the WM_MOUSEMOVE
  // event. We need to reflect the changed cursor in the frame view as the
  // mouse is moved in the boundaries of the windowless plugin.
  chrome_client->SetCursorForPlugin(cursor);

#else
  NOTIMPLEMENTED();
#endif
}

void WebPluginImpl::handleKeyboardEvent(WebCore::KeyboardEvent* event) {
#if defined(OS_WIN)
  NPEvent np_event;
  np_event.wParam = event->keyCode();

  if (event->type() == WebCore::eventNames().keydownEvent) {
    np_event.event = WM_KEYDOWN;
    np_event.lParam = 0;
  } else if (event->type() == WebCore::eventNames().keyupEvent) {
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
#else
  NOTIMPLEMENTED();
#endif
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
  result.append(webkit_glue::StringToStdWString(status));
  result.append(L"\n");

  WebCore::HTTPHeaderMap::const_iterator it =
      response.httpHeaderFields().begin();
  for (; it != response.httpHeaderFields().end(); ++it) {
    if (!it->first.isEmpty() && !it->second.isEmpty()) {
      result.append(webkit_glue::StringToStdWString(it->first));
      result.append(L": ");
      result.append(webkit_glue::StringToStdWString(it->second));
      result.append(L"\n");
    }
  }

  return result;
}

void WebPluginImpl::didReceiveResponse(WebCore::ResourceHandle* handle,
    const WebCore::ResourceResponse& response) {
  static const int kHttpPartialResponseStatusCode = 206;
  static const int kHttpResponseSuccessStatusCode = 200;

  WebPluginResourceClient* client = GetClientFromHandle(handle);
  if (!client)
    return;

  WebPluginContainer::HttpResponseInfo http_response_info;
  WebPluginContainer::ReadHttpResponseInfo(response, &http_response_info);

  bool cancel = false;
  bool request_is_seekable = true;
  if (client->IsMultiByteResponseExpected()) {
    if (response.httpStatusCode() == kHttpPartialResponseStatusCode) {
      HandleHttpMultipartResponse(response, client);
      return;
    } else if (response.httpStatusCode() == kHttpResponseSuccessStatusCode) {
      // If the client issued a byte range request and the server responds with
      // HTTP 200 OK, it indicates that the server does not support byte range
      // requests.
      // We need to emulate Firefox behavior by doing the following:-
      // 1. Destroy the plugin instance in the plugin process. Ensure that
      //    existing resource requests initiated for the plugin instance
      //    continue to remain valid.
      // 2. Create a new plugin instance and notify it about the response
      //    received here.
      if (!ReinitializePluginForResponse(handle)) {
        NOTREACHED();
        return;
      }

      // The server does not support byte range requests. No point in creating
      // seekable streams.
      request_is_seekable = false;

      delete client;
      client = NULL;

      // Create a new resource client for this request.
      for (size_t i = 0; i < clients_.size(); ++i) {
        if (clients_[i].handle.get() == handle) {
          WebPluginResourceClient* resource_client =
              delegate_->CreateResourceClient(clients_[i].id, 
                                              plugin_url_.spec().c_str(),
                                              NULL, false, NULL);
          clients_[i].client = resource_client;
          client = resource_client;
          break;
        }
      }

      DCHECK(client != NULL);
    }
  }

  client->DidReceiveResponse(
      base::SysWideToNativeMB(http_response_info.mime_type),
      base::SysWideToNativeMB(GetAllHeaders(response)),
      http_response_info.expected_length,
      http_response_info.last_modified, request_is_seekable, &cancel);

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
  const WebCore::String& protocol_scheme = response.url().protocol();
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
  if (client) {
    MultipartResponseDelegate* multi_part_handler =
      multi_part_response_map_[client];
    if (multi_part_handler) {
      multi_part_handler->OnReceivedData(buffer, length);
    } else {
      client->DidReceiveData(buffer, length, 0);
    }
  }
}

void WebPluginImpl::didFinishLoading(WebCore::ResourceHandle* handle) {
  WebPluginResourceClient* client = GetClientFromHandle(handle);
  if (client) {
    MultiPartResponseHandlerMap::iterator index =
        multi_part_response_map_.find(client);
    if (index != multi_part_response_map_.end()) {
      delete (*index).second;
      multi_part_response_map_.erase(index);
    }
    client->DidFinishLoading();
  }

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
    TearDownPluginInstance(NULL);
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
                                    WebCore::IntRect* clip_rect,
                                    std::vector<gfx::Rect>* cutout_rects) {
  DCHECK(parent()->isFrameView());
  WebCore::FrameView* view = static_cast<WebCore::FrameView*>(parent());

  *window_rect =
      WebCore::IntRect(view->contentsToWindow(frame_rect.location()),
                                              frame_rect.size());
  // Calculate a clip-rect so that we don't overlap the scrollbars, etc.
  *clip_rect = windowClipRect();
  clip_rect->move(-window_rect->x(), -window_rect->y());

  cutout_rects->clear();
  WTF::Vector<WebCore::IntRect> rects;
  widget_->windowCutoutRects(frame_rect, &rects);
  // Convert to gfx::Rect and subtract out the plugin position.
  for (size_t i = 0; i < rects.size(); i++) {
    gfx::Rect r = webkit_glue::FromIntRect(rects[i]);
    r.Offset(-frame_rect.x(), -frame_rect.y());
    cutout_rects->push_back(r);
  }
}

void WebPluginImpl::HandleURLRequest(const char *method,
                                     bool is_javascript_url,
                                     const char* target, unsigned int len,
                                     const char* buf, bool is_file_data,
                                     bool notify, const char* url,
                                     void* notify_data, bool popups_allowed) {
  HandleURLRequestInternal(method, is_javascript_url, target, len, buf,
                           is_file_data, notify, url, notify_data,
                           popups_allowed, true);
}

void WebPluginImpl::HandleURLRequestInternal(
    const char *method, bool is_javascript_url, const char* target,
    unsigned int len, const char* buf, bool is_file_data, bool notify,
    const char* url, void* notify_data, bool popups_allowed,
    bool use_plugin_src_as_referrer) {
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
    WebCore::String script = WebCore::decodeURLEscapeSequences(
        WebCore::String(escaped_script.data(),
                                  static_cast<int>(escaped_script.length())));

    ExecuteScript(original_url,
                  webkit_glue::StringToStdWString(script), notify,
                  reinterpret_cast<int>(notify_data), popups_allowed);
  } else {
    std::string complete_url_string;
    CompleteURL(url, &complete_url_string);

    int resource_id = GetNextResourceId();
    WebPluginResourceClient* resource_client =
        delegate_->CreateResourceClient(resource_id, complete_url_string,
                                        notify, notify_data, NULL);

    // If the RouteToFrame call returned a failure then inform the result
    // back to the plugin asynchronously.
    if ((routing_status == INVALID_URL) ||
        (routing_status == GENERAL_FAILURE)) {
      resource_client->DidFail();
      return;
    }

    InitiateHTTPRequest(resource_id, resource_client, method, buf, len,
                        GURL(complete_url_string), NULL,
                        use_plugin_src_as_referrer);
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
                                        const GURL& url,
                                        const char* range_info,
                                        bool use_plugin_src_as_referrer) {
  if (!client) {
    NOTREACHED();
    return false;
  }

  WebCore::KURL kurl = webkit_glue::GURLToKURL(url);

  ClientInfo info;
  info.id = resource_id;
  info.client = client;
  info.request.setFrame(frame());
  info.request.setURL(kurl);
  info.request.setOriginPid(delegate_->GetProcessId());
  info.request.setTargetType(WebCore::ResourceRequest::TargetIsObject);
  info.request.setHTTPMethod(method);

  if (range_info)
    info.request.addHTTPHeaderField("Range", range_info);

  WebCore::String referrer;
  // GetURL/PostURL requests initiated explicitly by plugins should specify the
  // plugin SRC url as the referrer if it is available.
  if (use_plugin_src_as_referrer && !plugin_url_.spec().empty()) {
    referrer = webkit_glue::StdStringToString(plugin_url_.spec());
  } else { 
    referrer = frame()->loader()->outgoingReferrer();
  }

  if (!WebCore::FrameLoader::shouldHideReferrer(kurl, referrer))
    info.request.setHTTPReferrer(referrer);

  if (strcmp(method, "POST") == 0) {
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

void WebPluginImpl::CancelDocumentLoad() {
  if (frame()->loader()->activeDocumentLoader()) {
    widget_->set_ignore_response_error(true);
    frame()->loader()->activeDocumentLoader()->stopLoading();
  }
}

void WebPluginImpl::InitiateHTTPRangeRequest(const char* url,
                                             const char* range_info,
                                             HANDLE existing_stream,
                                             bool notify_needed,
                                             HANDLE notify_data) {
  int resource_id = GetNextResourceId();
  std::string complete_url_string;
  CompleteURL(url, &complete_url_string);

  WebPluginResourceClient* resource_client =
      delegate_->CreateResourceClient(resource_id, complete_url_string,
                                      notify_needed, notify_data,
                                      existing_stream);
  InitiateHTTPRequest(resource_id, resource_client, "GET", NULL, 0,
                      GURL(complete_url_string), range_info, true);
}

void WebPluginImpl::HandleHttpMultipartResponse(
    const WebCore::ResourceResponse& response,
    WebPluginResourceClient* client) {
  std::string multipart_boundary;
  if (!MultipartResponseDelegate::ReadMultipartBoundary(
          response, &multipart_boundary)) {
    NOTREACHED();
    return;
  }

  MultiPartResponseClient* multi_part_response_client =
      new MultiPartResponseClient(client);

  MultipartResponseDelegate* multi_part_response_handler =
      new MultipartResponseDelegate(multi_part_response_client, NULL,
                                    response,
                                    multipart_boundary);
  multi_part_response_map_[client] = multi_part_response_handler;
}

bool WebPluginImpl::ReinitializePluginForResponse(
    WebCore::ResourceHandle* response_handle) {
  WebFrameImpl* web_frame = WebFrameImpl::FromFrame(frame());
  if (!web_frame)
    return false;

  WebViewImpl* web_view = web_frame->webview_impl();
  if (!web_view)
    return false;

  WebPluginContainer* container_widget = widget_;

  // Destroy the current plugin instance.
  TearDownPluginInstance(response_handle);

  widget_ = container_widget;
  webframe_ = web_frame;
  // Turn off the load_manually flag as we are going to hand data off to the
  // plugin.
  load_manually_ = false;

  WebViewDelegate* webview_delegate = web_view->GetDelegate();
  std::string actual_mime_type;
  WebPluginDelegate* plugin_delegate =
      webview_delegate->CreatePluginDelegate(web_view, plugin_url_,
                                             mime_type_, std::string(),
                                             &actual_mime_type);

  char** arg_names = new char*[arg_names_.size()];
  char** arg_values = new char*[arg_values_.size()];

  for (unsigned int index = 0; index < arg_names_.size(); ++index) {
    arg_names[index] = const_cast<char*>(arg_names_[index].c_str());
    arg_values[index] = const_cast<char*>(arg_values_[index].c_str());
  }

  bool init_ok = plugin_delegate->Initialize(plugin_url_, arg_names,
                                             arg_values, arg_names_.size(),
                                             this, load_manually_);
  delete[] arg_names;
  delete[] arg_values;

  if (!init_ok) {
    SetContainer(NULL);
    // TODO(iyengar) Should we delete the current plugin instance here?
    return false;
  }

  mime_type_ = actual_mime_type;
  delegate_ = plugin_delegate;
  // Force a geometry update to occur to ensure that the plugin becomes
  // visible.
  widget_->frameRectsChanged();
  delegate_->FlushGeometryUpdates();  
  return true;
}

void WebPluginImpl::ArrayToVector(int total_values, char** values,
                                  std::vector<std::string>* value_vector) {
  DCHECK(value_vector != NULL);
  for (int index = 0; index < total_values; ++index) {
    value_vector->push_back(values[index]);
  }
}

void WebPluginImpl::TearDownPluginInstance(
    WebCore::ResourceHandle* response_handle_to_ignore) {
  // The frame maintains a list of JSObjects which are related to this
  // plugin.  Tell the frame we're gone so that it can invalidate all
  // of those sub JSObjects.
  if (frame()) {
    ASSERT(widget_ != NULL);
    frame()->script()->cleanupScriptObjectsForPlugin(widget_);
  }

  // Call PluginDestroyed() first to prevent the plugin from calling us back
  // in the middle of tearing down the render tree.
  delegate_->PluginDestroyed();
  delegate_ = NULL;

  // Cancel any pending requests because otherwise this deleted object will
  // be called by the ResourceDispatcher.
  std::vector<ClientInfo>::iterator client_index = clients_.begin();
  while (client_index != clients_.end()) {
    ClientInfo& client_info = *client_index;

    if (response_handle_to_ignore == client_info.handle) {
      client_index++;
      continue;
    }

    if (client_info.handle)
      client_info.handle->cancel();

    WebPluginResourceClient* resource_client = client_info.client;
    client_index = clients_.erase(client_index);
    if (resource_client)
      resource_client->DidFail();
  }

  // This needs to be called now and not in the destructor since the
  // webframe_ might not be valid anymore.
  webframe_->set_plugin_delegate(NULL);
  webframe_ = NULL;
}
