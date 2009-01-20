// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_list.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/mime_util.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/webkit_glue.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN)
#include "webkit/activex_shim/activex_shared.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#endif

namespace NPAPI
{

scoped_refptr<PluginList> PluginList::singleton_;

// Extra paths to search.
static std::vector<FilePath>* extra_plugin_paths_ = NULL;

PluginList* PluginList::Singleton() {
  if (singleton_.get() == NULL) {
    singleton_ = new PluginList();
    singleton_->LoadPlugins(false);
  }

  return singleton_;
}

void PluginList::AddExtraPluginPath(const FilePath& plugin_path) {
  DCHECK(!singleton_.get() || !singleton_->plugins_loaded_);

  if (!extra_plugin_paths_)
    extra_plugin_paths_ = new std::vector<FilePath>;
  extra_plugin_paths_->push_back(plugin_path);
}

PluginList::PluginList() :
    plugins_loaded_(false) {
  PlatformInit();
}

void PluginList::LoadPlugins(bool refresh) {
  if (plugins_loaded_ && !refresh)
    return;

  plugins_.clear();
  plugins_loaded_ = true;

  base::TimeTicks start_time = base::TimeTicks::Now();

  LoadInternalPlugins();

  std::vector<FilePath> directories_to_scan;
  GetPluginDirectories(&directories_to_scan);

  for (size_t i = 0; i < directories_to_scan.size(); ++i) {
    LoadPluginsFromDir(directories_to_scan[i]);
  }

  if (extra_plugin_paths_) {
    for (size_t i = 0; i < extra_plugin_paths_->size(); ++i)
      LoadPlugin((*extra_plugin_paths_)[i]);
  }

  if (webkit_glue::IsDefaultPluginEnabled())
    LoadPlugin(FilePath(kDefaultPluginLibraryName));

  base::TimeTicks end_time = base::TimeTicks::Now();
  base::TimeDelta elapsed = end_time - start_time;
  DLOG(INFO) << "Loaded plugin list in " << elapsed.InMilliseconds() << " ms.";
}

void PluginList::LoadPlugin(const FilePath &path) {
  WebPluginInfo plugin_info;
  NP_GetEntryPointsFunc np_getentrypoints;
  NP_InitializeFunc np_initialize;
  NP_ShutdownFunc np_shutdown;
  if (!PluginLib::ReadWebPluginInfo(path, &plugin_info, &np_getentrypoints,
                                    &np_initialize, &np_shutdown)) {
    return;
  }

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

