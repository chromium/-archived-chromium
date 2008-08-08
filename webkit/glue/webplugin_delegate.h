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

#ifndef WEBKIT_GLUE_WEBPLUGIN_DELEGATE_H__
#define WEBKIT_GLUE_WEBPLUGIN_DELEGATE_H__

#include <string>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "third_party/npapi/bindings/npapi.h"

typedef struct HDC__* HDC;

class GURL;
class WebPlugin;
struct NPObject;
class WebCursor;
class WebPluginResourceClient;
// This is the interface that a plugin implementation needs to provide.
class WebPluginDelegate {
 public:
  WebPluginDelegate() {}

  // Initializes the plugin implementation with the given (UTF8) arguments.
  // Note that the lifetime of WebPlugin must be longer than this delegate.
  // If this function returns false the plugin isn't started and shouldn't be
  // called again.  If this method succeeds, then the WebPlugin is valid until
  // PluginDestroyed is called.
  // The load_manually parameter if true indicates that the plugin data would 
  // be passed from webkit. if false indicates that the plugin should download 
  // the data. This also controls whether the plugin is instantiated as a full 
  // page plugin (NP_FULL) or embedded (NP_EMBED)
  virtual bool Initialize(const GURL& url, char** argn, char** argv,
                          int argc, WebPlugin* plugin, bool load_manually) = 0;

  // Called when the WebPlugin is being destroyed.  This is a signal to the
  // delegate that it should tear-down the plugin implementation and not call
  // methods on the WebPlugin again.
  virtual void PluginDestroyed() = 0;

  // Update the geometry of the plugin.  This is a request to move the plugin,
  // relative to its containing window, to the coords given by window_rect.
  // Its contents should be clipped to the coords given by clip_rect, which are
  // relative to the origin of the plugin window.
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect, bool visible) = 0;

  // Tells the plugin to paint the damaged rect.  The HDC is only used for
  // windowless plugins.
  virtual void Paint(HDC hdc, const gfx::Rect& rect) = 0;

  // Tells the plugin to print itself.
  virtual void Print(HDC hdc) = 0;

  // Informs the plugin that it now has focus.
  virtual void SetFocus() = 0;

  // For windowless plugins, gives them a user event like mouse/keyboard.
  // Returns whether the event was handled.
  virtual bool HandleEvent(NPEvent* event, WebCursor* cursor) = 0;

  // Gets the NPObject associated with the plugin for scripting.
  virtual NPObject* GetPluginScriptableObject() = 0;

  // Receives notification about a resource load that the plugin initiated
  // for a frame.
  virtual void DidFinishLoadWithReason(NPReason reason) = 0;
  
  // Returns the process id of the process that is running the plugin.
  virtual int GetProcessId() = 0;

  // Returns the window handle for this plugin if it's a windowed plugin,
  // or NULL otherwise.
  virtual HWND GetWindowHandle() = 0;

  virtual void FlushGeometryUpdates() = 0;

  // The result of the script execution is returned via this function.
  virtual void SendJavaScriptStream(const std::string& url,
                                    const std::wstring& result, 
                                    bool success, bool notify_needed, 
                                    int notify_data) = 0;

  // Receives notification about data being available. 
  virtual void DidReceiveManualResponse(const std::string& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified) = 0;

  // Receives the data.
  virtual void DidReceiveManualData(const char* buffer, int length) = 0;

  // Indicates end of data load.
  virtual void DidFinishManualLoading() = 0;

  // Indicates a failure in data receipt.
  virtual void DidManualLoadFail() = 0;

  // Only Available after Initialize is called.
  virtual std::wstring GetPluginPath() = 0;

  // Only Supported when the plugin is the default plugin.
  virtual void InstallMissingPlugin() = 0;

  // Creates a WebPluginResourceClient instance and returns the same.
  virtual WebPluginResourceClient* CreateResourceClient(int resource_id,
                                                        const std::string &url,
                                                        bool notify_needed,
                                                        void *notify_data) = 0;
  // Notifies the delegate about a Get/Post URL request getting routed
  virtual void URLRequestRouted(const std::string&url, bool notify_needed, 
                                void* notify_data) = 0;
 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebPluginDelegate);
};

#endif  // #ifndef WEBKIT_GLUE_WEBPLUGIN_DELEGATE_H__
