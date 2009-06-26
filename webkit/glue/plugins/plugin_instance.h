// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Need to deal with NPAPI's NPSavedData.
//       I haven't seen plugins use it yet.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_INSTANCE_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_INSTANCE_H__

#include <string>
#include <vector>
#include <stack>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
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
class PluginInstance : public base::RefCountedThreadSafe<PluginInstance> {
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

  // Get/Set for the instance's window handle.
  gfx::NativeView window_handle() { return window_handle_; }
  void set_window_handle(gfx::NativeView value) { window_handle_ = value; }

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

#if defined(OS_WIN)
  // Handles a windows native message which this PluginInstance should deal
  // with.  Returns true if the event is handled, false otherwise.
  bool HandleEvent(UINT message, WPARAM wParam, LPARAM lParam);
#elif defined(OS_LINUX)
  bool HandleEvent(union _XEvent* event);
#endif

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
  void SetURLLoadData(const GURL& url, intptr_t notify_data);

  // If true, send the Mozilla user agent instead of Chrome's to the plugin.
  bool use_mozilla_user_agent() { return use_mozilla_user_agent_; }
  void set_use_mozilla_user_agent() { use_mozilla_user_agent_ = true; }

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
                            bool success, bool notify_needed,
                            intptr_t notify_data);

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

  // Initiates byte range reads for plugins.
  void RequestRead(NPStream* stream, NPByteRange* range_list);

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
  gfx::NativeView                          window_handle_;
  bool                                     windowless_;
  bool                                     transparent_;
  WebPlugin*                               webplugin_;
  std::string                              mime_type_;
  GURL                                     get_url_;
  intptr_t                                 get_notify_data_;
  bool                                     use_mozilla_user_agent_;
#if defined(OS_WIN)
  scoped_refptr<MozillaExtensionApi>       mozilla_extenstions_;
#endif
  MessageLoop*                             message_loop_;
  scoped_refptr<PluginStreamUrl>           plugin_data_stream_;
  GURL                                     instance_url_;

  // This flag if true indicates that the plugin data would be passed from
  // webkit. if false indicates that the plugin should download the data.
  bool                                     load_manually_;

  // Stack indicating if popups are to be enabled for the outgoing
  // NPN_GetURL/NPN_GetURLNotify calls.
  std::stack<bool>                         popups_enabled_stack_;

  // True if in CloseStreams().
  bool in_close_streams_;

  // List of files created for the current plugin instance. File names are
  // added to the list every time the NPP_StreamAsFile function is called.
  std::vector<FilePath> files_created_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginInstance);
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_INSTANCE_H__
