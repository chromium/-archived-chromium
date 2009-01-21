// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_lib.h"

#include "base/file_version_info.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "webkit/activex_shim/npp_impl.h"
#include "webkit/default_plugin/plugin_main.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_list.h"

#ifdef GEARS_STATIC_LIB
// defined in gears/base/common/module.cc
NPError API_CALL Gears_NP_GetEntryPoints(NPPluginFuncs* funcs);
NPError API_CALL Gears_NP_Initialize(NPNetscapeFuncs* funcs);
NPError API_CALL Gears_NP_Shutdown(void);
#endif

namespace NPAPI
{

// This struct fully describes a plugin. For external plugins, it's read in from
// the version info of the dll; For internal plugins, it's predefined and
// includes addresses of entry functions.
struct PluginVersionInfo {
  const wchar_t* path;
  const wchar_t* product_name;
  const wchar_t* file_description;
  const wchar_t* file_version;
  const wchar_t* mime_types;
  const wchar_t* file_extents;
  const wchar_t* file_open_names;
  NP_GetEntryPointsFunc np_getentrypoints;
  NP_InitializeFunc np_initialize;
  NP_ShutdownFunc np_shutdown;
};

static const PluginVersionInfo g_internal_plugins[] = {
  {
    kActiveXShimFileName,
    L"ActiveX Plug-in",
    L"ActiveX Plug-in provides a shim to support ActiveX controls",
    L"1, 0, 0, 1",
    L"application/x-oleobject|application/oleobject",
    L"*|*",
    L"",
    activex_shim::ActiveX_Shim_NP_GetEntryPoints,
    activex_shim::ActiveX_Shim_NP_Initialize,
    activex_shim::ActiveX_Shim_NP_Shutdown
  },
  {
    kActiveXShimFileNameForMediaPlayer,
    kActiveXShimFileNameForMediaPlayer,
    L"Windows Media Player",
    L"1, 0, 0, 1",
    L"application/x-ms-wmp|application/asx|video/x-ms-asf-plugin|"
        L"application/x-mplayer2|video/x-ms-asf|video/x-ms-wm|audio/x-ms-wma|"
        L"audio/x-ms-wax|video/x-ms-wmv|video/x-ms-wvx",
    L"*|*|*|*|asf,asx,*|wm,*|wma,*|wax,*|wmv,*|wvx,*",
    L"",
    activex_shim::ActiveX_Shim_NP_GetEntryPoints,
    activex_shim::ActiveX_Shim_NP_Initialize,
    activex_shim::ActiveX_Shim_NP_Shutdown
  },
  {
    kDefaultPluginLibraryName,
    L"Default Plug-in",
    L"Provides functionality for installing third-party plug-ins",
    L"1, 0, 0, 1",
    L"*",
    L"",
    L"",
    default_plugin::NP_GetEntryPoints,
    default_plugin::NP_Initialize,
    default_plugin::NP_Shutdown
  },
#ifdef GEARS_STATIC_LIB
  {
    kGearsPluginLibraryName,
    L"Gears",
    L"Statically linked Gears",
    L"1, 0, 0, 1",
    L"application/x-googlegears",
    L"",
    L"",
    Gears_NP_GetEntryPoints,
    Gears_NP_Initialize,
    Gears_NP_Shutdown
  },
#endif
};

/* static */
PluginLib::NativeLibrary PluginLib::LoadNativeLibrary(
    const FilePath& library_path) {
  // Switch the current directory to the plugin directory as the plugin
  // may have dependencies on dlls in this directory.
  bool restore_directory = false;
  std::wstring current_directory;
  if (PathService::Get(base::DIR_CURRENT, &current_directory)) {
    FilePath plugin_path = library_path.DirName();
    if (!plugin_path.value().empty()) {
      PathService::SetCurrentDirectory(plugin_path.value());
      restore_directory = true;
    }
  }

  HMODULE module = LoadLibrary(library_path.value().c_str());
  if (restore_directory)
    PathService::SetCurrentDirectory(current_directory);

  return module;
}

/* static */
void PluginLib::UnloadNativeLibrary(NativeLibrary library) {
  FreeLibrary(library);
}

/* static */
void* PluginLib::GetFunctionPointerFromNativeLibrary(
    NativeLibrary library,
    NativeLibraryFunctionNameType name) {
  return GetProcAddress(library, name);
}

namespace {

// Creates WebPluginInfo structure based on read in or built in
// PluginVersionInfo.
bool CreateWebPluginInfo(const PluginVersionInfo& pvi,
                         WebPluginInfo* info,
                         NP_GetEntryPointsFunc* np_getentrypoints,
                         NP_InitializeFunc* np_initialize,
                         NP_ShutdownFunc* np_shutdown) {
  std::vector<std::string> mime_types, file_extensions;
  std::vector<std::wstring> descriptions;
  SplitString(base::SysWideToNativeMB(pvi.mime_types), '|', &mime_types);
  SplitString(base::SysWideToNativeMB(pvi.file_extents), '|', &file_extensions);
  SplitString(pvi.file_open_names, '|', &descriptions);

  info->mime_types.clear();

  if (mime_types.empty())
    return false;

  info->name = pvi.product_name;
  info->desc = pvi.file_description;
  info->version = pvi.file_version;
  info->path = FilePath(pvi.path);

  for (size_t i = 0; i < mime_types.size(); ++i) {
    WebPluginMimeType mime_type;
    mime_type.mime_type = StringToLowerASCII(mime_types[i]);
    if (file_extensions.size() > i)
      SplitString(file_extensions[i], ',', &mime_type.file_extensions);

    if (descriptions.size() > i) {
      mime_type.description = descriptions[i];

      // Remove the extension list from the description.
      size_t ext = mime_type.description.find(L"(*");
      if (ext != std::wstring::npos) {
        if (ext > 1 && mime_type.description[ext -1] == ' ')
          ext--;

        mime_type.description.erase(ext);
      }
    }

    info->mime_types.push_back(mime_type);
  }

  *np_getentrypoints = pvi.np_getentrypoints;
  *np_initialize = pvi.np_initialize;
  *np_shutdown = pvi.np_shutdown;

  return true;
}

}

bool PluginLib::ReadWebPluginInfo(const FilePath &filename,
                                  WebPluginInfo* info,
                                  NP_GetEntryPointsFunc* np_getentrypoints,
                                  NP_InitializeFunc* np_initialize,
                                  NP_ShutdownFunc* np_shutdown) {
  for (int i = 0; i < arraysize(g_internal_plugins); ++i) {
    if (filename.value() == g_internal_plugins[i].path) {
      return CreateWebPluginInfo(g_internal_plugins[i], info, np_getentrypoints,
                                 np_initialize, np_shutdown);
    }
  }

  // On windows, the way we get the mime types for the library is
  // to check the version information in the DLL itself.  This
  // will be a string of the format:  <type1>|<type2>|<type3>|...
  // For example:
  //     video/quicktime|audio/aiff|image/jpeg
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(filename.value()));
  if (!version_info.get())
    return false;

  std::wstring mime_types = version_info->GetStringValue(L"MIMEType");
  std::wstring file_extents = version_info->GetStringValue(L"FileExtents");
  std::wstring file_open_names = version_info->GetStringValue(L"FileOpenName");
  std::wstring product_name = version_info->product_name();
  std::wstring file_description = version_info->file_description();
  std::wstring file_version = version_info->file_version();
  std::wstring path = filename.value();

  PluginVersionInfo pvi;
  pvi.mime_types = mime_types.c_str();
  pvi.file_extents = file_extents.c_str();
  pvi.file_open_names = file_open_names.c_str();
  pvi.product_name = product_name.c_str();
  pvi.file_description = file_description.c_str();
  pvi.file_version = file_version.c_str();
  pvi.path = path.c_str();
  pvi.np_getentrypoints = NULL;
  pvi.np_initialize = NULL;
  pvi.np_shutdown = NULL;

  return CreateWebPluginInfo(
      pvi, info, np_getentrypoints, np_initialize, np_shutdown);
}

}  // namespace NPAPI

