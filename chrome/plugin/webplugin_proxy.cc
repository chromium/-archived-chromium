// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/webplugin_proxy.h"

#include "app/gfx/canvas.h"
#if defined(OS_WIN)
#include "app/win_util.h"
#endif
#include "base/scoped_handle.h"
#include "base/shared_memory.h"
#include "base/singleton.h"
#include "base/waitable_event.h"
#include "build/build_config.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/plugin/plugin_channel.h"
#include "chrome/plugin/plugin_thread.h"
#include "chrome/plugin/webplugin_delegate_stub.h"
#include "skia/ext/platform_device.h"
#include "webkit/glue/webplugin_delegate.h"

#if defined(OS_WIN)
#include "base/gfx/gdi_util.h"
#endif

typedef std::map<CPBrowsingContext, WebPluginProxy*> ContextMap;
static ContextMap& GetContextMap() {
  return *Singleton<ContextMap>::get();
}

WebPluginProxy::WebPluginProxy(
    PluginChannel* channel,
    int route_id,
    WebPluginDelegate* delegate)
    : channel_(channel),
      route_id_(route_id),
      cp_browsing_context_(0),
      window_npobject_(NULL),
      plugin_element_(NULL),
      delegate_(delegate),
      waiting_for_paint_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(runnable_method_factory_(this))
{
}

WebPluginProxy::~WebPluginProxy() {
  if (cp_browsing_context_)
    GetContextMap().erase(cp_browsing_context_);
}

bool WebPluginProxy::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

#if defined(OS_LINUX)
gfx::PluginWindowHandle WebPluginProxy::CreatePluginContainer() {
  gfx::PluginWindowHandle container;
  Send(new PluginHostMsg_CreatePluginContainer(route_id_, &container));
  return container;
}
#endif

void WebPluginProxy::SetWindow(gfx::PluginWindowHandle window) {
  Send(new PluginHostMsg_SetWindow(route_id_, window));
}

void WebPluginProxy::WillDestroyWindow(gfx::PluginWindowHandle window) {
#if defined(OS_WIN)
  PluginThread::current()->Send(
      new PluginProcessHostMsg_PluginWindowDestroyed(
          window, ::GetParent(window)));
#elif defined(OS_LINUX)
  Send(new PluginHostMsg_DestroyPluginContainer(route_id_, window));
#else
  NOTIMPLEMENTED();
#endif
}

#if defined(OS_WIN)
void WebPluginProxy::SetWindowlessPumpEvent(HANDLE pump_messages_event) {
  HANDLE pump_messages_event_for_renderer = NULL;
  DuplicateHandle(GetCurrentProcess(), pump_messages_event,
                  channel_->renderer_handle(),
                  &pump_messages_event_for_renderer,
                  0, FALSE, DUPLICATE_SAME_ACCESS);
  DCHECK(pump_messages_event_for_renderer != NULL);
  Send(new PluginHostMsg_SetWindowlessPumpEvent(
      route_id_, pump_messages_event_for_renderer));
}

bool WebPluginProxy::SetModalDialogEvent(HANDLE modal_dialog_event) {
  // TODO(port): figure out how this will be set in the browser process, or
  // come up with a different mechanism.
  HANDLE event = NULL;
  BOOL result = DuplicateHandle(channel_->renderer_handle(),
      modal_dialog_event,
      GetCurrentProcess(),
      &event,
      SYNCHRONIZE,
      FALSE,
      0);
  DCHECK(result) <<
      "Couldn't duplicate the modal dialog handle for the plugin." \
      "handle: " << channel_->renderer_handle() << ". err: " << GetLastError();
  if (!event)
    return false;

  modal_dialog_event_.reset(new base::WaitableEvent(event));
  return true;
}
#endif

void WebPluginProxy::CancelResource(int id) {
  Send(new PluginHostMsg_CancelResource(route_id_, id));
  resource_clients_.erase(id);
}

void WebPluginProxy::Invalidate() {
  gfx::Rect rect(0, 0,
                 delegate_->GetRect().width(),
                 delegate_->GetRect().height());
  InvalidateRect(rect);
}

void WebPluginProxy::InvalidateRect(const gfx::Rect& rect) {
  damaged_rect_ = damaged_rect_.Union(rect);
  // Ignore NPN_InvalidateRect calls with empty rects.  Also don't send an
  // invalidate if it's outside the clipping region, since if we did it won't
  // lead to a paint and we'll be stuck waiting forever for a DidPaint response.
  if (rect.IsEmpty() || !delegate_->GetClipRect().Intersects(rect))
    return;

  // Only send a single InvalidateRect message at a time.  From DidPaint we
  // will dispatch an additional InvalidateRect message if necessary.
  if (!waiting_for_paint_) {
    waiting_for_paint_ = true;
    // Invalidates caused by calls to NPN_InvalidateRect/NPN_InvalidateRgn
    // need to be painted asynchronously as per the NPAPI spec.
    MessageLoop::current()->PostTask(FROM_HERE,
        runnable_method_factory_.NewRunnableMethod(
            &WebPluginProxy::OnPaint, damaged_rect_));
    damaged_rect_ = gfx::Rect();
  }
}

NPObject* WebPluginProxy::GetWindowScriptNPObject() {
  if (window_npobject_)
    return NPN_RetainObject(window_npobject_);

  int npobject_route_id = channel_->GenerateRouteID();
  bool success = false;
  intptr_t npobject_ptr;
  Send(new PluginHostMsg_GetWindowScriptNPObject(
      route_id_, npobject_route_id, &success, &npobject_ptr));
  if (!success)
    return NULL;

  window_npobject_ = NPObjectProxy::Create(channel_,
                                           npobject_route_id,
                                           npobject_ptr,
                                           modal_dialog_event_.get());

  return window_npobject_;
}

NPObject* WebPluginProxy::GetPluginElement() {
  if (plugin_element_)
    return NPN_RetainObject(plugin_element_);

  int npobject_route_id = channel_->GenerateRouteID();
  bool success = false;
  intptr_t npobject_ptr;
  Send(new PluginHostMsg_GetPluginElement(
      route_id_, npobject_route_id, &success, &npobject_ptr));
  if (!success)
    return NULL;

  plugin_element_ = NPObjectProxy::Create(channel_,
                                          npobject_route_id,
                                          npobject_ptr,
                                          modal_dialog_event_.get());

  return plugin_element_;
}

void WebPluginProxy::SetCookie(const GURL& url,
                               const GURL& policy_url,
                               const std::string& cookie) {
  Send(new PluginHostMsg_SetCookie(route_id_, url, policy_url, cookie));
}

std::string WebPluginProxy::GetCookies(const GURL& url,
                                       const GURL& policy_url) {
  std::string cookies;
  Send(new PluginHostMsg_GetCookies(route_id_, url, policy_url, &cookies));

  return cookies;
}

void WebPluginProxy::ShowModalHTMLDialog(const GURL& url, int width, int height,
                                         const std::string& json_arguments,
                                         std::string* json_retval) {
  PluginHostMsg_ShowModalHTMLDialog* msg =
      new PluginHostMsg_ShowModalHTMLDialog(
          route_id_, url, width, height, json_arguments, json_retval);

  // Create a new event and set it.  This forces us to pump messages while
  // waiting for a response (which won't come until the dialog is closed).  This
  // avoids a deadlock.
  scoped_ptr<base::WaitableEvent> event(
      new base::WaitableEvent(false, true));
  msg->set_pump_messages_event(event.get());

  Send(msg);
}

void WebPluginProxy::OnMissingPluginStatus(int status) {
  Send(new PluginHostMsg_MissingPluginStatus(route_id_, status));
}

CPBrowsingContext WebPluginProxy::GetCPBrowsingContext() {
  if (cp_browsing_context_ == 0) {
    Send(new PluginHostMsg_GetCPBrowsingContext(route_id_,
                                                &cp_browsing_context_));
    GetContextMap()[cp_browsing_context_] = this;
  }
  return cp_browsing_context_;
}

WebPluginProxy* WebPluginProxy::FromCPBrowsingContext(
    CPBrowsingContext context) {
  return GetContextMap()[context];
}

WebPluginResourceClient* WebPluginProxy::GetResourceClient(int id) {
  ResourceClientMap::iterator iterator = resource_clients_.find(id);
  if (iterator == resource_clients_.end()) {
    NOTREACHED();
    return NULL;
  }

  return iterator->second;
}

void WebPluginProxy::DidPaint() {
  // If we have an accumulated damaged rect, then check to see if we need to
  // send out another InvalidateRect message.
  waiting_for_paint_ = false;
  if (!damaged_rect_.IsEmpty())
    InvalidateRect(damaged_rect_);
}

void WebPluginProxy::OnResourceCreated(int resource_id, HANDLE cookie) {
  WebPluginResourceClient* resource_client =
      reinterpret_cast<WebPluginResourceClient*>(cookie);
  if (!resource_client) {
    NOTREACHED();
    return;
  }

  DCHECK(resource_clients_.find(resource_id) == resource_clients_.end());
  resource_clients_[resource_id] = resource_client;
}

void WebPluginProxy::HandleURLRequest(const char *method,
                                      bool is_javascript_url,
                                      const char* target, unsigned int len,
                                      const char* buf, bool is_file_data,
                                      bool notify, const char* url,
                                      intptr_t notify_data,
                                      bool popups_allowed) {
  if (!url) {
    NOTREACHED();
    return;
  }

  if (!target && (0 == base::strcasecmp(method, "GET"))) {
    // Please refer to https://bugzilla.mozilla.org/show_bug.cgi?id=366082
    // for more details on this.
    if (delegate_->GetQuirks() &
        WebPluginDelegate::PLUGIN_QUIRK_BLOCK_NONSTANDARD_GETURL_REQUESTS) {
      GURL request_url(url);
      if (!request_url.SchemeIs(chrome::kHttpScheme) &&
          !request_url.SchemeIs(chrome::kHttpsScheme) &&
          !request_url.SchemeIs(chrome::kFtpScheme)) {
        return;
      }
    }
  }

  PluginHostMsg_URLRequest_Params params;
  params.method = method;
  params.is_javascript_url = is_javascript_url;
  if (target)
    params.target = std::string(target);

  if (len) {
    params.buffer.resize(len);
    memcpy(&params.buffer.front(), buf, len);
  }

  params.is_file_data = is_file_data;
  params.notify = notify;
  params.url = url;
  params.notify_data = notify_data;
  params.popups_allowed = popups_allowed;

  Send(new PluginHostMsg_URLRequest(route_id_, params));
}

bool WebPluginProxy::GetDragData(struct NPObject* event, bool add_data,
                                 int32* identity, int32* event_id,
                                 std::string* type, std::string* data) {
  DCHECK(event);
  NPObjectProxy* proxy = NPObjectProxy::GetProxy(event);
  if (!proxy)  // NPObject* event should have/be a renderer proxy.
    return false;

  NPVariant_Param event_param;
  event_param.type = NPVARIANT_PARAM_OBJECT_POINTER;
  event_param.npobject_pointer = proxy->npobject_ptr();
  if (!event_param.npobject_pointer)
    return false;

  std::vector<NPVariant_Param> values;
  bool success = false;
  Send(new PluginHostMsg_GetDragData(route_id_, event_param, add_data,
                                     &values, &success));
  if (!success)
    return false;

  DCHECK(values.size() == 4);
  DCHECK(values[0].type == NPVARIANT_PARAM_INT);
  *identity = static_cast<int32>(values[0].int_value);
  DCHECK(values[1].type == NPVARIANT_PARAM_INT);
  *event_id = static_cast<int32>(values[1].int_value);
  DCHECK(values[2].type == NPVARIANT_PARAM_STRING);
  type->swap(values[2].string_value);
  if (add_data && (values[3].type == NPVARIANT_PARAM_STRING))
    data->swap(values[3].string_value);

  return true;
}

bool WebPluginProxy::SetDropEffect(struct NPObject* event, int effect) {
  DCHECK(event);
  NPObjectProxy* proxy = NPObjectProxy::GetProxy(event);
  if (!proxy)  // NPObject* event should have/be a renderer proxy.
    return false;

  NPVariant_Param event_param;
  event_param.type = NPVARIANT_PARAM_OBJECT_POINTER;
  event_param.npobject_pointer = proxy->npobject_ptr();
  if (!event_param.npobject_pointer)
    return false;

  bool success = false;
  Send(new PluginHostMsg_SetDropEffect(route_id_, event_param, effect,
                                       &success));
  return success;
}

void WebPluginProxy::Paint(const gfx::Rect& rect) {
#if defined(OS_WIN)
  if (!windowless_hdc_)
    return;

  // Clear the damaged area so that if the plugin doesn't paint there we won't
  // end up with the old values.
  gfx::Rect offset_rect = rect;
  offset_rect.Offset(delegate_->GetRect().origin());
  if (!background_hdc_) {
    FillRect(windowless_hdc_, &offset_rect.ToRECT(),
        static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
  } else {
    BitBlt(windowless_hdc_, offset_rect.x(), offset_rect.y(),
      offset_rect.width(), offset_rect.height(), background_hdc_,
      rect.x(), rect.y(), SRCCOPY);
  }

  RECT clip_rect = rect.ToRECT();
  HRGN clip_region = CreateRectRgnIndirect(&clip_rect);
  SelectClipRgn(windowless_hdc_, clip_region);

  // Before we send the invalidate, paint so that renderer uses the updated
  // bitmap.
  delegate_->Paint(windowless_hdc_, offset_rect);

  SelectClipRgn(windowless_hdc_, NULL);
  DeleteObject(clip_region);
#else
  // TODO(port): windowless painting.
  NOTIMPLEMENTED();
#endif
}

void WebPluginProxy::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect,
    const TransportDIB::Id& windowless_buffer_id,
    const TransportDIB::Id& background_buffer_id) {
  // TODO(port): this isn't correct usage of a TransportDIB; for now,
  // the caller temporarly just stuffs the handle into the HANDLE
  // field of the TransportDIB::Id so it should behave like the older
  // code.
  gfx::Rect old = delegate_->GetRect();
  gfx::Rect old_clip_rect = delegate_->GetClipRect();

  delegate_->UpdateGeometry(window_rect, clip_rect);
#if defined(OS_WIN)
  bool moved = old.x() != window_rect.x() || old.y() != window_rect.y();
  if (windowless_buffer_id.handle) {
    // The plugin's rect changed, so now we have a new buffer to draw into.
    SetWindowlessBuffer(windowless_buffer_id.handle,
                        background_buffer_id.handle);
  } else if (moved) {
    // The plugin moved, so update our world transform.
    UpdateTransform();
  }
  // Send over any pending invalidates which occured when the plugin was
  // off screen.
  if (delegate_->IsWindowless() && !clip_rect.IsEmpty() &&
      old_clip_rect.IsEmpty() && !damaged_rect_.IsEmpty()) {
    InvalidateRect(damaged_rect_);
  }
#else
  NOTIMPLEMENTED();
#endif
}

#if defined(OS_WIN)
void WebPluginProxy::SetWindowlessBuffer(
    const base::SharedMemoryHandle& windowless_buffer,
    const base::SharedMemoryHandle& background_buffer) {
  // Convert the shared memory handle to a handle that works in our process,
  // and then use that to create an HDC.
  ConvertBuffer(windowless_buffer,
                &windowless_shared_section_,
                &windowless_bitmap_,
                &windowless_hdc_);
  if (background_buffer) {
    ConvertBuffer(background_buffer,
                  &background_shared_section_,
                  &background_bitmap_,
                  &background_hdc_);
  }
  UpdateTransform();
}

void WebPluginProxy::ConvertBuffer(const base::SharedMemoryHandle& buffer,
                                   ScopedHandle* shared_section,
                                   ScopedBitmap* bitmap,
                                   ScopedHDC* hdc) {
  shared_section->Set(win_util::GetSectionFromProcess(
      buffer, channel_->renderer_handle(), false));
  if (shared_section->Get() == NULL) {
    NOTREACHED();
    return;
  }

  void* data = NULL;
  HDC screen_dc = GetDC(NULL);
  BITMAPINFOHEADER bitmap_header;
  gfx::CreateBitmapHeader(delegate_->GetRect().width(),
                          delegate_->GetRect().height(),
                          &bitmap_header);
  bitmap->Set(CreateDIBSection(
      screen_dc, reinterpret_cast<const BITMAPINFO*>(&bitmap_header),
      DIB_RGB_COLORS, &data, shared_section->Get(), 0));
  ReleaseDC(NULL, screen_dc);
  if (bitmap->Get() == NULL) {
    NOTREACHED();
    return;
  }

  hdc->Set(CreateCompatibleDC(NULL));
  if (hdc->Get() == NULL) {
    NOTREACHED();
    return;
  }

  skia::PlatformDevice::InitializeDC(hdc->Get());
  SelectObject(hdc->Get(), bitmap->Get());
}

void WebPluginProxy::UpdateTransform() {
  if (!windowless_hdc_)
    return;

  XFORM xf;
  xf.eDx = static_cast<FLOAT>(-delegate_->GetRect().x());
  xf.eDy = static_cast<FLOAT>(-delegate_->GetRect().y());
  xf.eM11 = 1;
  xf.eM21 = 0;
  xf.eM12 = 0;
  xf.eM22 = 1;
  SetWorldTransform(windowless_hdc_, &xf);
}
#endif

void WebPluginProxy::CancelDocumentLoad() {
  Send(new PluginHostMsg_CancelDocumentLoad(route_id_));
}

void WebPluginProxy::InitiateHTTPRangeRequest(const char* url,
                                              const char* range_info,
                                              intptr_t existing_stream,
                                              bool notify_needed,
                                              intptr_t notify_data) {

  Send(new PluginHostMsg_InitiateHTTPRangeRequest(route_id_, url,
                                                  range_info, existing_stream,
                                                  notify_needed, notify_data));
}

void WebPluginProxy::OnPaint(const gfx::Rect& damaged_rect) {
  Paint(damaged_rect);
  Send(new PluginHostMsg_InvalidateRect(route_id_, damaged_rect));
}

bool WebPluginProxy::IsOffTheRecord() {
  return channel_->off_the_record();
}

void WebPluginProxy::ResourceClientDeleted(
    WebPluginResourceClient* resource_client) {
  ResourceClientMap::iterator index = resource_clients_.begin();
  while (index != resource_clients_.end()) {
    WebPluginResourceClient* client = (*index).second;

    if (client == resource_client) {
      resource_clients_.erase(index++);
    } else {
      index++;
    }
  }
}
