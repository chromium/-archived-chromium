// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventNames.h"
#include "FloatPoint.h"
#include "FormData.h"
#include "FormState.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "IntRect.h"
#include "KURL.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformContextSkia.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "PlatformWidget.h"
#include "RenderBox.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "ScriptController.h"
#include "ScriptValue.h"
#include "ScrollView.h"
#include "Widget.h"

#undef LOG
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "net/base/escape.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebHTTPBody.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebKitClient.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLLoader.h"
#include "webkit/api/public/WebURLLoaderClient.h"
#include "webkit/api/public/WebURLResponse.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/stacking_order_iterator.h"
#include "webkit/glue/webplugin_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "googleurl/src/gurl.h"

using WebKit::WebCursorInfo;
using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderClient;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using webkit_glue::MultipartResponseDelegate;

// This class handles individual multipart responses. It is instantiated when
// we receive HTTP status code 206 in the HTTP response. This indicates
// that the response could have multiple parts each separated by a boundary
// specified in the response header.
class MultiPartResponseClient : public WebURLLoaderClient {
 public:
  MultiPartResponseClient(WebPluginResourceClient* resource_client)
      : resource_client_(resource_client) {
    Clear();
  }

  virtual void willSendRequest(
      WebURLLoader*, WebURLRequest&, const WebURLResponse&) {}
  virtual void didSendData(
      WebURLLoader*, unsigned long long, unsigned long long) {}

  // Called when the multipart parser encounters an embedded multipart
  // response.
  virtual void didReceiveResponse(
      WebURLLoader*, const WebURLResponse& response) {
    if (!MultipartResponseDelegate::ReadContentRanges(
            response,
            &byte_range_lower_bound_,
            &byte_range_upper_bound_)) {
      NOTREACHED();
      return;
    }

    resource_response_ = response;
  }

  // Receives individual part data from a multipart response.
  virtual void didReceiveData(
      WebURLLoader*, const char* data, int data_size, long long) {
    resource_client_->DidReceiveData(
        data, data_size, byte_range_lower_bound_);
  }

  virtual void didFinishLoading(WebURLLoader*) {}
  virtual void didFail(WebURLLoader*, const WebURLError&) {}

  void Clear() {
    resource_response_.reset();
    byte_range_lower_bound_ = 0;
    byte_range_upper_bound_ = 0;
  }

 private:
  WebURLResponse resource_response_;
  // The lower bound of the byte range.
  int byte_range_lower_bound_;
  // The upper bound of the byte range.
  int byte_range_upper_bound_;
  // The handler for the data.
  WebPluginResourceClient* resource_client_;
};

static std::wstring GetAllHeaders(const WebCore::ResourceResponse& response) {
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

    // Get our clip rect and intersect with it to ensure we don't
    // invalidate too much.
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
  setSelfVisible(true);
  impl_->UpdateVisibility();

  WebCore::Widget::show();
}

void WebPluginContainer::hide() {
  setSelfVisible(false);
  impl_->UpdateVisibility();

  WebCore::Widget::hide();
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

// We override this function, to make sure that geometry updates are sent
// over to the plugin. For e.g. when a plugin is instantiated it does
// not have a valid parent. As a result the first geometry update from
// webkit is ignored. This function is called when the plugin eventually
// gets a parent.
void WebPluginContainer::setParentVisible(bool visible) {
  if (isParentVisible() == visible)
    return;  // No change.

  WebCore::Widget::setParentVisible(visible);
  if (!isSelfVisible())
    return;  // This widget has explicitely been marked as not visible.

  impl_->UpdateVisibility();
}

// We override this function so that if the plugin is windowed, we can call
// NPP_SetWindow at the first possible moment.  This ensures that NPP_SetWindow
// is called before the manual load data is sent to a plugin.  If this order is
// reversed, Flash won't load videos.
void WebPluginContainer::setParent(WebCore::ScrollView* view) {
  WebCore::Widget::setParent(view);
  if (view) {
    impl_->setFrameRect(frameRect());
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

  // Manual loading, so make sure that the plugin receives window geometry
  // before data, or else plugins misbehave.
  frameRectsChanged();

  HttpResponseInfo http_response_info;
  ReadHttpResponseInfo(response, &http_response_info);

  impl_->delegate_->DidReceiveManualResponse(
      http_response_info.url,
      base::SysWideToNativeMB(http_response_info.mime_type),
      base::SysWideToNativeMB(GetAllHeaders(response)),
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
                                       WebCore::HTMLPlugInElement* element,
                                       WebFrameImpl* frame,
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

WebPluginImpl::WebPluginImpl(WebCore::HTMLPlugInElement* element,
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
      widget_(NULL),
      plugin_url_(plugin_url),
      load_manually_(load_manually),
      first_geometry_update_(true),
      mime_type_(mime_type),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {

  ArrayToVector(arg_count, arg_names, &arg_names_);
  ArrayToVector(arg_count, arg_values, &arg_values_);
}

WebPluginImpl::~WebPluginImpl() {
}

#if defined(OS_LINUX)
gfx::PluginWindowHandle WebPluginImpl::CreatePluginContainer() {
  WebCore::Frame* frame = element_->document()->frame();
  WebFrameImpl* webframe = WebFrameImpl::FromFrame(frame);
  WebViewImpl* webview = webframe->GetWebViewImpl();
  if (!webview->delegate())
    return 0;
  return webview->delegate()->CreatePluginContainer();
}
#endif

void WebPluginImpl::SetWindow(gfx::PluginWindowHandle window) {
  if (window) {
    DCHECK(!windowless_);  // Make sure not called twice.
    window_ = window;
  } else {
    DCHECK(!window_);  // Make sure not called twice.
    windowless_ = true;
  }
}

void WebPluginImpl::WillDestroyWindow(gfx::PluginWindowHandle window) {
  WebCore::Frame* frame = element_->document()->frame();
  WebFrameImpl* webframe = WebFrameImpl::FromFrame(frame);
  WebViewImpl* webview = webframe->GetWebViewImpl();
  if (!webview->delegate())
    return;
  webview->delegate()->WillDestroyPluginWindow(window);
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
                                  intptr_t notify_data,
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
      if (clients_[i].loader.get()) {
        clients_[i].loader->cancel();
        RemoveClient(i);
      }
      return;
    }
  }
}

bool WebPluginImpl::SetPostData(WebURLRequest* request,
                                const char *buf,
                                uint32 length) {
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<char> body;
  bool rv = NPAPI::PluginHost::SetPostData(buf, length, &names, &values, &body);

  for (size_t i = 0; i < names.size(); ++i) {
    request->addHTTPHeaderField(webkit_glue::StdStringToWebString(names[i]),
                                webkit_glue::StdStringToWebString(values[i]));
  }

  WebString content_type_header = WebString::fromUTF8("Content-Type");
  const WebString& content_type =
      request->httpHeaderField(content_type_header);
  if (content_type.isEmpty()) {
    request->setHTTPHeaderField(
        content_type_header,
        WebString::fromUTF8("application/x-www-form-urlencoded"));
  }

  WebHTTPBody http_body;
  if (body.size()) {
    http_body.initialize();
    http_body.appendData(WebData(&body[0], body.size()));
  }
  request->setHTTPBody(http_body);

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
  WebURLRequest request(webkit_glue::KURLToWebURL(complete_url_kurl));
  request.setHTTPMethod(WebString::fromUTF8(method));
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
  WebCore::FrameLoadRequest load_request(
      *webkit_glue::WebURLRequestToResourceRequest(&request));
  load_request.setFrameName(str_target);
  WebCore::FrameLoader *loader = frame()->loader();
  // we actually don't know whether usergesture is true or false,
  // passing true since all we can do is assume it is okay.
  loader->loadFrameRequest(
      load_request,
      false,  // lock history
      false,  // lock back forward list
      0,      // event
      0);     // form state

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
  return element_->getNPObject();
}

void WebPluginImpl::SetCookie(const GURL& url,
                              const GURL& policy_url,
                              const std::string& cookie) {
  WebKit::webKitClient()->setCookies(url, policy_url, UTF8ToUTF16(cookie));
}

std::string WebPluginImpl::GetCookies(const GURL& url, const GURL& policy_url) {
  return UTF16ToUTF8(WebKit::webKitClient()->cookies(url, policy_url));
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
          WebCore::RenderBox* rbox = WebCore::toRenderBox(ro);
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
  WebViewImpl* webview = webframe->GetWebViewImpl();
  // It is valid for this function to be invoked in code paths where the
  // the webview is closed.
  if (!webview->delegate()) {
    return;
  }

  WebCore::IntRect window_rect;
  WebCore::IntRect clip_rect;
  std::vector<gfx::Rect> cutout_rects;
  CalculateBounds(rect, &window_rect, &clip_rect, &cutout_rects);

  if (window_) {
    // Notify the window hosting the plugin (the WebViewDelegate) that
    // it needs to adjust the plugin, so that all the HWNDs can be moved
    // at the same time.
    WebPluginGeometry move;
    move.window = window_;
    move.window_rect = webkit_glue::FromIntRect(window_rect);
    move.clip_rect = webkit_glue::FromIntRect(clip_rect);
    move.cutout_rects = cutout_rects;
    move.rects_valid = true;
    move.visible = widget_->isVisible();

    webview->delegate()->DidMove(webview, move);
  }

  // Notify the plugin that its parameters have changed.
  delegate_->UpdateGeometry(webkit_glue::FromIntRect(window_rect),
                            webkit_glue::FromIntRect(clip_rect));

  // Initiate a download on the plugin url. This should be done for the
  // first update geometry sequence. We need to ensure that the plugin
  // receives the geometry update before it starts receiving data.
  if (first_geometry_update_) {
    first_geometry_update_ = false;
    // An empty url corresponds to an EMBED tag with no src attribute.
    if (!load_manually_ && plugin_url_.is_valid()) {
      // The Flash plugin hangs for a while if it receives data before
      // receiving valid plugin geometry. By valid geometry we mean the
      // geometry received by a call to setFrameRect in the Webkit
      // layout code path. To workaround this issue we download the
      // plugin source url on a timer.
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          method_factory_.NewRunnableMethod(
              &WebPluginImpl::OnDownloadPluginSrcUrl),
          0);
    }
  }
}

void WebPluginImpl::OnDownloadPluginSrcUrl() {
  HandleURLRequestInternal("GET", false, NULL, 0, NULL, false, false,
                           plugin_url_.spec().c_str(), NULL, false,
                           false);
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

#if defined(OS_WIN) || defined(OS_LINUX)
  // Note that |context| is only used when in windowless mode.
  gfx::NativeDrawingContext context =
      gc->platformContext()->canvas()->beginPlatformPaint();
#elif defined(OS_MACOSX)
  gfx::NativeDrawingContext context = gc->platformContext();
#endif

  WebCore::IntRect window_rect =
      WebCore::IntRect(view->contentsToWindow(damage_rect.location()),
                       damage_rect.size());

  delegate_->Paint(context, webkit_glue::FromIntRect(window_rect));

#if defined(OS_WIN) || defined(OS_LINUX)
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
  gfx::NativeDrawingContext hdc =
       gc->platformContext()->canvas()->beginPlatformPaint();
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
  // We cache the parent FrameView here as the plugin widget could be deleted
  // in the call to HandleEvent. See http://b/issue?id=1362948
  WebCore::FrameView* parent_view = static_cast<WebCore::FrameView*>(parent());

  WebMouseEvent web_event;
  if (!ToWebMouseEvent(*parent_view, *event, &web_event))
    return;

  if (event->type() == WebCore::eventNames().mousedownEvent) {
    // Ensure that the frame containing the plugin has focus.
    WebCore::Frame* containing_frame = webframe_->frame();
    if (WebCore::Page* current_page = containing_frame->page()) {
      current_page->focusController()->setFocusedFrame(containing_frame);
    }
    // Give focus to our containing HTMLPluginElement.
    containing_frame->document()->setFocusedNode(element_);
  }

  // TODO(pkasting): http://b/1119691 This conditional seems exactly backwards,
  // but it matches Safari's code, and if I reverse it, giving focus to a
  // transparent (windowless) plugin fails.
  WebCursorInfo cursor_info;
  if (!delegate_->HandleInputEvent(web_event, &cursor_info))
    event->setDefaultHandled();

  WebCore::Page* page = parent_view->frame()->page();
  if (!page)
    return;

  ChromeClientImpl* chrome_client =
      static_cast<ChromeClientImpl*>(page->chrome()->client());

  // A windowless plugin can change the cursor in response to the WM_MOUSEMOVE
  // event. We need to reflect the changed cursor in the frame view as the
  // mouse is moved in the boundaries of the windowless plugin.
  chrome_client->SetCursorForPlugin(cursor_info);
}

void WebPluginImpl::handleKeyboardEvent(WebCore::KeyboardEvent* event) {
  WebKeyboardEvent web_event;
  if (!ToWebKeyboardEvent(*event, &web_event))
    return;
  // TODO(pkasting): http://b/1119691 See above.
  WebCursorInfo cursor_info;
  if (!delegate_->HandleInputEvent(web_event, &cursor_info))
    event->setDefaultHandled();
}

NPObject* WebPluginImpl::GetPluginScriptableObject() {
  return delegate_->GetPluginScriptableObject();
}

WebPluginResourceClient* WebPluginImpl::GetClientFromLoader(
    WebURLLoader* loader) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].loader.get() == loader)
      return clients_[i].client;
  }

  NOTREACHED();
  return 0;
}


void WebPluginImpl::willSendRequest(WebURLLoader* loader,
                                    WebURLRequest& request,
                                    const WebURLResponse&) {
  WebPluginResourceClient* client = GetClientFromLoader(loader);
  if (client)
    client->WillSendRequest(request.url());
}

void WebPluginImpl::didSendData(WebURLLoader* loader,
                                unsigned long long bytes_sent,
                                unsigned long long total_bytes_to_be_sent) {
}

void WebPluginImpl::didReceiveResponse(WebURLLoader* loader,
                                       const WebURLResponse& response) {
  static const int kHttpPartialResponseStatusCode = 206;
  static const int kHttpResponseSuccessStatusCode = 200;

  WebPluginResourceClient* client = GetClientFromLoader(loader);
  if (!client)
    return;

  const WebCore::ResourceResponse& resource_response =
      *webkit_glue::WebURLResponseToResourceResponse(&response);

  WebPluginContainer::HttpResponseInfo http_response_info;
  WebPluginContainer::ReadHttpResponseInfo(resource_response,
                                           &http_response_info);

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
      if (!ReinitializePluginForResponse(loader)) {
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
        if (clients_[i].loader.get() == loader) {
          WebPluginResourceClient* resource_client =
              delegate_->CreateResourceClient(clients_[i].id,
                                              plugin_url_.spec().c_str(),
                                              false, 0, NULL);
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
      base::SysWideToNativeMB(GetAllHeaders(resource_response)),
      http_response_info.expected_length,
      http_response_info.last_modified, request_is_seekable, &cancel);

  if (cancel) {
    loader->cancel();
    RemoveClient(loader);
    return;
  }

  // Bug http://b/issue?id=925559. The flash plugin would not handle the HTTP
  // error codes in the stream header and as a result, was unaware of the
  // fate of the HTTP requests issued via NPN_GetURLNotify. Webkit and FF
  // destroy the stream and invoke the NPP_DestroyStream function on the
  // plugin if the HTTP request fails.
  const GURL& url = response.url();
  if (url.SchemeIs("http") || url.SchemeIs("https")) {
    if (response.httpStatusCode() < 100 || response.httpStatusCode() >= 400) {
      // The plugin instance could be in the process of deletion here.
      // Verify if the WebPluginResourceClient instance still exists before
      // use.
      WebPluginResourceClient* resource_client = GetClientFromLoader(loader);
      if (resource_client) {
        loader->cancel();
        resource_client->DidFail();
        RemoveClient(loader);
      }
    }
  }
}

void WebPluginImpl::didReceiveData(WebURLLoader* loader,
                                   const char *buffer,
                                   int length, long long) {
  WebPluginResourceClient* client = GetClientFromLoader(loader);
  if (!client)
    return;
  MultiPartResponseHandlerMap::iterator index =
      multi_part_response_map_.find(client);
  if (index != multi_part_response_map_.end()) {
    MultipartResponseDelegate* multi_part_handler = (*index).second;
    DCHECK(multi_part_handler != NULL);
    multi_part_handler->OnReceivedData(buffer, length);
  } else {
    client->DidReceiveData(buffer, length, 0);
  }
}

void WebPluginImpl::didFinishLoading(WebURLLoader* loader) {
  WebPluginResourceClient* client = GetClientFromLoader(loader);
  if (client) {
    MultiPartResponseHandlerMap::iterator index =
        multi_part_response_map_.find(client);
    if (index != multi_part_response_map_.end()) {
      delete (*index).second;
      multi_part_response_map_.erase(index);

      WebView* web_view = webframe_->GetView();
      web_view->GetDelegate()->DidStopLoading(web_view);
    }
    client->DidFinishLoading();
  }

  RemoveClient(loader);
}

void WebPluginImpl::didFail(WebURLLoader* loader,
                            const WebURLError&) {
  WebPluginResourceClient* client = GetClientFromLoader(loader);
  if (client)
    client->DidFail();

  RemoveClient(loader);
}

void WebPluginImpl::RemoveClient(size_t i) {
  clients_.erase(clients_.begin() + i);
}

void WebPluginImpl::RemoveClient(WebURLLoader* loader) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].loader.get() == loader) {
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
                                     intptr_t notify_data, bool popups_allowed) {
  HandleURLRequestInternal(method, is_javascript_url, target, len, buf,
                           is_file_data, notify, url, notify_data,
                           popups_allowed, true);
}

void WebPluginImpl::HandleURLRequestInternal(
    const char *method, bool is_javascript_url, const char* target,
    unsigned int len, const char* buf, bool is_file_data, bool notify,
    const char* url, intptr_t notify_data, bool popups_allowed,
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

    ExecuteScript(original_url, webkit_glue::StringToStdWString(script), notify,
                  notify_data, popups_allowed);
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

  ClientInfo info;
  info.id = resource_id;
  info.client = client;
  info.request.initialize();
  info.request.setURL(url);
  info.request.setRequestorProcessID(delegate_->GetProcessId());
  info.request.setTargetType(WebURLRequest::TargetIsObject);
  info.request.setHTTPMethod(WebString::fromUTF8(method));

  if (range_info) {
    info.request.addHTTPHeaderField(WebString::fromUTF8("Range"),
                                    WebString::fromUTF8(range_info));
  }

  WebCore::String referrer;
  // GetURL/PostURL requests initiated explicitly by plugins should specify the
  // plugin SRC url as the referrer if it is available.
  if (use_plugin_src_as_referrer && !plugin_url_.spec().empty()) {
    referrer = webkit_glue::StdStringToString(plugin_url_.spec());
  } else {
    referrer = frame()->loader()->outgoingReferrer();
  }

  if (!WebCore::FrameLoader::shouldHideReferrer(webkit_glue::GURLToKURL(url),
                                                referrer)) {
    info.request.setHTTPHeaderField(WebString::fromUTF8("Referer"),
                                    webkit_glue::StringToWebString(referrer));
  }

  if (strcmp(method, "POST") == 0) {
    // Adds headers or form data to a request.  This must be called before
    // we initiate the actual request.
    SetPostData(&info.request, buf, buf_len);
  }

  // Sets the routing id to associate the ResourceRequest with the RenderView.
  WebCore::ResourceResponse response;
  frame()->loader()->client()->dispatchWillSendRequest(
      NULL,
      0,
      *webkit_glue::WebURLRequestToMutableResourceRequest(&info.request),
      response);

  info.loader.reset(WebKit::webKitClient()->createURLLoader());
  if (!info.loader.get())
    return false;
  info.loader->loadAsynchronously(info.request, this);

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
                                             intptr_t existing_stream,
                                             bool notify_needed,
                                             intptr_t notify_data) {
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
    const WebURLResponse& response,
    WebPluginResourceClient* client) {
  std::string multipart_boundary;
  if (!MultipartResponseDelegate::ReadMultipartBoundary(
          response, &multipart_boundary)) {
    NOTREACHED();
    return;
  }

  WebView* web_view = webframe_->GetView();
  web_view->GetDelegate()->DidStartLoading(web_view);

  MultiPartResponseClient* multi_part_response_client =
      new MultiPartResponseClient(client);

  MultipartResponseDelegate* multi_part_response_handler =
      new MultipartResponseDelegate(multi_part_response_client, NULL,
                                    response,
                                    multipart_boundary);
  multi_part_response_map_[client] = multi_part_response_handler;
}

bool WebPluginImpl::ReinitializePluginForResponse(
    WebURLLoader* loader) {
  WebFrameImpl* web_frame = WebFrameImpl::FromFrame(frame());
  if (!web_frame)
    return false;

  WebViewImpl* web_view = web_frame->GetWebViewImpl();
  if (!web_view)
    return false;

  WebPluginContainer* container_widget = widget_;

  // Destroy the current plugin instance.
  TearDownPluginInstance(loader);

  widget_ = container_widget;
  webframe_ = web_frame;

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
    widget_ = NULL;
    // TODO(iyengar) Should we delete the current plugin instance here?
    return false;
  }

  mime_type_ = actual_mime_type;
  delegate_ = plugin_delegate;
  // Force a geometry update to occur to ensure that the plugin becomes
  // visible.
  widget_->frameRectsChanged();
  // The plugin move sequences accumulated via DidMove are sent to the browser
  // whenever the renderer paints. Force a paint here to ensure that changes
  // to the plugin window are propagated to the browser.
  widget_->invalidateRect(widget_->frameRect());
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
    WebURLLoader* loader_to_ignore) {
  // The frame maintains a list of JSObjects which are related to this
  // plugin.  Tell the frame we're gone so that it can invalidate all
  // of those sub JSObjects.
  if (frame()) {
    ASSERT(widget_ != NULL);
    frame()->script()->cleanupScriptObjectsForPlugin(widget_);
  }

  if (delegate_) {
    // Call PluginDestroyed() first to prevent the plugin from calling us back
    // in the middle of tearing down the render tree.
    delegate_->PluginDestroyed();
    delegate_ = NULL;
  }

  // Cancel any pending requests because otherwise this deleted object will
  // be called by the ResourceDispatcher.
  std::vector<ClientInfo>::iterator client_index = clients_.begin();
  while (client_index != clients_.end()) {
    ClientInfo& client_info = *client_index;

    if (loader_to_ignore == client_info.loader) {
      client_index++;
      continue;
    }

    if (client_info.loader.get())
      client_info.loader->cancel();

    WebPluginResourceClient* resource_client = client_info.client;
    client_index = clients_.erase(client_index);
    if (resource_client)
      resource_client->DidFail();
  }

  // This needs to be called now and not in the destructor since the
  // webframe_ might not be valid anymore.
  webframe_->set_plugin_delegate(NULL);
  webframe_ = NULL;
  method_factory_.RevokeAll();
}

void WebPluginImpl::UpdateVisibility() {
  if (!window_)
    return;

  WebCore::Frame* frame = element_->document()->frame();
  WebFrameImpl* webframe = WebFrameImpl::FromFrame(frame);
  WebViewImpl* webview = webframe->GetWebViewImpl();
  if (!webview->delegate())
    return;

  WebPluginGeometry move;
  move.window = window_;
  move.window_rect = gfx::Rect();
  move.clip_rect = gfx::Rect();
  move.rects_valid = false;
  move.visible = widget_->isVisible();

  webview->delegate()->DidMove(webview, move);
}
