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

#include "config.h"

#include "webkit/glue/plugins/plugin_lib.h"

#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/task.h"
#include "webkit/activex_shim/npp_impl.h"
#include "webkit/default_plugin/plugin_main.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "net/base/mime_util.h"


namespace NPAPI
{

const wchar_t kPluginLibrariesLoadedCounter[] = L"PluginLibrariesLoaded";
const wchar_t kPluginInstancesActiveCounter[] = L"PluginInstancesActive";

PluginLib::PluginMap* PluginLib::loaded_libs_;

PluginLib* PluginLib::CreatePluginLib(const std::wstring& filename) {
  // We can only have one PluginLib object per plugin as it controls the per
  // instance function calls (i.e. NP_Initialize and NP_Shutdown).  So we keep
  // a (non-ref counted) map of PluginLib objects.
  if (!loaded_libs_)
    loaded_libs_ = new PluginMap();

  PluginMap::const_iterator iter = loaded_libs_->find(filename);
  if (iter != loaded_libs_->end())
    return iter->second;

  static const InternalPluginInfo activex_shim_info = {
    {kActiveXShimFileName,
     L"ActiveX Plug-in",
     L"ActiveX Plug-in provides a shim to support ActiveX controls",
     L"1, 0, 0, 1",
     L"application/x-oleobject|application/oleobject|"
         L"application/x-ms-wmp|application/asx|video/x-ms-asf-plugin|"
         L"application/x-mplayer2|video/x-ms-asf|video/x-ms-wm|audio/x-ms-wma|"
         L"audio/x-ms-wax|video/x-ms-wmv|video/x-ms-wvx",
     L"*|*|*|*|*|*|asf,asx,*|wm,*|wma,*|wax,*|wmv,*|wvx,*",
     L""
    },
    activex_shim::ActiveX_Shim_NP_GetEntryPoints,
    activex_shim::ActiveX_Shim_NP_Initialize,
    activex_shim::ActiveX_Shim_NP_Shutdown
  };

  static const InternalPluginInfo default_null_plugin_info = {
    {kDefaultPluginDllName,
     L"Default Plug-in",
     L"Provides functionality for installing third-party plug-ins",
     L"1, 0, 0, 1",
     L"*",
     L"",
     L""
    },
    default_plugin::NP_GetEntryPoints,
    default_plugin::NP_Initialize,
    default_plugin::NP_Shutdown
  };

  WebPluginInfo* info = NULL;
  const InternalPluginInfo* internal_plugin_info = NULL;
  if (filename == activex_shim_info.version_info.file_name) {
    info = CreateWebPluginInfo(activex_shim_info.version_info);
    internal_plugin_info = &activex_shim_info;
  } else if (filename == default_null_plugin_info.version_info.file_name) {
    info = CreateWebPluginInfo(default_null_plugin_info.version_info);
    internal_plugin_info = &default_null_plugin_info;
  } else {
    info = ReadWebPluginInfo(filename);
    if (!info) {
      DLOG(INFO) << "This file isn't a valid NPAPI plugin: " << filename;
      return NULL;
    }
  }

  return new PluginLib(info, internal_plugin_info);
}

void PluginLib::UnloadAllPlugins() {
  if (loaded_libs_) {
    PluginMap::iterator lib_index;
    for (lib_index = loaded_libs_->begin(); lib_index != loaded_libs_->end();
            ++lib_index) {
      lib_index->second->Unload();
    }
    delete loaded_libs_;
    loaded_libs_ = NULL;
  }
}

void PluginLib::ShutdownAllPlugins() {
  if (loaded_libs_) {
    PluginMap::iterator lib_index;
    for (lib_index = loaded_libs_->begin(); lib_index != loaded_libs_->end();
            ++lib_index) {
      lib_index->second->Shutdown();
    }
  }
}

PluginLib::PluginLib(WebPluginInfo* info,
                     const InternalPluginInfo* internal_plugin_info)
    : web_plugin_info_(info),
      module_(0),
      initialized_(false),
      saved_data_(0),
      instance_count_(0) {
  StatsCounter(kPluginLibrariesLoadedCounter).Increment();
  memset((void*)&plugin_funcs_, 0, sizeof(plugin_funcs_));

  (*loaded_libs_)[info->file] = this;
  if (internal_plugin_info) {
    internal_ = true;
    NP_Initialize_ = internal_plugin_info->np_initialize;
    NP_GetEntryPoints_ = internal_plugin_info->np_getentrypoints;
    NP_Shutdown_ = internal_plugin_info->np_shutdown;
  } else {
    internal_ = false;
  }
}

PluginLib::~PluginLib() {
  StatsCounter(kPluginLibrariesLoadedCounter).Decrement();
  if (saved_data_ != 0) {
    // TODO - delete the savedData object here
  }
}

NPPluginFuncs *PluginLib::functions() {
  return &plugin_funcs_;
}

bool PluginLib::SupportsType(const std::string &mime_type,
                             bool allow_wildcard) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (size_t i = 0; i < web_plugin_info_->mime_types.size(); ++i) {
    const WebPluginMimeType& mime_info = web_plugin_info_->mime_types[i];
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (!allow_wildcard && (mime_info.mime_type == "*")) {
        continue;
      }
      return true;
    }
  }
  return false;
}

NPError PluginLib::NP_Initialize() {
  if (initialized_)
    return NPERR_NO_ERROR;

  if (!Load())
    return NPERR_MODULE_LOAD_FAILED_ERROR;

  PluginHost *host = PluginHost::Singleton();
  if (host == 0)
    return NPERR_GENERIC_ERROR;

  NPError rv = NP_Initialize_(host->host_functions());
  initialized_ = (rv == NPERR_NO_ERROR);
  return rv;
}

void PluginLib::NP_Shutdown(void) {
  DCHECK(initialized_);
  NP_Shutdown_();
}

PluginInstance *PluginLib::CreateInstance(const std::string &mime_type) {
  PluginInstance *new_instance = new PluginInstance(this, mime_type);
  instance_count_++;
  StatsCounter(kPluginInstancesActiveCounter).Increment();
  DCHECK(new_instance != 0);
  return new_instance;
}

void PluginLib::CloseInstance() {
  StatsCounter(kPluginInstancesActiveCounter).Decrement();
  instance_count_--;
  // If a plugin is running in its own process it will get unloaded on process
  // shutdown.
  if ((instance_count_ == 0) &&
       webkit_glue::IsPluginRunningInRendererProcess()) {
    Unload();
    loaded_libs_->erase(web_plugin_info_->file);
    if (loaded_libs_->empty()) {
      delete loaded_libs_;
      loaded_libs_ = NULL;
    }
  }
}

bool PluginLib::Load() {
  bool rv = false;
  HMODULE module = 0;

  if (!internal_) {
    if (module_ != 0)
      return rv;

    module = LoadPluginHelper(web_plugin_info_->file);
    if (module == 0)
      return rv;

    rv = true;  // assume success now

    NP_Initialize_ = (NP_InitializeFunc)GetProcAddress(
        module, "NP_Initialize");
    if (NP_Initialize_ == 0)
      rv = false;

    NP_GetEntryPoints_ = (NP_GetEntryPointsFunc)GetProcAddress(
        module, "NP_GetEntryPoints");
    if (NP_GetEntryPoints_ == 0)
      rv = false;

    NP_Shutdown_ = (NP_ShutdownFunc)GetProcAddress(
        module, "NP_Shutdown");
    if (NP_Shutdown_ == 0)
      rv = false;
  } else {
    rv = true;
  }

  if (rv) {
    plugin_funcs_.size = sizeof(plugin_funcs_);
    plugin_funcs_.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    if (NP_GetEntryPoints_(&plugin_funcs_) != NPERR_NO_ERROR)
      rv = false;
  }

  if (!internal_) {
    if (rv)
      module_ = module;
    else
      FreeLibrary(module);
  }

  return rv;
}

HMODULE PluginLib::LoadPluginHelper(const std::wstring plugin_file) {
  // Switch the current directory to the plugin directory as the plugin
  // may have dependencies on dlls in this directory.
  bool restore_directory = false;
  std::wstring current_directory;
  if (PathService::Get(base::DIR_CURRENT, &current_directory)) {
    std::wstring plugin_path = file_util::GetDirectoryFromPath(
        plugin_file);
    if (!plugin_path.empty()) {
      PathService::SetCurrentDirectory(plugin_path);
      restore_directory = true;
    }
  }

  HMODULE module = LoadLibrary(plugin_file.c_str());
  if (restore_directory)
    PathService::SetCurrentDirectory(current_directory);

  return module;
}

// This class implements delayed NP_Shutdown and FreeLibrary on the plugin dll.
class FreePluginLibraryTask : public Task {
 public:
  FreePluginLibraryTask(HMODULE module, NP_ShutdownFunc shutdown_func)
      : module_(module),
        NP_Shutdown_(shutdown_func) {
  }

  ~FreePluginLibraryTask() {}

  void Run() {
    if (NP_Shutdown_)
      NP_Shutdown_();

    if (module_) {
      FreeLibrary(module_);
      module_ = NULL;
    }
  }

 private:
  HMODULE module_;
  NP_ShutdownFunc NP_Shutdown_;
  DISALLOW_EVIL_CONSTRUCTORS(FreePluginLibraryTask);
};

void PluginLib::Unload() {
  if (!internal_ && module_) {
    // In case of single process mode, a plugin can delete itself
    // by executing a script. So delay the unloading of the DLL
    // so that the plugin will have a chance to unwind.
    bool defer_unload = webkit_glue::IsPluginRunningInRendererProcess();

#if USE(JAVASCRIPTCORE_BINDINGS)
    // The plugin NPAPI instances may still be around. Delay the
    // NP_Shutdown and FreeLibrary calls at least till the next
    // peek message.
    defer_unload = true;
#endif

    if (defer_unload) {
      FreePluginLibraryTask* free_library_task =
          new FreePluginLibraryTask(module_, NP_Shutdown_);
      MessageLoop::current()->PostTask(FROM_HERE, free_library_task);
    } else {
      Shutdown();
      FreeLibrary(module_);
    }

    module_ = 0;
  }
}

void PluginLib::Shutdown() {
  if (initialized_ && !internal_) {
    NP_Shutdown();
    initialized_ = false;
  }
}

WebPluginInfo* PluginLib::CreateWebPluginInfo(const PluginVersionInfo& pvi) {
  std::vector<std::string> mime_types, file_extensions;
  std::vector<std::wstring> descriptions;
  SplitString(WideToNativeMB(pvi.mime_types), '|', &mime_types);
  SplitString(WideToNativeMB(pvi.file_extents), '|', &file_extensions);
  SplitString(pvi.file_open_names, '|', &descriptions);

  if (mime_types.empty())
    return NULL;

  WebPluginInfo *info = new WebPluginInfo();
  info->name = pvi.product_name;
  info->desc = pvi.file_description;
  info->version = pvi.file_version;
  info->file = StringToLowerASCII(pvi.file_name);

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

  return info;
}

WebPluginInfo* PluginLib::ReadWebPluginInfo(const std::wstring &filename) {
  // On windows, the way we get the mime types for the library is
  // to check the version information in the DLL itself.  This
  // will be a string of the format:  <type1>|<type2>|<type3>|...
  // For example:
  //     video/quicktime|audio/aiff|image/jpeg
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(filename));
  if (!version_info.get())
    return NULL;

  PluginVersionInfo pvi;
  version_info->GetValue(L"MIMEType", &pvi.mime_types);
  version_info->GetValue(L"FileExtents", &pvi.file_extents);
  version_info->GetValue(L"FileOpenName", &pvi.file_open_names);
  pvi.product_name = version_info->product_name();
  pvi.file_description = version_info->file_description();
  pvi.file_version = version_info->file_version();
  pvi.file_name = filename;

  return CreateWebPluginInfo(pvi);
}

}  // namespace NPAPI
