/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// File declaring a class wrapping the raw npapi interface as exported from
// a Mozilla plug-in.

#ifndef O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_PLUGIN_PROXY_H_
#define O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_PLUGIN_PROXY_H_

#include <vector>
#include "third_party/npapi/files/include/npupp.h"

class NPBrowserProxy;
struct INPObjectProxy;
class StreamOperation;

typedef NPError (__stdcall *NP_InitializeFunc)(NPNetscapeFuncs* functions);
typedef NPError (__stdcall *NP_GetEntryPointsFunc)(NPPluginFuncs* functions);
typedef NPError (__stdcall *NP_ShutdownFunc)();

class NPPluginProxy {
 public:
  ~NPPluginProxy();

  // Initializes and binds this instance to the npapi plugin exported by
  // the given module.  Note that the object takes control of the lifetime of
  // module, and will unload it at instance destruction time.
  // Parameters:
  //  browser_proxy:  Browser environment in which the plug-in will reside.
  //  window:  NPWindow structure initialized for the plug-in.
  //  argument_names:  Array of string-argument names to be passed to the
  //    construction routine NPP_New.
  //  argument_values:  Array of string-argument values to be passed to the
  //    construction routine NPP_New.
  // Returns:
  //  true if the plugin successfully loaded and initialized in the provided
  //  window.
  bool Init(NPBrowserProxy* browser_proxy,
            const NPWindow& window,
            const std::vector<CStringA>& argument_names,
            const std::vector<CStringA>& argument_values);

  // Frees all resources allocated in Init, and blocks on all pending stream
  // operations.
  void TearDown();

  // Get the 'v-table' interface for the hosted plugin member functions.
  const NPPluginFuncs* GetPluginFunctions() const {
    return &plugin_funcs_;
  }

  // Get the plugin data associated with this instance.
  NPP_t* GetNPP() {
    return &npp_data_;
  }

  // Return the NPAPI object containing the scripting entry points for the
  // plugin.
  HRESULT GetScriptableObject(INPObjectProxy** scriptable_object) const;

  // Return a pointer to the NPAPI browser environment hosting the plugin.
  NPBrowserProxy* browser_proxy() const {
    return browser_proxy_;
  }

  // Registers stream_op with the list of active stream operations.
  void RegisterStreamOperation(StreamOperation* stream_op);

  // Removes stream_op from the set of active stream operations.
  void UnregisterStreamOperation(StreamOperation* stream_op);

  static HRESULT Create(NPPluginProxy** instance);

 private:
   // Basic constructor that does not perform any plugin-specific operations.
   // Simply prepares the structure for initialization.
   NPPluginProxy();

   // Stores pointers to the NPAPI entry points present in the passed in module.
   // This routine also performs one-time initialization of the plug-in,
   // but does not create a live instance.
   //  loaded_module:  Handle to a loaded module containing exports for a npapi
   //                  plugin.
   // Returns:
   //  true if the NPAPI plugin entry points were present in the module.
   bool MapEntryPoints(HMODULE loaded_module);

  // Pointer to the npapi browser environment in which the plugin lives.
  // A smart pointer is not used, as this is a back-pointer.
  NPBrowserProxy* browser_proxy_;

  // Cached scritable object for interacting with the plugin.
  CComPtr<INPObjectProxy> scriptable_object_;

  // Cache of plugin instance member functions.
  NPPluginFuncs plugin_funcs_;

  // Pointers to the three main entry points of the plug-in.
  NP_InitializeFunc NP_Initialize_;
  NP_GetEntryPointsFunc NP_GetEntryPoints_;
  NP_ShutdownFunc NP_Shutdown_;

  // The handle to the loaded plugin module.  The plugin unloads this module
  // upon destruction.
  HMODULE plugin_module_;

  // Plugin instance data passed to all plugin-calls.
  NPP_t npp_data_;

  typedef std::vector<StreamOperation*> StreamOpArray;

  // The set of currently pending/downloading streaming operations spawned
  // by the plugin.
  StreamOpArray active_stream_ops_;

  // Global count of the number of currently live plugin instances.  Used
  // to ensure that NP_Initialize and NP_Shutdown are called only once
  // per loading of the plugin module.
  static int kPluginInstanceCount;

  DISALLOW_COPY_AND_ASSIGN(NPPluginProxy);
};

#endif  // O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_PLUGIN_PROXY_H_
