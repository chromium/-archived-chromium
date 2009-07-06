// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webplugin_delegate_proxy.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <atlbase.h>
#endif

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/gfx/size.h"
#include "base/gfx/native_widget_types.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/npobject_stub.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/mime_util.h"
#include "printing/native_metafile.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webview.h"

#if defined(OS_POSIX)
#include "chrome/common/ipc_channel_posix.h"
#endif

using WebKit::WebInputEvent;
using WebKit::WebDragData;
using WebKit::WebVector;
using WebKit::WebString;

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
                  intptr_t notify_data, intptr_t existing_stream) {
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

    multibyte_response_expected_ = (existing_stream != 0);

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
  scoped_refptr<PluginChannelHost> channel_;
  int instance_id_;
  int resource_id_;
  std::string url_;
  bool notify_needed_;
  intptr_t notify_data_;
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
      plugin_(NULL),
      windowless_(false),
      mime_type_(mime_type),
      clsid_(clsid),
      npobject_(NULL),
      window_script_object_(NULL),
      sad_plugin_(NULL),
      invalidate_pending_(false),
      transparent_(false) {
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

bool WebPluginDelegateProxy::Initialize(const GURL& url, char** argn,
                                        char** argv, int argc,
                                        WebPlugin* plugin,
                                        bool load_manually) {
  IPC::ChannelHandle channel_handle;
  FilePath plugin_path;
  if (!RenderThread::current()->Send(new ViewHostMsg_OpenChannelToPlugin(
          url, mime_type_, clsid_, webkit_glue::GetWebKitLocale(),
          &channel_handle, &plugin_path))) {
    return false;
  }

#if defined(OS_POSIX)
  // If we received a ChannelHandle, register it now.
  if (channel_handle.socket.fd >= 0)
    IPC::AddChannelSocket(channel_handle.name, channel_handle.socket.fd);
#endif

  MessageLoop* ipc_message_loop = RenderThread::current()->owner_loop();
  scoped_refptr<PluginChannelHost> channel_host =
      PluginChannelHost::GetPluginChannelHost(channel_handle.name,
                                              ipc_message_loop);
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
#if defined(OS_WIN)
  params.modal_dialog_event = render_view_->modal_dialog_event()->handle();
#endif

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
                                                  intptr_t notify_data) {
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
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetWindowlessPumpEvent,
                        OnSetWindowlessPumpEvent)
#endif
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
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetDragData, OnGetDragData);
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetDropEffect, OnSetDropEffect);
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
                const gfx::Rect& clip_rect) {
  plugin_rect_ = window_rect;

  // Be careful to explicitly call the default constructors for these ids,
  // as they can be POD on some platforms and we want them initialized.
  TransportDIB::Id transport_store_id = TransportDIB::Id();
  TransportDIB::Id background_store_id = TransportDIB::Id();

  if (windowless_) {
#if defined(OS_WIN)
    // TODO(port): use TransportDIB instead of allocating these directly.
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

        // TODO(port): once we use TransportDIB we will properly fill in these
        // ids; for now we just fill in the HANDLE field.
        transport_store_id.handle = transport_store_->handle();
        if (background_store_.get())
          background_store_id.handle = background_store_->handle();
      }
    }
#else
    // TODO(port): refactor our allocation of backing stores.
    NOTIMPLEMENTED();
#endif
  }

  IPC::Message* msg = new PluginMsg_UpdateGeometry(
      instance_id_, window_rect, clip_rect,
      transport_store_id, background_store_id);
  msg->set_unblock(true);
  Send(msg);
}

#if defined(OS_WIN)
// Copied from render_widget.cc
static size_t GetPaintBufSize(const gfx::Rect& rect) {
  // TODO(darin): protect against overflow
  return 4 * rect.width() * rect.height();
}
#endif

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
    scoped_ptr<skia::PlatformCanvas>* canvas) {
#if defined(OS_WIN)
  size_t size = GetPaintBufSize(plugin_rect_);
  scoped_ptr<base::SharedMemory> new_shared_memory(new base::SharedMemory());
  if (!new_shared_memory->Create(L"", false, true, size))
    return false;

  scoped_ptr<skia::PlatformCanvas> new_canvas(new skia::PlatformCanvas);
  if (!new_canvas->initialize(plugin_rect_.width(), plugin_rect_.height(),
                              true, new_shared_memory->handle())) {
    return false;
  }

  memory->swap(new_shared_memory);
  canvas->swap(new_canvas);
  return true;
#else
  // TODO(port): use TransportDIB properly.
  NOTIMPLEMENTED();
  return false;
#endif
}

void WebPluginDelegateProxy::Paint(gfx::NativeDrawingContext context,
                                   const gfx::Rect& damaged_rect) {
  // If the plugin is no longer connected (channel crashed) draw a crashed
  // plugin bitmap
  if (!channel_host_->channel_valid()) {
    PaintSadPlugin(context, damaged_rect);
    return;
  }

  // No paint events for windowed plugins.
  if (!windowless_)
    return;

  // TODO(port): side-stepping some windowless plugin code for now.
#if defined(OS_WIN)
  // We got a paint before the plugin's coordinates, so there's no buffer to
  // copy from.
  if (!backing_store_canvas_.get())
    return;

  // Limit the damaged rectangle to whatever is contained inside the plugin
  // rectangle, as that's the rectangle that we'll bitblt to the hdc.
  gfx::Rect rect = damaged_rect.Intersect(plugin_rect_);

  bool background_changed = false;
  if (background_store_canvas_.get() && BackgroundChanged(context, rect)) {
    background_changed = true;
    HDC background_hdc =
        background_store_canvas_->getTopPlatformDevice().getBitmapDC();
    BitBlt(background_hdc, rect.x()-plugin_rect_.x(), rect.y()-plugin_rect_.y(),
        rect.width(), rect.height(), context, rect.x(), rect.y(), SRCCOPY);
  }

  gfx::Rect offset_rect = rect;
  offset_rect.Offset(-plugin_rect_.x(), -plugin_rect_.y());
  if (background_changed || !backing_store_painted_.Contains(offset_rect)) {
    Send(new PluginMsg_Paint(instance_id_, offset_rect));
    CopyFromTransportToBacking(offset_rect);
  }

  HDC backing_hdc = backing_store_canvas_->getTopPlatformDevice().getBitmapDC();
  BitBlt(context, rect.x(), rect.y(), rect.width(), rect.height(), backing_hdc,
      rect.x()-plugin_rect_.x(), rect.y()-plugin_rect_.y(), SRCCOPY);

  if (invalidate_pending_) {
    // Only send the PaintAck message if this paint is in response to an
    // invalidate from the plugin, since this message acts as an access token
    // to ensure only one process is using the transport dib at a time.
    invalidate_pending_ = false;
    Send(new PluginMsg_DidPaint(instance_id_));
  }
#else
  // TODO(port): windowless plugin paint handling goes here.
  NOTIMPLEMENTED();
#endif
}

#if defined(OS_WIN)
// TODO(port): this should be portable; just avoiding windowless plugins for
// now.
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

  // The damaged rect that we're given can be larger than the bitmap, so
  // intersect their rects first.
  gfx::Rect bitmap_rect(static_cast<int>(-xf.eDx), static_cast<int>(-xf.eDy),
                        bitmap.bmWidth, bitmap.bmHeight);
  gfx::Rect check_rect = rect.Intersect(bitmap_rect);
  int row_byte_size = check_rect.width() * (bitmap.bmBitsPixel / 8);
  for (int y = check_rect.y(); y < check_rect.bottom(); y++) {
    char* hdc_row_start = static_cast<char*>(bitmap.bmBits) +
        (y + static_cast<int>(xf.eDy)) * bitmap.bmWidthBytes +
        (check_rect.x() + static_cast<int>(xf.eDx)) * (bitmap.bmBitsPixel / 8);

    // getAddr32 doesn't use the translation units, so we have to subtract
    // the plugin origin from the coordinates.
    uint32_t* canvas_row_start =
        background_store_canvas_->getDevice()->accessBitmap(true).getAddr32(
            check_rect.x() - plugin_rect_.x(), y - plugin_rect_.y());
    if (memcmp(hdc_row_start, canvas_row_start, row_byte_size) != 0)
      return true;
  }

  return false;
}
#endif

void WebPluginDelegateProxy::Print(gfx::NativeDrawingContext context) {
  base::SharedMemoryHandle shared_memory;
  size_t size;
  if (!Send(new PluginMsg_Print(instance_id_, &shared_memory, &size)))
    return;

  base::SharedMemory memory(shared_memory, true);
  if (!memory.Map(size)) {
    NOTREACHED();
    return;
  }

#if defined(OS_WIN)
  printing::NativeMetafile metafile;
  if (!metafile.CreateFromData(memory.memory(), size)) {
    NOTREACHED();
    return;
  }
  // Playback the buffer.
  metafile.Playback(context, NULL);
#else
  // TODO(port): plugin printing.
  NOTIMPLEMENTED();
#endif
}

NPObject* WebPluginDelegateProxy::GetPluginScriptableObject() {
  if (npobject_)
    return NPN_RetainObject(npobject_);

  int route_id = MSG_ROUTING_NONE;
  intptr_t npobject_ptr;
  Send(new PluginMsg_GetPluginScriptableObject(
      instance_id_, &route_id, &npobject_ptr));
  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  npobject_ = NPObjectProxy::Create(
      channel_host_.get(), route_id, npobject_ptr,
      render_view_->modal_dialog_event());

  return NPN_RetainObject(npobject_);
}

void WebPluginDelegateProxy::DidFinishLoadWithReason(NPReason reason) {
  Send(new PluginMsg_DidFinishLoadWithReason(instance_id_, reason));
}

void WebPluginDelegateProxy::SetFocus() {
  Send(new PluginMsg_SetFocus(instance_id_));
}

bool WebPluginDelegateProxy::HandleInputEvent(
    const WebInputEvent& event,
    WebCursor* cursor) {
  bool handled;
  // A windowless plugin can enter a modal loop in the context of a
  // NPP_HandleEvent call, in which case we need to pump messages to
  // the plugin. We pass of the corresponding event handle to the
  // plugin process, which is set if the plugin does enter a modal loop.
  IPC::SyncMessage* message = new PluginMsg_HandleInputEvent(
      instance_id_, &event, &handled, cursor);
  message->set_pump_messages_event(modal_loop_pump_messages_event_.get());
  Send(message);
  return handled;
}

int WebPluginDelegateProxy::GetProcessId() {
  return channel_host_->peer_pid();
}

void WebPluginDelegateProxy::OnSetWindow(gfx::NativeViewId window_id) {
#if defined(OS_WIN)
  gfx::NativeView window = gfx::NativeViewFromId(window_id);
  windowless_ = window == NULL;
  if (plugin_)
    plugin_->SetWindow(window);
#else
  NOTIMPLEMENTED();
#endif
}

#if defined(OS_WIN)
  void WebPluginDelegateProxy::OnSetWindowlessPumpEvent(
      HANDLE modal_loop_pump_messages_event) {
  DCHECK(modal_loop_pump_messages_event_ == NULL);

  modal_loop_pump_messages_event_.reset(
      new base::WaitableEvent(modal_loop_pump_messages_event));
}
#endif

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
    int route_id, bool* success, intptr_t* npobject_ptr) {
  *success = false;
  NPObject* npobject = NULL;
  if (plugin_)
    npobject = plugin_->GetWindowScriptNPObject();

  if (!npobject)
    return;

  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  NPObjectStub* stub = new NPObjectStub(
      npobject, channel_host_.get(), route_id,
      render_view_->modal_dialog_event());
  window_script_object_ = stub;
  window_script_object_->set_proxy(this);
  *success = true;
  *npobject_ptr = reinterpret_cast<intptr_t>(npobject);
}

void WebPluginDelegateProxy::OnGetPluginElement(
    int route_id, bool* success, intptr_t* npobject_ptr) {
  *success = false;
  NPObject* npobject = NULL;
  if (plugin_)
    npobject = plugin_->GetPluginElement();
  if (!npobject)
    return;

  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  new NPObjectStub(
      npobject, channel_host_.get(), route_id,
      render_view_->modal_dialog_event());
  *success = true;
  *npobject_ptr = reinterpret_cast<intptr_t>(npobject);
}

void WebPluginDelegateProxy::OnSetCookie(const GURL& url,
                                         const GURL& first_party_for_cookies,
                                         const std::string& cookie) {
  if (plugin_)
    plugin_->SetCookie(url, first_party_for_cookies, cookie);
}

void WebPluginDelegateProxy::OnGetCookies(const GURL& url,
                                          const GURL& first_party_for_cookies,
                                          std::string* cookies) {
  DCHECK(cookies);
  if (plugin_)
    *cookies = plugin_->GetCookies(url, first_party_for_cookies);
}

void WebPluginDelegateProxy::OnShowModalHTMLDialog(
    const GURL& url, int width, int height, const std::string& json_arguments,
    std::string* json_retval) {
  DCHECK(json_retval);
  if (render_view_)
    render_view_->ShowModalHTMLDialog(url, width, height, json_arguments,
                                      json_retval);
}

static void EncodeDragData(const WebDragData& data, bool add_data,
                           NPVariant* drag_type, NPVariant* drag_data) {
  const NPString* np_drag_type;
  if (data.hasFileNames()) {
    static const NPString kFiles = { "Files", 5 };
    np_drag_type = &kFiles;
  } else {
    static const NPString kEmpty = { "" , 0 };
    np_drag_type = &kEmpty;
    add_data = false;
  }

  STRINGN_TO_NPVARIANT(np_drag_type->UTF8Characters,
                       np_drag_type->UTF8Length,
                       *drag_type);
  if (!add_data) {
    VOID_TO_NPVARIANT(*drag_data);
    return;
  }

  WebVector<WebString> files;
  data.fileNames(files);

  static std::string utf8;
  utf8.clear();
  for (size_t i = 0; i < files.size(); ++i) {
    static const char kBackspaceDelimiter('\b');
    if (i != 0)
      utf8.append(1, kBackspaceDelimiter);
    utf8.append(UTF16ToUTF8(files[i]));
  }

  STRINGN_TO_NPVARIANT(utf8.data(), utf8.length(), *drag_data);
}

void WebPluginDelegateProxy::OnGetDragData(const NPVariant_Param& object,
                                           bool add_data,
                                           std::vector<NPVariant_Param>* values,
                                           bool* success) {
  DCHECK(values && success);
  *success = false;

  WebView* webview = NULL;
  if (render_view_)
    webview = render_view_->webview();
  if (!webview)
    return;

  int event_id;
  WebDragData data;
  NPObject* event = reinterpret_cast<NPObject*>(object.npobject_pointer);
  const int32 drag_id = webview->GetDragIdentity();
  if (!drag_id || !webkit_glue::GetDragData(event, &event_id, &data))
    return;

  NPVariant results[4];
  INT32_TO_NPVARIANT(drag_id, results[0]);
  INT32_TO_NPVARIANT(event_id, results[1]);
  EncodeDragData(data, add_data, &results[2], &results[3]);

  for (size_t i = 0; i < arraysize(results); ++i) {
    values->push_back(NPVariant_Param());
    CreateNPVariantParam(results[i], NULL, &values->back(), false, NULL);
  }

  *success = true;
}

void WebPluginDelegateProxy::OnSetDropEffect(const NPVariant_Param& object,
                                             int effect,
                                             bool* success) {
  DCHECK(success);
  *success = false;

  WebView* webview = NULL;
  if (render_view_)
    webview = render_view_->webview();
  if (!webview)
    return;

  NPObject* event = reinterpret_cast<NPObject*>(object.npobject_pointer);
  const int32 drag_id = webview->GetDragIdentity();
  if (!drag_id || !webkit_glue::IsDragEvent(event))
    return;

  *success = webview->SetDropEffect(effect != 0);
}

void WebPluginDelegateProxy::OnMissingPluginStatus(int status) {
  if (render_view_)
    render_view_->OnMissingPluginStatus(this, status);
}

void WebPluginDelegateProxy::OnGetCPBrowsingContext(uint32* context) {
  *context = render_view_ ? render_view_->GetCPBrowsingContext() : 0;
}

void WebPluginDelegateProxy::PaintSadPlugin(gfx::NativeDrawingContext hdc,
                                            const gfx::Rect& rect) {
#if defined(OS_WIN)
  const int width = plugin_rect_.width();
  const int height = plugin_rect_.height();

  gfx::Canvas canvas(width, height, false);
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
#else
  // TODO(port): it ought to be possible to refactor this to be shared between
  // platforms.  It's just the final drawToHDC that kills us.
  NOTIMPLEMENTED();
#endif
}

void WebPluginDelegateProxy::CopyFromTransportToBacking(const gfx::Rect& rect) {
  if (!backing_store_canvas_.get())
    return;

#if defined(OS_WIN)
  // Copy the damaged rect from the transport bitmap to the backing store.
  HDC backing = backing_store_canvas_->getTopPlatformDevice().getBitmapDC();
  HDC transport = transport_store_canvas_->getTopPlatformDevice().getBitmapDC();
  BitBlt(backing, rect.x(), rect.y(), rect.width(), rect.height(),
      transport, rect.x(), rect.y(), SRCCOPY);
  backing_store_painted_ = backing_store_painted_.Union(rect);
#else
  // TODO(port): probably some new code in TransportDIB should go here.
  NOTIMPLEMENTED();
#endif
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
    intptr_t notify_data, intptr_t npstream) {
  ResourceClientProxy* proxy = new ResourceClientProxy(channel_host_,
                                                       instance_id_);
  proxy->Initialize(resource_id, url, notify_needed, notify_data, npstream);
  return proxy;
}

void WebPluginDelegateProxy::URLRequestRouted(const std::string& url,
                                               bool notify_needed,
                                               intptr_t notify_data) {
  Send(new PluginMsg_URLRequestRouted(instance_id_, url, notify_needed,
                                      notify_data));
}

void WebPluginDelegateProxy::OnCancelDocumentLoad() {
  plugin_->CancelDocumentLoad();
}

void WebPluginDelegateProxy::OnInitiateHTTPRangeRequest(
    const std::string& url, const std::string& range_info,
    intptr_t existing_stream, bool notify_needed, intptr_t notify_data) {
  plugin_->InitiateHTTPRangeRequest(url.c_str(), range_info.c_str(),
                                    existing_stream, notify_needed,
                                    notify_data);
}
