// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_list.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/mime_util.h"
#include "webkit/default_plugin/plugin_main.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/webkit_glue.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN)
#include "webkit/activex_shim/activex_shared.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#endif

namespace NPAPI {

base::LazyInstance<PluginList> g_singleton(base::LINKER_INITIALIZED);

// static
PluginList* PluginList::Singleton() {
  PluginList* singleton = g_singleton.Pointer();
  if (!singleton->plugins_loaded_) {
    singleton->LoadPlugins(false);
    DCHECK(singleton->plugins_loaded_);
  }
  return singleton;
}

// static
void PluginList::ResetPluginsLoaded() {
  // We access the singleton directly, and not through Singleton(), since
  // we don't want LoadPlugins() to be called.
  g_singleton.Pointer()->plugins_loaded_ = false;
}

// static
void PluginList::AddExtraPluginPath(const FilePath& plugin_path) {
  DCHECK(!g_singleton.Pointer()->plugins_loaded_);
  g_singleton.Pointer()->extra_plugin_paths_.push_back(plugin_path);
}

// static
void PluginList::AddExtraPluginDir(const FilePath& plugin_dir) {
  DCHECK(!g_singleton.Pointer()->plugins_loaded_);
  g_singleton.Pointer()->extra_plugin_dirs_.push_back(plugin_dir);
}

void PluginList::RegisterInternalPlugin(const PluginVersionInfo& info) {
  DCHECK(!g_singleton.Pointer()->plugins_loaded_);
  g_singleton.Pointer()->internal_plugins_.push_back(info);
}

bool PluginList::ReadPluginInfo(const FilePath &filename,
                                WebPluginInfo* info,
                                const PluginEntryPoints** entry_points) {
  // We access the singleton directly, and not through Singleton(), since
  // we might be in a LoadPlugins call and don't want to call it recursively!
  const std::vector<PluginVersionInfo>& internal_plugins =
      g_singleton.Pointer()->internal_plugins_;
  for (size_t i = 0; i < internal_plugins.size(); ++i) {
    if (filename == internal_plugins[i].path) {
      *entry_points = &internal_plugins[i].entry_points;
      return CreateWebPluginInfo(internal_plugins[i], info);
    }
  }

  // Not an internal plugin.
  *entry_points = NULL;

  return PluginLib::ReadWebPluginInfo(filename, info);
}

bool PluginList::CreateWebPluginInfo(const PluginVersionInfo& pvi,
                                     WebPluginInfo* info) {
  std::vector<std::string> mime_types, file_extensions;
  std::vector<std::wstring> descriptions;
  SplitString(WideToUTF8(pvi.mime_types), '|', &mime_types);
  SplitString(WideToUTF8(pvi.file_extensions), '|', &file_extensions);
  SplitString(pvi.type_descriptions, '|', &descriptions);

  info->mime_types.clear();

  if (mime_types.empty())
    return false;

  info->name = pvi.product_name;
  info->desc = pvi.file_description;
  info->version = pvi.file_version;
  info->path = pvi.path;

  for (size_t i = 0; i < mime_types.size(); ++i) {
    WebPluginMimeType mime_type;
    mime_type.mime_type = StringToLowerASCII(mime_types[i]);
    if (file_extensions.size() > i)
      SplitString(file_extensions[i], ',', &mime_type.file_extensions);

    if (descriptions.size() > i) {
      mime_type.description = descriptions[i];

      // On Windows, the description likely has a list of file extensions
      // embedded in it (e.g. "SurfWriter file (*.swr)"). Remove an extension
      // list from the description if it is present.
      size_t ext = mime_type.description.find(L"(*");
      if (ext != std::wstring::npos) {
        if (ext > 1 && mime_type.description[ext -1] == ' ')
          ext--;

        mime_type.description.erase(ext);
      }
    }

    info->mime_types.push_back(mime_type);
  }

  return true;
}

PluginList::PluginList() : plugins_loaded_(false) {
  PlatformInit();

#if defined(OS_WIN)
  const PluginVersionInfo default_plugin = {
    FilePath(kDefaultPluginLibraryName),
    L"Default Plug-in",
    L"Provides functionality for installing third-party plug-ins",
    L"1",
    L"*",
    L"",
    L"",
    {
      default_plugin::NP_GetEntryPoints,
      default_plugin::NP_Initialize,
      default_plugin::NP_Shutdown
    }
  };

  internal_plugins_.push_back(default_plugin);
#endif
}

void PluginList::LoadPlugins(bool refresh) {
  if (plugins_loaded_ && !refresh)
    return;

  plugins_.clear();
  plugins_loaded_ = true;

  base::TimeTicks start_time = base::TimeTicks::Now();

  std::vector<FilePath> directories_to_scan;
  GetPluginDirectories(&directories_to_scan);

  for (size_t i = 0; i < extra_plugin_paths_.size(); ++i)
    LoadPlugin(extra_plugin_paths_[i]);

  for (size_t i = 0; i < extra_plugin_dirs_.size(); ++i) {
    LoadPluginsFromDir(extra_plugin_dirs_[i]);
  }

  for (size_t i = 0; i < directories_to_scan.size(); ++i) {
    LoadPluginsFromDir(directories_to_scan[i]);
  }

  LoadInternalPlugins();

  if (webkit_glue::IsDefaultPluginEnabled())
    LoadPlugin(FilePath(kDefaultPluginLibraryName));

  base::TimeTicks end_time = base::TimeTicks::Now();
  base::TimeDelta elapsed = end_time - start_time;
  DLOG(INFO) << "Loaded plugin list in " << elapsed.InMilliseconds() << " ms.";
}

void PluginList::LoadPlugin(const FilePath &path) {
  WebPluginInfo plugin_info;
  const PluginEntryPoints* entry_points;

  if (!ReadPluginInfo(path, &plugin_info, &entry_points))
    return;

  if (!ShouldLoadPlugin(plugin_info))
    return;

  if (path.value() != kDefaultPluginLibraryName
#if defined(OS_WIN) && !defined(NDEBUG)
      && path.BaseName().value() != L"npspy.dll"  // Make an exception for NPSPY
#endif
      ) {
    for (size_t i = 0; i < plugin_info.mime_types.size(); ++i) {
      // TODO: don't load global handlers for now.
      // WebKit hands to the Plugin before it tries
      // to handle mimeTypes on its own.
      const std::string &mime_type = plugin_info.mime_types[i].mime_type;
      if (mime_type == "*" )
        return;
    }
  }

  plugins_.push_back(plugin_info);
}

bool PluginList::FindPlugin(const std::string& mime_type,
                            const std::string& clsid,
                            bool allow_wildcard,
                            WebPluginInfo* info) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));

  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (SupportsType(plugins_[i], mime_type, allow_wildcard)) {
#if defined(OS_WIN)
      if (!clsid.empty() && plugins_[i].path.value() == kActiveXShimFileName) {
        // Special handling for ActiveX shim. If ActiveX is not installed, we
        // should use the default plugin to show the installation UI.
        if (!activex_shim::IsActiveXInstalled(clsid))
          continue;
      }
#endif
      *info = plugins_[i];
      return true;
    }
  }

  return false;
}

bool PluginList::FindPlugin(const GURL &url, std::string* actual_mime_type,
                            WebPluginInfo* info) {
  std::string path = url.path();
  std::string::size_type last_dot = path.rfind('.');
  if (last_dot == std::string::npos)
    return false;

  std::string extension = StringToLowerASCII(std::string(path, last_dot+1));

  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (SupportsExtension(plugins_[i], extension, actual_mime_type)) {
      *info = plugins_[i];
      return true;
    }
  }

  return false;
}

bool PluginList::SupportsType(const WebPluginInfo& info,
                              const std::string &mime_type,
                              bool allow_wildcard) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (size_t i = 0; i < info.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_info = info.mime_types[i];
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (!allow_wildcard && mime_info.mime_type == "*") {
        continue;
      }
      return true;
    }
  }
  return false;
}

bool PluginList::SupportsExtension(const WebPluginInfo& info,
                                   const std::string &extension,
                                   std::string* actual_mime_type) {
  for (size_t i = 0; i < info.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_type = info.mime_types[i];
    for (size_t j = 0; j < mime_type.file_extensions.size(); ++j) {
      if (mime_type.file_extensions[j] == extension) {
        if (actual_mime_type)
          *actual_mime_type = mime_type.mime_type;
        return true;
      }
    }
  }

  return false;
}


bool PluginList::GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  if (refresh)
    LoadPlugins(true);

  *plugins = plugins_;

  return true;
}

bool PluginList::GetPluginInfo(const GURL& url,
                               const std::string& mime_type,
                               const std::string& clsid,
                               bool allow_wildcard,
                               WebPluginInfo* info,
                               std::string* actual_mime_type) {
  bool found = FindPlugin(mime_type,
                          clsid,
                          allow_wildcard, info);
  if (!found
      || (info->path.value() == kDefaultPluginLibraryName
#if defined(OS_WIN)
                                                          && clsid.empty()
#endif
                                                                          )) {
    WebPluginInfo info2;
    if (FindPlugin(url, actual_mime_type, &info2)) {
      found = true;
      *info = info2;
    }
  }

  return found;
}

bool PluginList::GetPluginInfoByPath(const FilePath& plugin_path,
                                     WebPluginInfo* info) {
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (plugins_[i].path == plugin_path) {
      *info = plugins_[i];
      return true;
    }
  }

  return false;
}

void PluginList::Shutdown() {
  // TODO
}

} // namespace NPAPI
