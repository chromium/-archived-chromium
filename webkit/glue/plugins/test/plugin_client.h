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

#ifndef WEBKIT_PORT_PLUGINS_TEST_PLUGIN_CLIENT_H__
#define WEBKIT_PORT_PLUGINS_TEST_PLUGIN_CLIENT_H__

#include "webkit/glue/plugins/nphostapi.h"
#include "third_party/npapi/bindings/npapi.h"

namespace NPAPIClient {

// A PluginClient is a NPAPI Plugin.  This class contains
// the bootstrapping functions used by the browser to load
// the plugin.
class PluginClient {
 public:
  // Although not documented in the NPAPI specification, this function
  // gets the list of entry points in the NPAPI Plugin (client) for the
  // NPAPI Host to call.
  static NPError GetEntryPoints(NPPluginFuncs* pFuncs);

  // The browser calls this function only once: when a plug-in is loaded,
  // before the first instance is created. This is the first function that
  // the browser calls. NP_Initialize tells the plug-in that the browser has
  // loaded it and provides global initialization. Allocate any memory or
  // resources shared by all instances of your plug-in at this time.
  static NPError Initialize(NPNetscapeFuncs* pFuncs);

  // The browser calls this function once after the last instance of your
  // plug-in is destroyed, before unloading the plug-in library itself. Use
  // NP_Shutdown to delete any data allocated in NP_Initialize to be shared
  // by all instances of a plug-in.
  static NPError Shutdown();

  // The table of functions provided by the host.
  static NPNetscapeFuncs *HostFunctions() { return host_functions_; }

 private:
  static NPNetscapeFuncs* host_functions_;
};

} // namespace NPAPIClient

#endif  // WEBKIT_PORT_PLUGINS_TEST_PLUGIN_CLIENT_H__

