// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webplugin_delegate_proxy.h"

#include <atlbase.h>

#include "generated_resources.h"

#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/gfx/size.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/emf.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/npobject_stub.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webview.h"

// Proxy for WebPluginResourceClient.  The object owns itself after creation,
// deleting itself after its callback has been called.
class ResourceClientProxy : public WebPluginResourceClient {
 public:
  ResourceClientProxy(PluginChannelHost* channel, int instance_id)
    : channel_(channel), instance_id_(instance_id), resource_id_(0),
      notify_needed_(false), notify_data_(NULL),
      multibyte_response_expected_(false) {
  }

  ~ResourceClientProxy() {
  }

  void Initialize(int resource_id, const std::string &url, bool notify_needed,
                  void *notify_data, void* existing_stream) {
    resource_id_ = resource_id;
    url_ = url;
    notify_needed_ = notify_needed;
    notify_data_ = notify_data;

    PluginMsg_URLRequestReply_Params params;
    params.resource_id = resource_id;
    params.url = url_;
    params.notify_needed = notify_needed_;
    params.notify_data = notify_data_;
    params.stream = existing_stream;

    multibyte_response_expected_ = (existing_stream != NULL);

    channel_->Send(new PluginMsg_HandleURLRequestReply(instance_id_, params));
  }

  // PluginResourceClient implementation:
  void WillSendRequest(const GURL& url) {
    DCHECK(channel_ != NULL);
    channel_->Send(new PluginMsg_WillSendRequest(instance_id_, resource_id_,
                                                 url));
  }

  void DidReceiveResponse(const std::string& mime_type,
                          const std::string& headers,
                          uint32 expected_length,
                          uint32 last_modified,
                          bool request_is_seekable,
                          bool* cancel) {
    DCHECK(channel_ != NULL);
    PluginMsg_DidReceiveResponseParams params;
    params.id = resource_id_;
    params.mime_type = mime_type;
    params.headers = headers;
    params.expected_length = expected_length;
    params.last_modified = last_modified;
    params.request_is_seekable = request_is_seekable;
    // Grab a reference on the underlying channel so it does not get
    // deleted from under us.
    scoped_refptr<PluginChannelHost> channel_ref(channel_);
    channel_->Send(new PluginMsg_DidReceiveResponse(instance_id_, params,
                                                    cancel));
  }

  void DidReceiveData(const char* buffer, int length, int data_offset) {
    DCHECK(channel_ != NULL);
    DCHECK(length > 0);
    std::vector<char> data;
    data.resize(static_cast<size_t>(length));
    memcpy(&data.front(), buffer, length);
    // Grab a reference on the underlying channel so it does not get
    // deleted from under us.
    scoped_refptr<PluginChannelHost> channel_ref(channel_);
    channel_->Send(new PluginMsg_DidReceiveData(instance_id_, resource_id_,
                                                data, data_offset));
  }

  void DidFinishLoading() {
    DCHECK(channel_ != NULL);
    channel_->Send(new PluginMsg_DidFinishLoading(instance_id_, resource_id_));
    channel_ = NULL;
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  void DidFail() {
    DCHECK(channel_ != NULL);
    channel_->Send(new PluginMsg_DidFail(instance_id_, resource_id_));
    channel_ = NULL;
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  bool IsMultiByteResponseExpected() {
    return multibyte_response_expected_;
  }

private:
  int resource_id_;
  int instance_id_;
  scoped_refptr<PluginChannelHost> channel_;
  std::string url_;
  bool notify_needed_;
  void* notify_data_;
  // Set to true if the response expected is a multibyte response.
  // For e.g. response for a HTTP byte range request.
  bool multibyte_response_expected_;
};

WebPluginDelegateProxy* WebPluginDelegateProxy::Create(
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    RenderView* render_view) {
  return new WebPluginDelegateProxy(mime_type, clsid, render_view);
}

WebPluginDelegateProxy::WebPluginDelegateProxy(const std::string& mime_type,
                                               const std::string& clsid,
                                               RenderView* render_view)
    : render_view_(render_view),
      mime_type_(mime_type),
      clsid_(clsid),
      plugin_(NULL),
      windowless_(false),
      npobject_(NULL),
      send_deferred_update_geometry_(false),
      visible_(false),
      sad_plugin_(NULL),
      window_script_object_(NULL),
      transparent_(false),
      invalidate_pending_(false) {
}

WebPluginDelegateProxy::~WebPluginDelegateProxy() {
}

void WebPluginDelegateProxy::PluginDestroyed() {
  plugin_ = NULL;

  if (npobject_) {
    // When we destroy the plugin instance, the NPObjectStub NULLs out its
    // pointer to the npobject (see NPObjectStub::OnChannelError).  Therefore,
    // we release the object before destroying the instance to avoid leaking.
    NPN_ReleaseObject(npobject_);
    npobject_ = NULL;
  }

  if (window_script_object_) {
    // The ScriptController deallocates this object independent of its ref count
    // to avoid leaks if the plugin forgets to release it.  So mark the object
    // invalid to avoid accessing it past this point.
    window_script_object_->set_proxy(NULL);
    window_script_object_->set_invalid();
  }

  if (channel_host_) {
    channel_host_->RemoveRoute(instance_id_);
    Send(new PluginMsg_DestroyInstance(instance_id_));
  }

  render_view_->PluginDestroyed(this);
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebPluginDelegateProxy::FlushGeometryUpdates() {
  if (send_deferred_update_geometry_) {
    send_deferred_update_geometry_ = false;
    Send(new PluginMsg_UpdateGeometry(instance_id_,
                                      plugin_rect_,
                                      deferred_clip_rect_,
                                      deferred_cutout_rects_,
                                      visible_,
                                      NULL,
                                      NULL));
  }
}

bool WebPluginDelegateProxy::Initialize(const GURL& url, char** argn,
                                        char** argv, int argc,
                                        WebPlugin* plugin,
                                        bool load_manually) {
  std::wstring channel_name;
  FilePath plugin_path;
  if (!g_render_thread->Send(new ViewHostMsg_OpenChannelToPlugin(
          url, mime_type_, clsid_, webkit_glue::GetWebKitLocale(),
          &channel_name, &plugin_path)))
    return false;

  MessageLoop* ipc_message_loop = g_render_thread->owner_loop();
  scoped_refptr<PluginChannelHost> channel_host =
      PluginChannelHost::GetPluginChannelHost(channel_name, ipc_message_loop);
  if (!channel_host.get())
    return false;

  int instance_id;
  bool result = channel_host->Send(new PluginMsg_CreateInstance(
      mime_type_, &instance_id));
  if (!result)
    return false;

  plugin_path_ = plugin_path;
  channel_host_ = channel_host;
  instance_id_ = instance_id;

  channel_host_->AddRoute(instance_id_, this, false);

  // Now tell the PluginInstance in the plugin process to initialize.
  PluginMsg_Init_Params params;
  params.containing_window = render_view_->host_window();
  params.url = url;
  for (int i = 0; i < argc; ++i) {
    params.arg_names.push_back(argn[i]);
    params.arg_values.push_back(argv[i]);

    if (LowerCaseEqualsASCII(params.arg_names.back(), "wmode") &&
        LowerCaseEqualsASCII(params.arg_values.back(), "transparent")) {
      transparent_ = true;
    }
  }
  params.load_manually = load_manually;
  params.modal_dialog_event = render_view_->modal_dialog_event();

  plugin_ = plugin;

  result = false;
  IPC::Message* msg = new PluginMsg_Init(instance_id_, params, &result);
  Send(msg);

  return result;
}

bool WebPluginDelegateProxy::Send(IPC::Message* msg) {
  if (!channel_host_) {
    DLOG(WARNING) << "dropping message because channel host is null";
    delete msg;
    return false;
  }

  return channel_host_->Send(msg);
}

void WebPluginDelegateProxy::SendJavaScriptStream(const std::string& url,
                                                  const std::wstring& result,
                                                  bool success,
                                                  bool notify_needed,
                                                  int notify_data) {
  PluginMsg_SendJavaScriptStream* msg =
      new PluginMsg_SendJavaScriptStream(instance_id_, url, result,
                                         success, notify_needed,
                                         notify_data);
  Send(msg);
}

void WebPluginDelegateProxy::DidReceiveManualResponse(
    const std::string& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length,
    uint32 last_modified) {
  PluginMsg_DidReceiveResponseParams params;
  params.id = 0;
  params.mime_type = mime_type;
  params.headers = headers;
  params.expected_length = expected_length;
  params.last_modified = last_modified;
  Send(new PluginMsg_DidReceiveManualResponse(instance_id_, url, params));
}

void WebPluginDelegateProxy::DidReceiveManualData(const char* buffer,
                                                  int length) {
  DCHECK(length > 0);
  std::vector<char> data;
  data.resize(static_cast<size_t>(length));
  memcpy(&data.front(), buffer, length);
  Send(new PluginMsg_DidReceiveManualData(instance_id_, data));
}

void WebPluginDelegateProxy::DidFinishManualLoading() {
  Send(new PluginMsg_DidFinishManualLoading(instance_id_));
}

void WebPluginDelegateProxy::DidManualLoadFail() {
  Send(new PluginMsg_DidManualLoadFail(instance_id_));
}

FilePath WebPluginDelegateProxy::GetPluginPath() {
  return plugin_path_;
}

void WebPluginDelegateProxy::InstallMissingPlugin() {
  Send(new PluginMsg_InstallMissingPlugin(instance_id_));
}

void WebPluginDelegateProxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(WebPluginDelegateProxy, msg)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetWindow, OnSetWindow)
    IPC_MESSAGE_HANDLER(PluginHostMsg_CancelResource, OnCancelResource)
    IPC_MESSAGE_HANDLER(PluginHostMsg_InvalidateRect, OnInvalidateRect)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetWindowScriptNPObject,
                        OnGetWindowScriptNPObject)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetPluginElement,
                        OnGetPluginElement)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER(PluginHostMsg_ShowModalHTMLDialog,
                        OnShowModalHTMLDialog)
    IPC_MESSAGE_HANDLER(PluginHostMsg_MissingPluginStatus,
                        OnMissingPluginStatus)
    IPC_MESSAGE_HANDLER(PluginHostMsg_URLRequest, OnHandleURLRequest)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetCPBrowsingContext,
                        OnGetCPBrowsingContext)
    IPC_MESSAGE_HANDLER(PluginHostMsg_CancelDocumentLoad, OnCancelDocumentLoad)
    IPC_MESSAGE_HANDLER(PluginHostMsg_InitiateHTTPRangeRequest,
                        OnInitiateHTTPRangeRequest)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void WebPluginDelegateProxy::OnChannelError() {
  if (plugin_)
    plugin_->Invalidate();
  render_view_->PluginCrashed(plugin_path_);
}

void WebPluginDelegateProxy::UpdateGeometry(
                const gfx::Rect& window_rect,
                const gfx::Rect& clip_rect,
                const std::vector<gfx::Rect>& cutout_rects,
                bool visible) {
  plugin_rect_ = window_rect;
  if (!windowless_) {
    deferred_clip_rect_ = clip_rect;
    deferred_cutout_rects_ = cutout_rects;
    visible_ = visible;
    send_deferred_update_geometry_ = true;
    return;
  }

  HANDLE transport_store_handle = NULL;
  HANDLE background_store_handle = NULL;
  if (!backing_store_canvas_.get() ||
      (window_rect.width() != backing_store_canvas_->getDevice()->width() ||
       window_rect.height() != backing_store_canvas_->getDevice()->height())) {
    // Create a shared memory section that the plugin paints into
    // asynchronously.
    ResetWindowlessBitmaps();
    if (!window_rect.IsEmpty()) {
      if (!CreateBitmap(&backing_store_, &backing_store_canvas_) ||
          !CreateBitmap(&transport_store_, &transport_store_canvas_) ||
          (transparent_ &&
           !CreateBitmap(&background_store_, &background_store_canvas_))) {
        DCHECK(false);
        ResetWindowlessBitmaps();
        return;
      }

      transport_store_handle = transport_store_->handle();
      if (background_store_.get())
        background_store_handle = background_store_->handle();
    }
  }

  IPC::Message* msg = new PluginMsg_UpdateGeometry(
      instance_id_, window_rect, clip_rect, cutout_rects, visible,
      transport_store_handle, background_store_handle);
  msg->set_unblock(true);
  Send(msg);
}

// Copied from render_widget.cc
static size_t GetPaintBufSize(const gfx::Rect& rect) {
  // TODO(darin): protect against overflow
  return 4 * rect.width() * rect.height();
}

void WebPluginDelegateProxy::ResetWindowlessBitmaps() {
  backing_store_.reset();
  transport_store_.reset();
  backing_store_canvas_.reset();
  transport_store_canvas_.reset();
  background_store_.reset();
  background_store_canvas_.release();
  backing_store_painted_ = gfx::Rect();
}

bool WebPluginDelegateProxy::CreateBitmap(
    scoped_ptr<base::SharedMemory>* memory,
    scoped_ptr<skia::PlatformCanvasWin>* canvas) {
  size_t size = GetPaintBufSize(plugin_rect_);
  scoped_ptr<base::SharedMemory> new_shared_memory(new base::SharedMemory());
  if (!new_shared_memory->Create(L"", false, true, size))
    return false;

  scoped_ptr<skia::PlatformCanvasWin> new_canvas(new skia::PlatformCanvasWin);
  if (!new_canvas->initialize(plugin_rect_.width(), plugin_rect_.height(),
                              true, new_shared_memory->handle())) {
    return false;
  }

  memory->swap(new_shared_memory);
  canvas->swap(new_canvas);
  return true;
}

void WebPluginDelegateProxy::Paint(HDC hdc, const gfx::Rect& damaged_rect) {
  // If the plugin is no longer connected (channel crashed) draw a crashed
  // plugin bitmap
  if (!channel_host_->channel_valid()) {
    PaintSadPlugin(hdc, damaged_rect);
    return;
  }

  // No paint events for windowed plugins.
  if (!windowless_)
    return;

  // We got a paint before the plugin's coordinates, so there's no buffer to
  // copy from.
  if (!backing_store_canvas_.get())
    return;

  // Limit the damaged rectangle to whatever is contained inside the plugin
  // rectangle, as that's the rectangle that we'll bitblt to the hdc.
  gfx::Rect rect = damaged_rect.Intersect(plugin_rect_);

  bool background_changed = false;
  if (background_store_canvas_.get() && BackgroundChanged(hdc, rect)) {
    background_changed = true;
    HDC background_hdc =
        background_store_canvas_->getTopPlatformDevice().getBitmapDC();
    BitBlt(background_hdc, rect.x()-plugin_rect_.x(), rect.y()-plugin_rect_.y(),
        rect.width(), rect.height(), hdc, rect.x(), rect.y(), SRCCOPY);
  }

  gfx::Rect offset_rect = rect;
  offset_rect.Offset(-plugin_rect_.x(), -plugin_rect_.y());
  if (background_changed || !backing_store_painted_.Contains(offset_rect)) {
    Send(new PluginMsg_Paint(instance_id_, offset_rect));
    CopyFromTransportToBacking(offset_rect);
  }

  HDC backing_hdc = backing_store_canvas_->getTopPlatformDevice().getBitmapDC();
  BitBlt(hdc, rect.x(), rect.y(), rect.width(), rect.height(), backing_hdc,
      rect.x()-plugin_rect_.x(), rect.y()-plugin_rect_.y(), SRCCOPY);

  if (invalidate_pending_) {
    // Only send the PaintAck message if this paint is in response to an 
    // invalidate from the plugin, since this message acts as an access token
    // to ensure only one process is using the transport dib at a time.
    invalidate_pending_ = false;
    Send(new PluginMsg_DidPaint(instance_id_));
  }
}

bool WebPluginDelegateProxy::BackgroundChanged(
    HDC hdc, 
    const gfx::Rect& rect) {
  HBITMAP hbitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));
  if (hbitmap == NULL) {
    NOTREACHED();
    return true;
  }

  BITMAP bitmap = { 0 };
  int result = GetObject(hbitmap, sizeof(bitmap), &bitmap);
  if (!result) {
    NOTREACHED();
    return true;
  }

  XFORM xf;
  if (!GetWorldTransform(hdc, &xf)) {
    NOTREACHED();
    return true;
  }

  int row_byte_size = rect.width() * (bitmap.bmBitsPixel / 8);
  for (int y = rect.y(); y < rect.bottom(); y++) {
    char* hdc_row_start = static_cast<char*>(bitmap.bmBits) +
        (y + static_cast<int>(xf.eDy)) * bitmap.bmWidthBytes +
        (rect.x() + static_cast<int>(xf.eDx)) * (bitmap.bmBitsPixel / 8);

    // getAddr32 doesn't use the translation units, so we have to subtract
    // the plugin origin from the coordinates.
    uint32_t* canvas_row_start =
        background_store_canvas_->getDevice()->accessBitmap(true).getAddr32(
            rect.x() - plugin_rect_.x(), y - plugin_rect_.y());
    if (memcmp(hdc_row_start, canvas_row_start, row_byte_size) != 0)
      return true;
  }

  return false;
}

void WebPluginDelegateProxy::Print(HDC hdc) {
  PluginMsg_PrintResponse_Params params = { 0 };
  Send(new PluginMsg_Print(instance_id_, &params));

  base::SharedMemory memory(params.shared_memory, true);
  if (!memory.Map(params.size)) {
    NOTREACHED();
    return;
  }

  gfx::Emf emf;
  if (!emf.CreateFromData(memory.memory(), params.size)) {
    NOTREACHED();
    return;
  }
  // Playback the buffer.
  emf.Playback(hdc, NULL);
}

NPObject* WebPluginDelegateProxy::GetPluginScriptableObject() {
  if (npobject_)
    return NPN_RetainObject(npobject_);

  int route_id = MSG_ROUTING_NONE;
  void* npobject_ptr;
  Send(new PluginMsg_GetPluginScriptableObject(
      instance_id_, &route_id, &npobject_ptr));
  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  npobject_ = NPObjectProxy::Create(
      channel_host_.get(), route_id, npobject_ptr, NULL);

  return NPN_RetainObject(npobject_);
}

void WebPluginDelegateProxy::DidFinishLoadWithReason(NPReason reason) {
  Send(new PluginMsg_DidFinishLoadWithReason(instance_id_, reason));
}

void WebPluginDelegateProxy::SetFocus() {
  Send(new PluginMsg_SetFocus(instance_id_));
}

bool WebPluginDelegateProxy::HandleEvent(NPEvent* event, WebCursor* cursor) {
  bool handled;
  // A windowless plugin can enter a modal loop in the context of a
  // NPP_HandleEvent call, in which case we need to pump messages to
  // the plugin. We pass of the corresponding event handle to the
  // plugin process, which is set if the plugin does enter a modal loop.
  IPC::SyncMessage* message = new PluginMsg_HandleEvent(instance_id_,
                                                        *event, &handled,
                                                        cursor);
  message->set_pump_messages_event(modal_loop_pump_messages_event_);
  Send(message);
  return handled;
}

int WebPluginDelegateProxy::GetProcessId() {
  return channel_host_->peer_pid();
}

void WebPluginDelegateProxy::OnSetWindow(
    HWND window, HANDLE modal_loop_pump_messages_event) {
  windowless_ = window == NULL;
  if (plugin_)
    plugin_->SetWindow(window, modal_loop_pump_messages_event);

  DCHECK(modal_loop_pump_messages_event_ == NULL);
  modal_loop_pump_messages_event_.Set(modal_loop_pump_messages_event);
}

void WebPluginDelegateProxy::OnCancelResource(int id) {
  if (plugin_)
    plugin_->CancelResource(id);
}

void WebPluginDelegateProxy::OnInvalidateRect(const gfx::Rect& rect) {
  if (!plugin_)
    return;

  invalidate_pending_ = true;
  CopyFromTransportToBacking(rect);
  plugin_->InvalidateRect(rect);
}

void WebPluginDelegateProxy::OnGetWindowScriptNPObject(
    int route_id, bool* success, void** npobject_ptr) {
  *success = false;
  NPObject* npobject = NULL;
  if (plugin_)
    npobject = plugin_->GetWindowScriptNPObject();

  if (!npobject)
    return;

  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  NPObjectStub* stub = new NPObjectStub(npobject, channel_host_.get(),
                                        route_id);
  window_script_object_ = stub;
  window_script_object_->set_proxy(this);
  *success = true;
  *npobject_ptr = npobject;
}

void WebPluginDelegateProxy::OnGetPluginElement(
    int route_id, bool* success, void** npobject_ptr) {
  *success = false;
  NPObject* npobject = NULL;
  if (plugin_)
    npobject = plugin_->GetPluginElement();
  if (!npobject)
    return;

  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  NPObjectStub* stub = new NPObjectStub(npobject, channel_host_.get(),
                                        route_id);
  *success = true;
  *npobject_ptr = npobject;
}

void WebPluginDelegateProxy::OnSetCookie(const GURL& url,
                                         const GURL& policy_url,
                                         const std::string& cookie) {
  if (plugin_)
    plugin_->SetCookie(url, policy_url, cookie);
}

void WebPluginDelegateProxy::OnGetCookies(const GURL& url,
                                          const GURL& policy_url,
                                          std::string* cookies) {
  DCHECK(cookies);
  if (plugin_)
    *cookies = plugin_->GetCookies(url, policy_url);
}

void WebPluginDelegateProxy::OnShowModalHTMLDialog(
    const GURL& url, int width, int height, const std::string& json_arguments,
    std::string* json_retval) {
  DCHECK(json_retval);
  if (render_view_)
    render_view_->ShowModalHTMLDialog(url, width, height, json_arguments,
                                      json_retval);
}

void WebPluginDelegateProxy::OnMissingPluginStatus(int status) {
  if (render_view_)
    render_view_->OnMissingPluginStatus(this, status);
}

void WebPluginDelegateProxy::OnGetCPBrowsingContext(uint32* context) {
  *context = render_view_ ? render_view_->GetCPBrowsingContext() : 0;
}

void WebPluginDelegateProxy::PaintSadPlugin(HDC hdc, const gfx::Rect& rect) {
  const int width = plugin_rect_.width();
  const int height = plugin_rect_.height();

  ChromeCanvas canvas(width, height, false);
  SkPaint paint;

  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);
  canvas.drawRectCoords(0, 0, SkIntToScalar(width), SkIntToScalar(height),
                        paint);

  if (!sad_plugin_) {
    sad_plugin_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SAD_PLUGIN);
  }

  if (sad_plugin_) {
    canvas.DrawBitmapInt(*sad_plugin_,
                         std::max(0, (width - sad_plugin_->width())/2),
                         std::max(0, (height - sad_plugin_->height())/2));
  }

  canvas.getTopPlatformDevice().drawToHDC(
      hdc, plugin_rect_.x(), plugin_rect_.y(), NULL);
  return;
}

void WebPluginDelegateProxy::CopyFromTransportToBacking(const gfx::Rect& rect) {
  if (!backing_store_canvas_.get())
    return;

  // Copy the damaged rect from the transport bitmap to the backing store.
  HDC backing = backing_store_canvas_->getTopPlatformDevice().getBitmapDC();
  HDC transport = transport_store_canvas_->getTopPlatformDevice().getBitmapDC();
  BitBlt(backing, rect.x(), rect.y(), rect.width(), rect.height(),
      transport, rect.x(), rect.y(), SRCCOPY);
  backing_store_painted_ = backing_store_painted_.Union(rect);
}

void WebPluginDelegateProxy::OnHandleURLRequest(
    const PluginHostMsg_URLRequest_Params& params) {
  const char* data = NULL;
  if (params.buffer.size())
    data = &params.buffer[0];

  const char* target = NULL;
  if (params.target.length())
    target = params.target.c_str();

  plugin_->HandleURLRequest(params.method.c_str(),
                            params.is_javascript_url, target,
                            static_cast<unsigned int>(params.buffer.size()),
                            data, params.is_file_data, params.notify,
                            params.url.c_str(), params.notify_data,
                            params.popups_allowed);
}

WebPluginResourceClient* WebPluginDelegateProxy::CreateResourceClient(
    int resource_id, const std::string &url, bool notify_needed,
    void* notify_data, void* npstream) {
  ResourceClientProxy* proxy = new ResourceClientProxy(channel_host_,
                                                       instance_id_);
  proxy->Initialize(resource_id, url, notify_needed, notify_data, npstream);
  return proxy;
}

void WebPluginDelegateProxy::URLRequestRouted(const std::string& url,
                                               bool notify_needed,
                                               void* notify_data) {
  Send(new PluginMsg_URLRequestRouted(instance_id_, url, notify_needed,
                                      notify_data));
}

void WebPluginDelegateProxy::OnCancelDocumentLoad() {
  plugin_->CancelDocumentLoad();
}

void WebPluginDelegateProxy::OnInitiateHTTPRangeRequest(
   const std::string& url, const std::string& range_info,
   HANDLE existing_stream, bool notify_needed, HANDLE notify_data) {
  plugin_->InitiateHTTPRangeRequest(url.c_str(), range_info.c_str(),
                                    existing_stream, notify_needed,
                                    notify_data);
}
