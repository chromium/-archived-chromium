// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_lib.h"

#include "base/file_version_info.h"
#include "base/path_service.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace NPAPI
{

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

bool PluginLib::ReadWebPluginInfo(const FilePath &filename,
                                  WebPluginInfo* info) {
  // On windows, the way we get the mime types for the library is
  // to check the version information in the DLL itself.  This
  // will be a string of the format:  <type1>|<type2>|<type3>|...
  // For example:
  //     video/quicktime|audio/aiff|image/jpeg
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(filename.value()));
  if (!version_info.get())
    return false;

  PluginVersionInfo pvi;
  pvi.mime_types = version_info->GetStringValue(L"MIMEType");
  pvi.file_extensions = version_info->GetStringValue(L"FileExtents");
  pvi.type_descriptions = version_info->GetStringValue(L"FileOpenName");
  pvi.product_name = version_info->product_name();
  pvi.file_description = version_info->file_description();
  pvi.file_version = version_info->file_version();
  pvi.path = filename;

  return PluginList::CreateWebPluginInfo(pvi, info);
}

}  // namespace NPAPI
