// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__
#define CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/chrome_plugin_api.h"
#include "webkit/glue/webplugin.h"

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

  void WillPaint();

  // Notification received on a plugin issued resource request
  // creation.
  void OnResourceCreated(int resource_id, HANDLE cookie);

  void HandleURLRequest(const char *method,
                        bool is_javascript_url,
                        const char* target, unsigned int len,
                        const char* buf, bool is_file_data,
                        bool notify, const char* url,
                        void* notify_data, bool popups_allowed);

  void CancelDocumentLoad();

  void InitiateHTTPRangeRequest(const char* url,
                                const char* range_info,
                                void* existing_stream,
                                bool notify_needed,
                                HANDLE notify_data);
 private:
  bool Send(IPC::Message* msg);

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
  ScopedHandle modal_dialog_event_;
};

#endif  // CHROME_PLUGIN_PLUGIN_WEBPLUGIN_PROXY_H__
