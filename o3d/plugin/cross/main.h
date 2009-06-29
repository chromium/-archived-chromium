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


// This header is used by the platform-specific portions of the plugin
// main implementation to define the cross-platform parts of the
// interface and global variables.

#ifndef O3D_PLUGIN_CROSS_MAIN_H_
#define O3D_PLUGIN_CROSS_MAIN_H_

#include <npupp.h>
#include <stdio.h>

#include <fstream>
#include <iostream>

#include "breakpad/win/exception_handler_win32.h"
#include "core/cross/renderer.h"
#include "core/cross/renderer_platform.h"
#include "plugin/cross/o3d_glue.h"
#include "plugin/cross/config.h"
#include "plugin/cross/stream_manager.h"
#include "third_party/nixysa/files/static_glue/npapi/common.h"
#include "third_party/nixysa/files/static_glue/npapi/npn_api.h"

extern ExceptionManager *g_exception_manager;

class RenderOnDemandCallbackHandler
    : public o3d::Client::RenderOnDemandCallback {
 public:
  explicit RenderOnDemandCallbackHandler(glue::_o3d::PluginObject* obj)
      : obj_(obj) {
  }

  // This function is implemented for each platform.
  virtual void Run();
 private:
  glue::_o3d::PluginObject* obj_;
};

// BreakpadEnabler is a simple class to keep track of whether or not
// we're executing code that we want to handle crashes for
// (when the o3d plugin is running in Firefox, we don't want to handle
// crashes for the Flash plugin or Firefox, just the o3d code)
// Create a stack-based instance at the start of each function
// where crash handling is desired.

#define HANDLE_CRASHES   BreakpadEnabler enabler

class BreakpadEnabler {
 public:
  BreakpadEnabler() {
    ++scope_count_;
  }

  virtual ~BreakpadEnabler() {
    --scope_count_;
  }

  static bool IsEnabled() { return scope_count_ > 0; }

 private:
  static int  scope_count_;
};


namespace o3d {
void WriteLogString(const char* text, int length);
}  // end namespace o3d

NPError PlatformNPPGetValue(NPP instance, NPPVariable variable, void *value);

// NPAPI declarations.  Some of these are only implemented in the
// platform-specific versions of "main.cc".

extern "C" {
  NPError OSCALL NP_Shutdown(void);
  NPError OSCALL NP_GetEntryPoints(NPPluginFuncs *pluginFuncs);
  NPError NPP_Destroy(NPP instance, NPSavedData **save);
  NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason);
  NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value);

  NPError NPP_New(NPMIMEType pluginType,
                  NPP instance,
                  uint16 mode,
                  int16 argc,
                  char *argn[],
                  char *argv[],
                  NPSavedData *saved);

  NPError NPP_NewStream(NPP instance,
                        NPMIMEType type,
                        NPStream *stream,
                        NPBool seekable,
                        uint16 *stype);

  NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value);
  NPError NPP_SetWindow(NPP instance, NPWindow *window);

  int32 NPP_Write(NPP instance,
                  NPStream *stream,
                  int32 offset,
                  int32 len,
                  void *buffer);

  int32 NPP_WriteReady(NPP instance, NPStream *stream);
  void NPP_Print(NPP instance, NPPrint *platformPrint);
  void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname);

  void NPP_URLNotify(NPP instance,
                     const char *url,
                     NPReason reason,
                     void *notifyData);
};  // end extern "C"

#endif  // O3D_PLUGIN_CROSS_MAIN_H_
