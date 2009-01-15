// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_list.h"

#include "base/file_util.h"
#include "base/mac_util.h"
#include "webkit/glue/plugins/plugin_lib.h"

namespace {

void GetPluginCommonDirectory(std::vector<FilePath>* plugin_dirs,
                              bool user) {
  // Note that there are no NSSearchPathDirectory constants for these
  // directories so we can't use Cocoa's NSSearchPathForDirectoriesInDomains().
  // Interestingly, Safari hard-codes the location (see
  // WebKit/WebKit/mac/Plugins/WebPluginDatabase.mm's +_defaultPlugInPaths).
  FSRef ref;	
  OSErr err = FSFindFolder(user ? kLocalDomain : kUserDomain,
                           kInternetPlugInFolderType, false, &ref);
  
  if (err)
    return;
 	
  plugin_dirs->push_back(FilePath(mac_util::PathFromFSRef(ref)));
}

void GetPluginPrivateDirectory(std::vector<FilePath>* plugin_dirs) {
  NSString* plugin_path = [[NSBundle mainBundle] builtInPlugInsPath];
  if (!plugin_path)
    return;
  
  plugin_dirs->push_back(FilePath([plugin_path fileSystemRepresentation]));
}

}  // namespace

namespace NPAPI
{

PluginList::PluginList() :
    plugins_loaded_(false) {
}

void PluginList::PlatformInit() {
}

void PluginList::GetPluginDirectories(std::vector<FilePath>* plugin_dirs) {
  // Load from the user's area
  GetPluginCommonDirectory(plugin_dirs, true);

  // Load from the machine-wide area
  GetPluginCommonDirectory(plugin_dirs, false);

  // Load any bundled plugins
  GetPluginPrivateDirectory(plugin_dirs);
}

void PluginList::LoadPluginsFromDir(const FilePath &path) {
  file_util::FileEnumerator enumerator(path,
                                       false, // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  for (FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    LoadPlugin(path);
  }
}

bool PluginList::ShouldLoadPlugin(const WebPluginInfo& info) {
  
  // Hierarchy check
  // (we're loading plugins hierarchically from Library folders, so plugins we
  //  encounter earlier must override plugins we encounter later)
  
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (plugins_[i].path.BaseName() == info.path.BaseName()) {
      return false;  // We already have a loaded plugin higher in the hierarchy.
    }
  }
  
  return true;
}

void PluginList::LoadInternalPlugins() {
  // none for now
}

} // namespace NPAPI

