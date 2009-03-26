// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__
#define CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/timer.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/chrome_plugin_api.h"
#include "webkit/glue/webplugin.h"

namespace base {
class WaitableEvent;
}

class PluginChannel;
class WebPluginDelegate;

// This is an implementation of WebPlugin that proxies all calls to the
// renderer.
class WebPluginProxy : public WebPlugin {
 public:
  // Creates a new proxy for WebPlugin, using the given sender to send the
  // marshalled WebPlugin calls.
  WebPluginProxy(PluginChannel* channel,
                 int route_id,
                 WebPluginDelegate* delegate,
                 HANDLE modal_dialog_event);
  ~WebPluginProxy();

  // WebPlugin overrides
  bool SetWindow(HWND window, HANDLE pump_messages_event);
  void CancelResource(int id);
  void Invalidate();
  void InvalidateRect(const gfx::Rect& rect);
  NPObject* GetWindowScriptNPObject();
  NPObject* GetPluginElement();
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

  // For windowless plugins, paints the given rectangle into the local buffer.
  void Paint(const gfx::Rect& rect);

  // Callback from the renderer to let us know that a paint occurred.
  void DidPaint();

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
                      const base::SharedMemoryHandle& windowless_buffer,
                      const base::SharedMemoryHandle& background_buffer);

  void CancelDocumentLoad();

  void InitiateHTTPRangeRequest(const char* url,
                                const char* range_info,
                                void* existing_stream,
                                bool notify_needed,
                                HANDLE notify_data);

  bool IsOffTheRecord();

  base::WaitableEvent* modal_dialog_event() {
    return modal_dialog_event_.get();
  }

 private:
  bool Send(IPC::Message* msg);

  // Updates the shared memory section where windowless plugins paint.
  void SetWindowlessBuffer(const base::SharedMemoryHandle& windowless_buffer,
                           const base::SharedMemoryHandle& background_buffer);

  // Converts a shared memory section handle from the renderer process into a
  // bitmap and hdc that are mapped to this process.
  void ConvertBuffer(const base::SharedMemoryHandle& buffer,
                     ScopedHandle* shared_section,
                     ScopedBitmap* bitmap,
                     ScopedHDC* hdc);

  // Handler for sending over the paint event to the plugin.
  void OnPaint(const gfx::Rect& damaged_rect);

  // Called when a plugin's origin moves, so that we can update the world
  // transform of the local HDC.
  void UpdateTransform();

  typedef base::hash_map<int, WebPluginResourceClient*> ResourceClientMap;
  ResourceClientMap resource_clients_;

  scoped_refptr<PluginChannel> channel_;
  int route_id_;
  NPObject* window_npobject_;
  NPObject* plugin_element_;
  WebPluginDelegate* delegate_;
  gfx::Rect damaged_rect_;
  bool waiting_for_paint_;
  uint32 cp_browsing_context_;
  scoped_ptr<base::WaitableEvent> modal_dialog_event_;
  HWND parent_window_;

  // Variables used for desynchronized windowless plugin painting.  See note in
  // webplugin_delegate_proxy.h for how this works.

  // These hold the bitmap where the plugin draws.
  ScopedHandle windowless_shared_section_;
  ScopedBitmap windowless_bitmap_;
  ScopedHDC windowless_hdc_;

  // These hold the bitmap of the background image.
  ScopedHandle background_shared_section_;
  ScopedBitmap background_bitmap_;
  ScopedHDC background_hdc_;

  ScopedRunnableMethodFactory<WebPluginProxy> runnable_method_factory_;
};

#endif  // CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__
