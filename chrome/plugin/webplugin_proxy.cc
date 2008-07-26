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

#include "chrome/plugin/webplugin_proxy.h"

#include "base/scoped_handle.h"
#include "base/singleton.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/plugin_channel.h"
#include "chrome/plugin/webplugin_delegate_stub.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/npobject_util.h"
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
      waiting_for_paint_(false) {

  HANDLE event;
  BOOL result = DuplicateHandle(channel->renderer_handle(),
      modal_dialog_event,
      GetCurrentProcess(),
      &event,
      SYNCHRONIZE,
      FALSE,
      0);
  DCHECK(result) << "Couldn't duplicate the modal dialog handle for the plugin.";
  modal_dialog_event_.Set(event);
}

WebPluginProxy::~WebPluginProxy() {
  if (cp_browsing_context_)
    GetContextMap().erase(cp_browsing_context_);
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
  }

  Send(new PluginHostMsg_SetWindow(route_id_, window,
                                   pump_messages_event_for_renderer));
}

void WebPluginProxy::CancelResource(int id) {
  Send(new PluginHostMsg_CancelResource(route_id_, id));
  resource_clients_.erase(id);
}

void WebPluginProxy::Invalidate() {
  Send(new PluginHostMsg_Invalidate(route_id_));
}

void WebPluginProxy::InvalidateRect(const gfx::Rect& rect) {
  // Ignore NPN_InvalidateRect calls with empty rects.
  if (rect.IsEmpty()) {
    return;
  }
  // Only send a single InvalidateRect message at a time.  From WillPaint we
  // will dispatch an additional InvalidateRect message if necessary.
  if (waiting_for_paint_) {
    damaged_rect_ = damaged_rect_.Union(rect);
  } else {
    waiting_for_paint_ = true;
    Send(new PluginHostMsg_InvalidateRect(route_id_, rect));
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
                                           modal_dialog_event_.Get());

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
                                          modal_dialog_event_.Get());

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
  ScopedHandle event(CreateEvent(NULL, FALSE, TRUE, NULL));
  msg->set_pump_messages_event(event);

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

void WebPluginProxy::WillPaint() {
  // If we have an accumulated damaged rect, then check to see if we need to
  // send out another InvalidateRect message.
  waiting_for_paint_ = false;
  if (!damaged_rect_.IsEmpty()) {
    InvalidateRect(damaged_rect_);
    damaged_rect_ = gfx::Rect();
  }
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
