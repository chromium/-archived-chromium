// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__
#define CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "base/shared_memory.h"
#include "base/timer.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/chrome_plugin_api.h"
#include "webkit/glue/webplugin.h"

class PluginChannel;
class WebPluginDelegateImpl;

// This is an implementation of WebPlugin that proxies all calls to the
// renderer.
class WebPluginProxy : public WebPlugin {
 public:
  // Creates a new proxy for WebPlugin, using the given sender to send the
  // marshalled WebPlugin calls.
  WebPluginProxy(PluginChannel* channel,
                 int route_id,
                 WebPluginDelegateImpl* delegate,
                 HANDLE modal_dialog_event);
  ~WebPluginProxy();

  // WebPlugin overrides
  void SetWindow(HWND window, HANDLE pump_messages_event);
  void CancelResource(int id);
  void Invalidate();
  void InvalidateRect(const gfx::Rect& rect);
  NPObject* GetWindowScriptNPObject();
  NPObject* GetPluginElement();
  WebFrame* GetWebFrame() {
    return NULL;  // doesn't make sense in the plugin process.
  }
  void SetCookie(const GURL& url,
                 const GURL& policy_url,
                 const std::string& cookie);
  std::string GetCookies(const GURL& url, const GURL& policy_url);

  void ShowModalHTMLDialog(const GURL& url, int width, int height,
                           const std::string& json_arguments,
                           std::string* json_retval);
  void OnMissingPluginStatus(int status);
  // class-specific methods

  // Retrieves the browsing context associated with the renderer this plugin
  // is in.  Calling multiple times will return the same value.
  CPBrowsingContext GetCPBrowsingContext();

  // Retrieves the WebPluginProxy for the given context that was returned by
  // GetCPBrowsingContext, or NULL if not found.
  static WebPluginProxy* FromCPBrowsingContext(CPBrowsingContext context);

  // Returns a WebPluginResourceClient object given its id, or NULL if no
  // object with that id exists.
  WebPluginResourceClient* GetResourceClient(int id);

  // Notification received on a plugin issued resource request
  // creation.
  void OnResourceCreated(int resource_id, HANDLE cookie);

  void HandleURLRequest(const char *method,
                        bool is_javascript_url,
                        const char* target, unsigned int len,
                        const char* buf, bool is_file_data,
                        bool notify, const char* url,
                        void* notify_data, bool popups_allowed);

  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      bool visible,
                      const SharedMemoryHandle& windowless_buffer,
                      const SharedMemoryLock& lock);

  void CancelDocumentLoad();

  void InitiateHTTPRangeRequest(const char* url,
                                const char* range_info,
                                void* existing_stream,
                                bool notify_needed,
                                HANDLE notify_data);

 private:
  bool Send(IPC::Message* msg);

  // Called periodically so that we can paint windowless plugins.
  void OnPaintTimerFired();

  // Updates the shared memory section where windowless plugins paint.
  void SetWindowlessBuffer(const SharedMemoryHandle& handle,
                           const SharedMemoryLock& lock);

  // Called when a plugin's origin moves, so that we can update the world
  // transform of the local HDC.
  void UpdateTransform();

  typedef base::hash_map<int, WebPluginResourceClient*> ResourceClientMap;
  ResourceClientMap resource_clients_;

  scoped_refptr<PluginChannel> channel_;
  int route_id_;
  NPObject* window_npobject_;
  NPObject* plugin_element_;
  WebPluginDelegateImpl* delegate_;
  uint32 cp_browsing_context_;
  ScopedHandle modal_dialog_event_;

  // Used to desynchronize windowless painting.  We accumulate invalidates and
  // paint into a shared buffer when our repeating timer fires.  After painting
  // we tell the renderer asynchronously and it paints from the buffer.  This
  // allows the renderer to paint without a blocking call, which improves
  // performance, and lets us control the frame rate at which we paint.
  gfx::Rect damaged_rect_;
  base::RepeatingTimer<WebPluginProxy> paint_timer_;
  ScopedHandle windowless_shared_section_;
  ScopedBitmap windowless_bitmap_;
  ScopedHDC windowless_hdc_;
  ScopedHandle windowless_buffer_lock_;
};

#endif  // CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__
