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
      notify_needed_(false), notify_data_(NULL) {
  }

  ~ResourceClientProxy() {
  }

  void Initialize(int resource_id, const std::string &url, bool notify_needed,
                  void *notify_data) {
    resource_id_ = resource_id;
    url_ = url;
    notify_needed_ = notify_needed;
    notify_data_ = notify_data;

    PluginMsg_URLRequestReply_Params params;
    params.resource_id = resource_id;
    params.url = url_;
    params.notify_needed = notify_needed_;
    params.notify_data = notify_data_;

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
                          bool* cancel) {
    DCHECK(channel_ != NULL);
    PluginMsg_DidReceiveResponseParams params;
    params.id = resource_id_;
    params.mime_type = mime_type;
    params.headers = headers;
    params.expected_length = expected_length;
    params.last_modified = last_modified;
    // Grab a reference on the underlying channel so it does not get
    // deleted from under us.
    scoped_refptr<PluginChannelHost> channel_ref(channel_);
    channel_->Send(new PluginMsg_DidReceiveResponse(instance_id_, params,
                                                    cancel));
  }

  void DidReceiveData(const char* buffer, int length) {
    DCHECK(channel_ != NULL);
    DCHECK(length > 0);
    std::vector<char> data;
    data.resize(static_cast<size_t>(length));
    memcpy(&data.front(), buffer, length);
    // Grab a reference on the underlying channel so it does not get
    // deleted from under us.
    scoped_refptr<PluginChannelHost> channel_ref(channel_);
    channel_->Send(new PluginMsg_DidReceiveData(instance_id_, resource_id_,
                                                data));
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

 private:
  int resource_id_;
  int instance_id_;
  scoped_refptr<PluginChannelHost> channel_;
  std::string url_;
  bool notify_needed_;
  void* notify_data_;
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
      first_paint_(true),
      npobject_(NULL),
      send_deferred_update_geometry_(false),
      visible_(false),
      sad_plugin_(NULL),
      window_script_object_(NULL),
      modal_loop_pump_messages_event_(NULL) {
}

WebPluginDelegateProxy::~WebPluginDelegateProxy() {
  if (npobject_)
    NPN_ReleaseObject(npobject_);

  if (window_script_object_) {
    window_script_object_->set_proxy(NULL);
    window_script_object_->set_invalid();
  }

  if (modal_loop_pump_messages_event_) {
    CloseHandle(modal_loop_pump_messages_event_);
  }
}

void WebPluginDelegateProxy::PluginDestroyed() {
  plugin_ = NULL;

  if (channel_host_) {
    if (npobject_) {
      // When we destroy the plugin instance, the NPObjectStub NULLs out its
      // pointer to the npobject (see NPObjectStub::OnChannelError).  Therefore,
      // we release the object before destroying the instance to avoid leaking.
      NPN_ReleaseObject(npobject_);
      npobject_ = NULL;
    }

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
                                      visible_));
  }
}

bool WebPluginDelegateProxy::Initialize(const GURL& url, char** argn,
                                        char** argv, int argc,
                                        WebPlugin* plugin,
                                        bool load_manually) {
  std::wstring channel_name, plugin_path;
  if (!RenderThread::current()->Send(new ViewHostMsg_OpenChannelToPlugin(
      url, mime_type_, clsid_, webkit_glue::GetWebKitLocale(),
      &channel_name, &plugin_path)))
    return false;

  MessageLoop* ipc_message_loop = RenderThread::current()->owner_loop();
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

std::wstring WebPluginDelegateProxy::GetPluginPath() {
  return plugin_path_;
}

void WebPluginDelegateProxy::InstallMissingPlugin() {
  Send(new PluginMsg_InstallMissingPlugin(instance_id_));
}

void WebPluginDelegateProxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(WebPluginDelegateProxy, msg)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetWindow, OnSetWindow)
    IPC_MESSAGE_HANDLER(PluginHostMsg_CancelResource, OnCancelResource)
    IPC_MESSAGE_HANDLER(PluginHostMsg_Invalidate, OnInvalidate)
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
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void WebPluginDelegateProxy::OnChannelError() {
  OnInvalidate();
  render_view_->PluginCrashed(plugin_path_);
}

void WebPluginDelegateProxy::UpdateGeometry(const gfx::Rect& window_rect,
                                            const gfx::Rect& clip_rect,
                                            bool visible) {
  plugin_rect_ = window_rect;

  if (windowless_) {
    IPC::Message* msg = new PluginMsg_UpdateGeometry(
        instance_id_, window_rect, clip_rect, visible);
    msg->set_unblock(true);
    Send(msg);
  } else {
    deferred_clip_rect_ = clip_rect;
    visible_ = visible;
    send_deferred_update_geometry_ = true;
  }
}

void WebPluginDelegateProxy::Paint(HDC hdc, const gfx::Rect& damaged_rect) {
  // If the plugin is no longer connected (channel crashed) draw a crashed
  // plugin bitmap
  if (!channel_host_->channel_valid()) {
    PaintSadPlugin(hdc, damaged_rect);
    return;
  }

  // Can't duplicate an HDC handle, so get all the parameters we need in
  // order to recreate the same HDC in the plugin process.
  PluginMsg_Paint_Params params;
  HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));
  if (bitmap == NULL) {
    NOTREACHED();
    return;
  }

  DIBSECTION dibsection = { 0 };
  int result = GetObject(bitmap, sizeof(dibsection), &dibsection);
  if (!result) {
    NOTREACHED();
    return;
  }

  win_util::ScopedHRGN hrgn(CreateRectRgn(0, 0, 0, 0));
  result = GetClipRgn(hdc, hrgn);
  if (result == -1) {
    NOTREACHED();
    return;
  }

  params.size.SetSize(dibsection.dsBmih.biWidth, dibsection.dsBmih.biHeight);
  if (result == 0) {
    // No clipping region.
    params.clip_rect.set_width(params.size.width());
    params.clip_rect.set_height(params.size.height());
  } else {
    RECT clip_rect;
    result = GetRgnBox(hrgn, &clip_rect);
    DCHECK_NE(result, 0);
    params.clip_rect = clip_rect;
  }

  if (!GetWorldTransform(hdc, &params.xf)) {
    NOTREACHED();
    return;
  }

  params.damaged_rect = damaged_rect;
  params.shared_memory = dibsection.dshSection;

  // Normal painting code path.
  if (dibsection.dshSection) {
    // No paint events for windowed plugins.  However, if it is the first paint
    // we don't know yet whether the plugin is windowless or not, so we have to
    // send the event.
    if (!windowless_ && !first_paint_) {
      // TODO(maruel): That's not true for printing and thumbnail capture.
      // We shall use PrintWindow() to draw the window.
      return;
    }
    first_paint_ = false;

    DCHECK(params.size.width());
    DCHECK(params.size.height());
    Send(new PluginMsg_Paint(instance_id_, params));
    return;
  }

  params.size.SetSize(damaged_rect.width(), damaged_rect.height());
  // Pass the window size instead.
  if (!windowless_)
    params.damaged_rect = plugin_rect_;

  // When the thumbnail is painted or during printing, a shared memory handle
  // isn't given to CreateDIBSection so we don't get one now.
  size_t bytes = 0;
  SharedMemoryHandle emf_buffer = NULL;
  PluginMsg_PaintIntoSharedMemory* msg =
      new PluginMsg_PaintIntoSharedMemory(instance_id_, params, &emf_buffer,
                                          &bytes);
  if (!Send(msg))
    return;

  if (!emf_buffer) {
    NOTREACHED();
    return;
  }

  // memory's destructor will automatically close emf_buffer.
  SharedMemory memory(emf_buffer, true);
  if (!memory.Map(bytes)) {
    NOTREACHED();
    return;
  }

  gfx::Emf emf;
  if (!emf.CreateFromData(memory.memory(), bytes)) {
    NOTREACHED();
    return;
  }
  emf.Playback(hdc, NULL);
}

void WebPluginDelegateProxy::Print(HDC hdc) {
  PluginMsg_PrintResponse_Params params = { 0 };
  Send(new PluginMsg_Print(instance_id_, &params));

  SharedMemory memory(params.shared_memory, true);
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

HWND WebPluginDelegateProxy::GetWindowHandle() {
  NOTREACHED() << "GetWindowHandle can't be called on the proxy.";
  return NULL;
}

void WebPluginDelegateProxy::OnSetWindow(
    HWND window, HANDLE modal_loop_pump_messages_event) {
  windowless_ = window == NULL;
  if (plugin_)
    plugin_->SetWindow(window, modal_loop_pump_messages_event);

  modal_loop_pump_messages_event_ = modal_loop_pump_messages_event;
}

void WebPluginDelegateProxy::OnCancelResource(int id) {
  if (plugin_)
    plugin_->CancelResource(id);
}

void WebPluginDelegateProxy::OnInvalidate() {
  if (plugin_)
    plugin_->Invalidate();
}

void WebPluginDelegateProxy::OnInvalidateRect(const gfx::Rect& rect) {
  if (plugin_)
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
    sad_plugin_ = ResourceBundle::LoadBitmap(
        _AtlBaseModule.GetResourceInstance(), IDR_CRASHED_PLUGIN);
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
    void *notify_data) {
  ResourceClientProxy* proxy = new ResourceClientProxy(channel_host_,
                                                       instance_id_);
  proxy->Initialize(resource_id, url, notify_needed, notify_data);
  return proxy;
}

void WebPluginDelegateProxy::URLRequestRouted(const std::string& url,
                                               bool notify_needed,
                                               void* notify_data) {
  Send(new PluginMsg_URLRequestRouted(instance_id_, url, notify_needed,
                                      notify_data));
}
