// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBPLUGIN_DELEGATE_H_
#define WEBKIT_GLUE_WEBPLUGIN_DELEGATE_H_

#include <string>

#include "base/gfx/native_widget_types.h"
#include "build/build_config.h"
#include "third_party/npapi/bindings/npapi.h"

struct NPObject;

class FilePath;
class GURL;
class WebPlugin;
class WebPluginResourceClient;

namespace WebKit {
class WebInputEvent;
struct WebCursorInfo;
}

namespace gfx {
class Rect;
}

// This is the interface that a plugin implementation needs to provide.
class WebPluginDelegate {
 public:
  enum PluginQuirks {
    PLUGIN_QUIRK_SETWINDOW_TWICE = 1,  // Win32
    PLUGIN_QUIRK_THROTTLE_WM_USER_PLUS_ONE = 2,  // Win32
    PLUGIN_QUIRK_DONT_CALL_WND_PROC_RECURSIVELY = 4,  // Win32
    PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY = 8,  // Win32
    PLUGIN_QUIRK_DONT_ALLOW_MULTIPLE_INSTANCES = 16,  // Win32
    PLUGIN_QUIRK_DIE_AFTER_UNLOAD = 32,  // Win32
    PLUGIN_QUIRK_PATCH_SETCURSOR = 64,  // Win32
    PLUGIN_QUIRK_BLOCK_NONSTANDARD_GETURL_REQUESTS = 128,  // Win32
    PLUGIN_QUIRK_WINDOWLESS_OFFSET_WINDOW_TO_DRAW = 256,  // Linux
  };

  WebPluginDelegate() {}
  virtual ~WebPluginDelegate() {}

  static WebPluginDelegate* Create(const FilePath& filename,
                                   const std::string& mime_type,
                                   gfx::PluginWindowHandle containing_view);

  // Initializes the plugin implementation with the given (UTF8) arguments.
  // Note that the lifetime of WebPlugin must be longer than this delegate.
  // If this function returns false the plugin isn't started and shouldn't be
  // called again.  If this method succeeds, then the WebPlugin is valid until
  // PluginDestroyed is called.
  // The load_manually parameter if true indicates that the plugin data would
  // be passed from webkit. if false indicates that the plugin should download
  // the data. This also controls whether the plugin is instantiated as a full
  // page plugin (NP_FULL) or embedded (NP_EMBED).
  virtual bool Initialize(const GURL& url, char** argn, char** argv,
                          int argc, WebPlugin* plugin, bool load_manually) = 0;

  // Called when the WebPlugin is being destroyed.  This is a signal to the
  // delegate that it should tear-down the plugin implementation and not call
  // methods on the WebPlugin again.
  virtual void PluginDestroyed() = 0;

  // Update the geometry of the plugin.  This is a request to move the
  // plugin, relative to its containing window, to the coords given by
  // window_rect.  Its contents should be clipped to the coords given
  // by clip_rect, which are relative to the origin of the plugin
  // window.  The clip_rect is in plugin-relative coordinates.
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect) = 0;

  // Tells the plugin to paint the damaged rect.  |context| is only used for
  // windowless plugins.
  virtual void Paint(gfx::NativeDrawingContext context,
                     const gfx::Rect& rect) = 0;

  // Tells the plugin to print itself.
  virtual void Print(gfx::NativeDrawingContext hdc) = 0;

  // Informs the plugin that it now has focus.
  virtual void SetFocus() = 0;

  // For windowless plugins, gives them a user event like mouse/keyboard.
  // Returns whether the event was handled.
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor) = 0;

  // Gets the NPObject associated with the plugin for scripting.
  virtual NPObject* GetPluginScriptableObject() = 0;

  // Receives notification about a resource load that the plugin initiated
  // for a frame.
  virtual void DidFinishLoadWithReason(NPReason reason) = 0;

  // Returns the process id of the process that is running the plugin.
  virtual int GetProcessId() = 0;

  // The result of the script execution is returned via this function.
  virtual void SendJavaScriptStream(const std::string& url,
                                    const std::wstring& result,
                                    bool success, bool notify_needed,
                                    intptr_t notify_data) = 0;

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

  // Only available after Initialize is called.
  virtual FilePath GetPluginPath() = 0;

  // Only supported when the plugin is the default plugin.
  virtual void InstallMissingPlugin() = 0;

  // Creates a WebPluginResourceClient instance and returns the same.
  virtual WebPluginResourceClient* CreateResourceClient(int resource_id,
                                                        const std::string &url,
                                                        bool notify_needed,
                                                        intptr_t notify_data,
                                                        intptr_t stream) = 0;

  // Notifies the delegate about a Get/Post URL request getting routed.
  virtual void URLRequestRouted(const std::string&url, bool notify_needed,
                                intptr_t notify_data) = 0;

  virtual bool IsWindowless() const;

  virtual const gfx::Rect& GetRect() const;

  virtual const gfx::Rect& GetClipRect() const;

  // Returns a combination of PluginQuirks.
  virtual int GetQuirks() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegate);
};

#endif  // #ifndef WEBKIT_GLUE_WEBPLUGIN_DELEGATE_H_
