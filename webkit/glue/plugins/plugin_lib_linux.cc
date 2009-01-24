// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_lib.h"

#include <dlfcn.h>

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "webkit/glue/plugins/plugin_list.h"

// These headers must be included in this order to make the declaration gods
// happy.
#include "base/third_party/nspr/prcpucfg_linux.h"
#include "third_party/mozilla/include/nsplugindefs.h"

namespace NPAPI {

// static
PluginLib::NativeLibrary PluginLib::LoadNativeLibrary(
    const FilePath& library_path) {
  void* dl = dlopen(library_path.value().c_str(), RTLD_LAZY);
  if (!dl)
    NOTREACHED() << "dlopen failed: " << dlerror();

  return dl;
}

// static
void PluginLib::UnloadNativeLibrary(NativeLibrary library) {
  int ret = dlclose(library);
  if (ret < 0)
    NOTREACHED() << "dlclose failed: " << dlerror();
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

  info->path = filename;

  // See comments in plugin_lib_mac regarding this symbol.
  typedef const char* (*NP_GetMimeDescriptionType)();
  NP_GetMimeDescriptionType NP_GetMIMEDescription =
      reinterpret_cast<NP_GetMimeDescriptionType>(
          dlsym(dl, "NP_GetMIMEDescription"));
  const char* mime_description = NULL;
  if (NP_GetMIMEDescription)
    mime_description = NP_GetMIMEDescription();

  if (mime_description) {
    // We parse the description here into WebPluginMimeType structures.
    // Description for Flash 10 looks like (all as one string):
    //   "application/x-shockwave-flash:swf:Shockwave Flash;"
    //   "application/futuresplash:spl:FutureSplash Player"
    std::vector<std::string> descriptions;
    SplitString(mime_description, ';', &descriptions);
    for (size_t i = 0; i < descriptions.size(); ++i) {
      std::vector<std::string> fields;
      SplitString(descriptions[i], ':', &fields);
      if (fields.size() != 3) {
        LOG(WARNING) << "Couldn't parse plugin info: " << descriptions[i];
        continue;
      }

      WebPluginMimeType mime_type;
      mime_type.mime_type = fields[0];
      SplitString(fields[1], ',', &mime_type.file_extensions);
      mime_type.description = ASCIIToWide(fields[2]);
      info->mime_types.push_back(mime_type);
    }
  }

  // The plugin name and description live behind NP_GetValue calls.
  typedef NPError (*NP_GetValueType)(void* unused,
                                     nsPluginVariable variable,
                                     void* value_out);
  NP_GetValueType NP_GetValue =
      reinterpret_cast<NP_GetValueType>(dlsym(dl, "NP_GetValue"));
  if (NP_GetValue) {
    const char* name = NULL;
    NP_GetValue(NULL, nsPluginVariable_NameString, &name);
    if (name)
      info->name = ASCIIToWide(name);

    const char* description = NULL;
    NP_GetValue(NULL, nsPluginVariable_DescriptionString, &description);
    if (description)
      info->desc = ASCIIToWide(description);
  }

  return true;
}

}  // namespace NPAPI
