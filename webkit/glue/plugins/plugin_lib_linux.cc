// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_lib.h"

#include <dlfcn.h>
#include <errno.h>
#include <string.h>

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace NPAPI {

// static
PluginLib::NativeLibrary PluginLib::LoadNativeLibrary(
    const FilePath& library_path) {
  void* dl = dlopen(library_path.value().c_str(), RTLD_LAZY);
  if (!dl)
    NOTREACHED() << "dlopen failed: " << strerror(errno);

  return dl;
}

// static
void PluginLib::UnloadNativeLibrary(NativeLibrary library) {
  int ret = dlclose(library);
  if (ret < 0)
    NOTREACHED() << "dlclose failed: " << strerror(errno);
}

// static
void* PluginLib::GetFunctionPointerFromNativeLibrary(
    NativeLibrary library,
    NativeLibraryFunctionNameType name) {
  return dlsym(library, name);
}

bool PluginLib::ReadWebPluginInfo(const FilePath& filename,
                                  WebPluginInfo* info,
                                  NP_GetEntryPointsFunc* np_getentrypoints,
                                  NP_InitializeFunc* np_initialize,
                                  NP_ShutdownFunc* np_shutdown) {
  // The file to reference is:
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginsDirUnix.cpp

  *np_getentrypoints = NULL;
  *np_initialize = NULL;
  *np_shutdown = NULL;

  void* dl = LoadNativeLibrary(filename);
  if (!dl)
    return false;

  void* ns_get_factory = GetFunctionPointerFromNativeLibrary(dl,
                                                             "NSGetFactory");

  if (ns_get_factory) {
    // Mozilla calls this an "almost-new-style plugin", then proceeds to
    // poke at it via XPCOM.  Our testing plugin doesn't use it.
    NOTIMPLEMENTED() << ": " << filename.value()
                     << " is an \"almost-new-style plugin\".";
    UnloadNativeLibrary(dl);
    return false;
  }

  // See comments in plugin_lib_mac regarding this symbol.
  typedef const char* (*GetMimeDescriptionType)();
  GetMimeDescriptionType ns_get_mime_description =
      reinterpret_cast<GetMimeDescriptionType>(
          GetFunctionPointerFromNativeLibrary(dl, "NP_GetMIMEDescription"));
  const char* description = "";
  if (ns_get_mime_description)
    description = ns_get_mime_description();

  // TODO(port): pick up from here.
  LOG(INFO) << description;
  NOTIMPLEMENTED();
  UnloadNativeLibrary(dl);
  return false;
}

}  // namespace NPAPI
