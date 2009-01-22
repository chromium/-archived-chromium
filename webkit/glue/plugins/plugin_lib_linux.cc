// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_lib.h"

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace NPAPI {

// static
PluginLib::NativeLibrary PluginLib::LoadNativeLibrary(
    const FilePath& library_path) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
void PluginLib::UnloadNativeLibrary(NativeLibrary library) {
  NOTIMPLEMENTED();
}

// static
void* PluginLib::GetFunctionPointerFromNativeLibrary(
    NativeLibrary library,
    NativeLibraryFunctionNameType name) {
  NOTIMPLEMENTED();
  return NULL;
}

bool PluginLib::ReadWebPluginInfo(const FilePath& filename,
                                  WebPluginInfo* info,
                                  NP_GetEntryPointsFunc* np_getentrypoints,
                                  NP_InitializeFunc* np_initialize,
                                  NP_ShutdownFunc* np_shutdown) {
  *np_getentrypoints = NULL;
  *np_initialize = NULL;
  *np_shutdown = NULL;

  return false;
}

}  // namespace NPAPI
