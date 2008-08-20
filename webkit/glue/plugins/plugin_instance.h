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

// TODO: Need to deal with NPAPI's NPSavedData.  
//       I haven't seen plugins use it yet.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_INSTANCE_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_INSTANCE_H__

#include <string>
#include <vector>
#include <stack>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread_local_storage.h"
#include "webkit/glue/plugins/nphostapi.h"
#include "googleurl/src/gurl.h"
#include "third_party/npapi/bindings/npapi.h"

class WebPlugin;
class MessageLoop;

namespace NPAPI
{
class PluginLib;
class PluginHost;
class PluginStream;
class PluginStreamUrl;
class PluginDataStream;
class MozillaExtensionApi;

// A PluginInstance is an active, running instance of a Plugin.
// A single plugin may have many PluginInstances.
class PluginInstance : public base::RefCounted<PluginInstance> {
 public:
  // Create a new instance of a plugin.  The PluginInstance
  // will hold a reference to the plugin.
  PluginInstance(PluginLib *plugin, const std::string &mime_type);
  virtual ~PluginInstance();

  // Activates the instance by calling NPP_New.
  // This should be called after our instance is all 
  // setup from the host side and we are ready to receive
  // requests from the plugin.  We must not call any 
  // functions on the plugin instance until start has 
  // been called.
  //
  // url: The instance URL.
  // param_names: the list of names of attributes passed via the 
  //       element.
  // param_values: the list of values corresponding to param_names
  // param_count: number of attributes
  // load_manually: if true indicates that the plugin data would be passed
  //                from webkit. if false indicates that the plugin should 
  //                download the data.
  //                This also controls whether the plugin is instantiated as
  //                a full page plugin (NP_FULL) or embedded (NP_EMBED)
  // 
  bool Start(const GURL& url,
             char** const param_names,
             char** const param_values,
             int param_count,
             bool load_manually);

  // NPAPI's instance identifier for this instance
  NPP npp() { return npp_; }

  // Get/Set for the instance's HWND.  
  HWND window_handle() { return hwnd_; }
  void set_window_handle(HWND value) { hwnd_ = value; }

  // Get/Set whether this instance is in Windowless mode.
  // Default is false.
  bool windowless() { return windowless_; }
  void set_windowless(bool value) { windowless_ = value; }

  // Get/Set whether this instance is transparent.  
  // This only applies to windowless plugins.  Transparent
  // plugins require that webkit paint the background.
  // Default is true.
  bool transparent() { return transparent_; }
  void set_transparent(bool value) { transparent_ = value; }

  // Get/Set the WebPlugin associated with this instance
  WebPlugin* webplugin() { return webplugin_; }
  void set_web_plugin(WebPlugin* webplugin) { webplugin_ = webplugin; }

  // Get the mimeType for this plugin stream
  const std::string &mime_type() { return mime_type_; }

  NPAPI::PluginLib* plugin_lib() { return plugin_; }

  // Handles a windows native message which this PluginInstance should deal
  // with.  Returns true if the event is handled, false otherwise.
  bool HandleEvent(UINT message, WPARAM wParam, LPARAM lParam);

  // Creates a stream for sending an URL.  If notify_needed
  // is true, it will send a notification to the plugin
  // when the stream is complete; otherwise it will not.
  // Set object_url to true if the load is for the object tag's
  // url, or false if it's for a url that the plugin
  // fetched through NPN_GetUrl[Notify].
  PluginStreamUrl *CreateStream(int resource_id,
                                const std::string &url, 
                                const std::string &mime_type, 
                                bool notify_needed,
                                void *notify_data);

  // Convenience function for sending a stream from a URL to this instance.
  // URL can be a relative or a fully qualified url. 
  void SendStream(const std::string& url, bool notify_needed,
                  void* notify_data);
  // For each instance, we track all streams.  When the
  // instance closes, all remaining streams are also 
  // closed.  All streams associated with this instance
  // should call AddStream so that they can be cleaned 
  // up when the instance shuts down.
  void AddStream(PluginStream* stream);

  // This is called when a stream is closed. We remove the stream from the 
  // list, which releases the reference maintained to the stream.
  void RemoveStream(PluginStream* stream);

  // Closes all open streams on this instance.  
  void CloseStreams();

  // Have the plugin create it's script object.
  NPObject *GetPluginScriptableObject();

  // WebViewDelegate methods that we implement. This is for handling
  // callbacks during getURLNotify.
  virtual void DidFinishLoadWithReason(NPReason reason);

  // Helper method to set some persistent data for getURLNotify since
  // resource fetches happen async.
  void SetURLLoadData(const GURL& url, void* notify_data);

  // If true, send the Mozilla user agent instead of Chrome's to the plugin.
  bool use_mozilla_user_agent() { return use_mozilla_user_agent_; }
  void set_use_mozilla_user_agent() { use_mozilla_user_agent_ = true; }

  bool throttle_invalidate() const { return throttle_invalidate_; }
  void set_throttle_invalidate(bool throttle_invalidate) {
    throttle_invalidate_ = throttle_invalidate;
  }

  // Helper that implements NPN_PluginThreadAsyncCall semantics
  void PluginThreadAsyncCall(void (*func)(void *),
                             void *userData);

  //
  // NPAPI methods for calling the Plugin Instance
  //
  NPError NPP_New(unsigned short, short, char *[], char *[]);
  NPError NPP_SetWindow(NPWindow *);
  NPError NPP_NewStream(NPMIMEType, NPStream *, NPBool, unsigned short *);
  NPError NPP_DestroyStream(NPStream *, NPReason);
  int NPP_WriteReady(NPStream *);
  int NPP_Write(NPStream *, int, int, void *);
  void NPP_StreamAsFile(NPStream *, const char *);
  void NPP_URLNotify(const char *, NPReason, void *);
  NPError NPP_GetValue(NPPVariable, void *);
  NPError NPP_SetValue(NPNVariable, void *);
  short NPP_HandleEvent(NPEvent *);
  void NPP_Destroy();
  bool NPP_Print(NPPrint* platform_print);

  void SendJavaScriptStream(const std::string& url, const std::wstring& result,
                            bool success, bool notify_needed, int notify_data);

  void DidReceiveManualResponse(const std::string& url,
                                const std::string& mime_type,
                                const std::string& headers,
                                uint32 expected_length,
                                uint32 last_modified);
  void DidReceiveManualData(const char* buffer, int length);
  void DidFinishManualLoading();
  void DidManualLoadFail();

  NPError GetServiceManager(void** service_manager);

  static PluginInstance* SetInitializingInstance(PluginInstance* instance);
  static PluginInstance* GetInitializingInstance();

  void PushPopupsEnabledState(bool enabled);
  void PopPopupsEnabledState();

  bool popups_allowed() const { 
    return popups_enabled_stack_.empty() ? false : popups_enabled_stack_.top();
  }

 private:
  void OnPluginThreadAsyncCall(void (*func)(void *),
                               void *userData);
  bool IsValidStream(const NPStream* stream);

  // This is a hack to get the real player plugin to work with chrome
  // The real player plugin dll(nppl3260) when loaded by firefox is loaded via
  // the NS COM API which is analogous to win32 COM. So the NPAPI functions in
  // the plugin are invoked via an interface by firefox. The plugin instance
  // handle which is passed to every NPAPI method is owned by the real player 
  // plugin, i.e. it expects the ndata member to point to a structure which
  // it knows about. Eventually it dereferences this structure and compares
  // a member variable at offset 0x24(Version 6.0.11.2888) /2D (Version
  // 6.0.11.3088) with 0 and on failing this check, takes  a different code
  // path which causes a crash. Safari and Opera work with version 6.0.11.2888
  // by chance as their ndata structure contains a 0 at the location which real
  // player checks:(. They crash with version 6.0.11.3088 as well. The 
  // following member just adds a 96 byte padding to our PluginInstance class 
  // which is passed in the ndata member. This magic number works correctly on 
  // Vista with UAC on or off :(. 
  // NOTE: Please dont change the ordering of the member variables 
  // New members should be added after this padding array.
  // TODO(iyengar) : Disassemble the Realplayer ndata structure and look into
  // the possiblity of conforming to it (http://b/issue?id=936667). We
  // could also log a bug with Real, which would save the effort.
  uint8                                    zero_padding_[96];
  scoped_refptr<NPAPI::PluginLib>          plugin_;
  NPP                                      npp_;
  scoped_refptr<PluginHost>                host_;
  NPPluginFuncs*                           npp_functions_;
  std::vector<scoped_refptr<PluginStream> > open_streams_;
  HWND                                     hwnd_;
  bool                                     windowless_;
  bool                                     transparent_;
  WebPlugin*                               webplugin_;
  std::string                              mime_type_;
  GURL                                     get_url_;
  void*                                    get_notify_data_;
  bool                                     use_mozilla_user_agent_;
  scoped_refptr<MozillaExtensionApi>       mozilla_extenstions_;
  MessageLoop*                             message_loop_;
  // Using TLS to store PluginInstance object during its creation.
  // We need to pass this instance to the service manager 
  // (MozillaExtensionApi) created as a result of NPN_GetValue
  // in the context of NP_Initialize. 
  static ThreadLocalStorage::Slot          plugin_instance_tls_index_;
  scoped_refptr<PluginDataStream>          plugin_data_stream_;
  GURL                                     instance_url_;

  // This flag if true indicates that the plugin data would be passed from
  // webkit. if false indicates that the plugin should download the data.
  bool                                     load_manually_;

  // This flag indicates if the NPN_InvalidateRect calls made by the
  // plugin need to be throttled.
  bool                                     throttle_invalidate_;

  // Stack indicating if popups are to be enabled for the outgoing
  // NPN_GetURL/NPN_GetURLNotify calls.
  std::stack<bool>                         popups_enabled_stack_;

  // True if in CloseStreams().
  bool in_close_streams_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginInstance);
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_INSTANCE_H__