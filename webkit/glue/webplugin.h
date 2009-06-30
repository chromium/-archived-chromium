// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBPLUGIN_H__
#define WEBKIT_GLUE_WEBPLUGIN_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/gfx/native_widget_types.h"

// TODO(port): this typedef is obviously incorrect on non-Windows
// platforms, but now a lot of code now accidentally depends on them
// existing.  #ifdef out these declarations and fix all the users.
typedef void* HANDLE;

class GURL;
class WebFrame;
class WebPluginResourceClient;

struct NPObject;

// Describes the new location for a plugin window.
struct WebPluginGeometry {
  // On Windows, this is the plugin window in the plugin process.
  // On X11, this is the browser process's hosting window (the GtkSocket).
  gfx::PluginWindowHandle window;
  gfx::Rect window_rect;
  // Clip rect (include) and cutouts (excludes), relative to
  // window_rect origin.
  gfx::Rect clip_rect;
  std::vector<gfx::Rect> cutout_rects;
  bool rects_valid;
  bool visible;
};


enum RoutingStatus {
  ROUTED,
  NOT_ROUTED,
  INVALID_URL,
  GENERAL_FAILURE
};

// The WebKit side of a plugin implementation.  It provides wrappers around
// operations that need to interact with the frame and other WebCore objects.
class WebPlugin {
 public:
  WebPlugin() { }
  virtual ~WebPlugin() { }

#if defined(OS_LINUX)
  // Called by the plugin delegate to request a container for a new
  // windowed plugin.  This handle will later get destroyed with
  // WillDestroyWindow.
  virtual gfx::PluginWindowHandle CreatePluginContainer() = 0;
#endif

  // Called by the plugin delegate to let the WebPlugin know if the plugin is
  // windowed (i.e. handle is not NULL) or windowless (handle is NULL).  This
  // tells the WebPlugin to send mouse/keyboard events to the plugin delegate,
  // as well as the information about the HDC for paint operations.
  virtual void SetWindow(gfx::PluginWindowHandle window) = 0;

  // Called by the plugin delegate to let it know that the window is being
  // destroyed.
  virtual void WillDestroyWindow(gfx::PluginWindowHandle window) = 0;
#if defined(OS_WIN)
  // The pump_messages_event is a event handle which is valid only for
  // windowless plugins and is used in NPP_HandleEvent calls to pump messages
  // if the plugin enters a modal loop.
  // Cancels a pending request.
  virtual void SetWindowlessPumpEvent(HANDLE pump_messages_event) = 0;
#endif
  virtual void CancelResource(int id) = 0;
  virtual void Invalidate() = 0;
  virtual void InvalidateRect(const gfx::Rect& rect) = 0;

  // Returns the NPObject for the browser's window object.
  virtual NPObject* GetWindowScriptNPObject() = 0;

  // Returns the DOM element that loaded the plugin.
  virtual NPObject* GetPluginElement() = 0;

  // Cookies
  virtual void SetCookie(const GURL& url,
                         const GURL& policy_url,
                         const std::string& cookie) = 0;
  virtual std::string GetCookies(const GURL& url,
                                 const GURL& policy_url) = 0;

  // Shows a modal HTML dialog containing the given URL.  json_arguments are
  // passed to the dialog via the DOM 'window.chrome.dialogArguments', and the
  // retval is the string returned by 'window.chrome.send("DialogClose",
  // retval)'.
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   std::string* json_retval) = 0;

  // When a default plugin has downloaded the plugin list and finds it is
  // available, it calls this method to notify the renderer. Also it will update
  // the status when user clicks on the plugin to install.
  virtual void OnMissingPluginStatus(int status) = 0;

  // Handles GetURL/GetURLNotify/PostURL/PostURLNotify requests initiated
  // by plugins.
  virtual void HandleURLRequest(const char *method,
                                bool is_javascript_url,
                                const char* target, unsigned int len,
                                const char* buf, bool is_file_data,
                                bool notify, const char* url,
                                intptr_t notify_data, bool popups_allowed) = 0;

  // Cancels document load.
  virtual void CancelDocumentLoad() = 0;

  // Initiates a HTTP range request.
  virtual void InitiateHTTPRangeRequest(const char* url,
                                        const char* range_info,
                                        intptr_t existing_stream,
                                        bool notify_needed,
                                        intptr_t notify_data) = 0;

  // Returns true iff in off the record (Incognito) mode.
  virtual bool IsOffTheRecord() = 0;

  // Called when the WebPluginResourceClient instance is deleted.
  virtual void ResourceClientDeleted(
      WebPluginResourceClient* resource_client) {}

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebPlugin);
};

// Simpler version of ResourceHandleClient that lends itself to proxying.
class WebPluginResourceClient {
 public:
  virtual ~WebPluginResourceClient() {}
  virtual void WillSendRequest(const GURL& url) = 0;
  // The request_is_seekable parameter indicates whether byte range requests
  // can be issued for the underlying stream.
  virtual void DidReceiveResponse(const std::string& mime_type,
                                  const std::string& headers,
                                  uint32 expected_length,
                                  uint32 last_modified,
                                  bool request_is_seekable,
                                  bool* cancel) = 0;
  virtual void DidReceiveData(const char* buffer, int length,
                              int data_offset) = 0;
  virtual void DidFinishLoading() = 0;
  virtual void DidFail() = 0;
  virtual bool IsMultiByteResponseExpected() = 0;
};


#endif  // #ifndef WEBKIT_GLUE_WEBPLUGIN_H__
