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

#ifndef WEBKIT_GLUE_WEBPLUGIN_H__
#define WEBKIT_GLUE_WEBPLUGIN_H__

#include <string>
#include <vector>
#include <windows.h>

#include "base/basictypes.h"
#include "base/gfx/rect.h"

typedef struct HWND__* HWND;

class GURL;
class WebFrame;
class WebPluginResourceClient;

struct NPObject;

// Describes a mime type entry for a plugin.
struct WebPluginMimeType {
  // The actual mime type.
  std::string mime_type;

  // A list of all the file extensions for this mime type.
  std::vector<std::string> file_extensions;

  // Description of the mime type.
  std::wstring description;
};


// Describes an available NPAPI plugin.
struct WebPluginInfo {
  // The name of the plugin (i.e. Flash).
  std::wstring name;

  // The path to the dll.
  std::wstring file;

  // The version number of the plugin file (may be OS-specific)
  std::wstring version;

  // A description of the plugin that we get from it's version info.
  std::wstring desc;

  // A list of all the mime types that this plugin supports.
  std::vector<WebPluginMimeType> mime_types;
};


// Describes the new location for a plugin window.
struct WebPluginGeometry {
  HWND window;
  gfx::Rect window_rect;
  gfx::Rect clip_rect;
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

  // Called by the plugin delegate to let the WebPlugin know if the plugin is
  // windowed (i.e. handle is not NULL) or windowless (handle is NULL).  This
  // tells the WebPlugin to send mouse/keyboard events to the plugin delegate,
  // as well as the information about the HDC for paint operations.
  // The pump_messages_event is a event handle which is valid only for
  // windowless plugins and is used in NPP_HandleEvent calls to pump messages
  // if the plugin enters a modal loop.
  virtual void SetWindow(HWND window, HANDLE pump_messages_event) = 0;
  // Cancels a pending request.
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
                                void* notify_data, bool popups_allowed) = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebPlugin);
};

// Simpler version of ResourceHandleClient that lends itself to proxying.
class WebPluginResourceClient {
 public:
  virtual void WillSendRequest(const GURL& url) = 0;
  virtual void DidReceiveResponse(const std::string& mime_type,
                                  const std::string& headers,
                                  uint32 expected_length,
                                  uint32 last_modified,
                                  bool* cancel) = 0;
  virtual void DidReceiveData(const char* buffer, int length) = 0;
  virtual void DidFinishLoading() = 0;
  virtual void DidFail() = 0;
};


#endif  // #ifndef WEBKIT_GLUE_WEBPLUGIN_H__
