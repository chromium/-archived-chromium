// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/webplugin_proxy.h"

#include "base/gfx/gdi_util.h"
#include "base/scoped_handle.h"
#include "base/shared_memory.h"
#include "base/singleton.h"
#include "base/waitable_event.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/win_util.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/plugin/plugin_channel.h"
#include "chrome/plugin/plugin_thread.h"
#include "chrome/plugin/webplugin_delegate_stub.h"
#include "skia/ext/platform_device.h"
#include "webkit/glue/webplugin_delegate.h"

typedef std::map<CPBrowsingContext, WebPluginProxy*> ContextMap;
static ContextMap& GetContextMap() {
  return *Singleton<ContextMap>::get();
}

WebPluginProxy::WebPluginProxy(
    PluginChannel* channel,
    int route_id,
    WebPluginDelegate* delegate,
    HANDLE modal_dialog_event)
    : channel_(channel),
      route_id_(route_id),
      cp_browsing_context_(0),
      window_npobject_(NULL),
      plugin_element_(NULL),
      delegate_(delegate),
      waiting_for_paint_(false),
#pragma warning(suppress: 4355)  // can use this
      runnable_method_factory_(this),
      parent_window_(NULL) {

  HANDLE event;
  BOOL result = DuplicateHandle(channel->renderer_handle(),
      modal_dialog_event,
      GetCurrentProcess(),
      &event,
      SYNCHRONIZE,
      FALSE,
      0);
  DCHECK(result) <<
      "Couldn't duplicate the modal dialog handle for the plugin.";
  modal_dialog_event_.reset(new base::WaitableEvent(event));
}

WebPluginProxy::~WebPluginProxy() {
  if (cp_browsing_context_)
    GetContextMap().erase(cp_browsing_context_);

  if (parent_window_) {
    PluginThread::current()->Send(
        new PluginProcessHostMsg_DestroyWindow(parent_window_));
  }
}

bool WebPluginProxy::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void WebPluginProxy::SetWindow(HWND window, HANDLE pump_messages_event) {
  HANDLE pump_messages_event_for_renderer = NULL;

  if (pump_messages_event) {
    DCHECK(window == NULL);
    DuplicateHandle(GetCurrentProcess(), pump_messages_event,
                    channel_->renderer_handle(),
                    &pump_messages_event_for_renderer,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    DCHECK(pump_messages_event_for_renderer != NULL);
  } else {
    DCHECK (window);
    // To make scrolling windowed plugins fast, we create the page's direct
    // child windows in the browser process.  This way no cross process messages
    // are sent.
    HWND old_parent = GetParent(window);
    IPC::SyncMessage* msg = new PluginProcessHostMsg_CreateWindow(
        old_parent, &parent_window_);

    // Need to process window messages in the meantime to avoid a deadlock if
    // the browser paints or sends any other (synchronous) WM_ message to the
    // plugin window.
    msg->EnableMessagePumping();
    PluginThread::current()->Send(msg);

    SetParent(window, parent_window_);

    // We want the browser process to move this window which has a message loop
    // in its process.
    window = parent_window_;
  }

  Send(new PluginHostMsg_SetWindow(route_id_, window,
                                   pump_messages_event_for_renderer));
}

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
  void* npobject_ptr;
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
  void* npobject_ptr;
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
                                      void* notify_data, bool popups_allowed) {
  if (!url) {
    NOTREACHED();
    return;
  }

  if (!target && (0 == _strcmpi(method, "GET"))) {
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

void WebPluginProxy::Paint(const gfx::Rect& rect) {
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

  // Before we send the invalidate, paint so that renderer uses the updated
  // bitmap.
  delegate_->Paint(windowless_hdc_, offset_rect);
}

void WebPluginProxy::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect,
    const base::SharedMemoryHandle& windowless_buffer,
    const base::SharedMemoryHandle& background_buffer) {
  gfx::Rect old = delegate_->GetRect();
  gfx::Rect old_clip_rect = delegate_->GetClipRect();

  bool moved = old.x() != window_rect.x() || old.y() != window_rect.y();
  delegate_->UpdateGeometry(window_rect, clip_rect);
  if (windowless_buffer) {
    // The plugin's rect changed, so now we have a new buffer to draw into.
    SetWindowlessBuffer(windowless_buffer, background_buffer);
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
}

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

  skia::PlatformDeviceWin::InitializeDC(hdc->Get());
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

void WebPluginProxy::CancelDocumentLoad() {
  Send(new PluginHostMsg_CancelDocumentLoad(route_id_));
}

void WebPluginProxy::InitiateHTTPRangeRequest(const char* url,
                                              const char* range_info,
                                              void* existing_stream,
                                              bool notify_needed,
                                              HANDLE notify_data) {

  Send(new PluginHostMsg_InitiateHTTPRangeRequest(route_id_, url,
                                                  range_info, existing_stream,
                                                  notify_needed, notify_data));
}

void WebPluginProxy::OnPaint(const gfx::Rect& damaged_rect) {
  Paint(damaged_rect);
  Send(new PluginHostMsg_InvalidateRect(route_id_, damaged_rect));
}
