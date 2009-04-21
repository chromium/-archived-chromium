// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/native_library.h"
#include "base/ref_counted.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/webplugin.h"

struct WebPluginInfo;

namespace NPAPI
{

class PluginInstance;

// A PluginLib is a single NPAPI Plugin Library, and is the lifecycle
// manager for new PluginInstances.
class PluginLib : public base::RefCounted<PluginLib> {
 public:
  static PluginLib* CreatePluginLib(const FilePath& filename);
  virtual ~PluginLib();

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".
  // Returns false if the library couldn't be found, or if it's not a plugin.
  static bool ReadWebPluginInfo(const FilePath& filename, WebPluginInfo* info);

  // Unloads all the loaded plugin libraries and cleans up the plugin map.
  static void UnloadAllPlugins();

  // Shuts down all loaded plugin instances.
  static void ShutdownAllPlugins();

  // Get the Plugin's function pointer table.
  NPPluginFuncs* functions();

  // Creates a new instance of this plugin.
  PluginInstance* CreateInstance(const std::string& mime_type);

  // Called by the instance when the instance is tearing down.
  void CloseInstance();

  // Gets information about this plugin and the mime types that it
  // supports.
  const WebPluginInfo& plugin_info() { return web_plugin_info_; }

  //
  // NPAPI functions
  //

  // NPAPI method to initialize a Plugin.
  // Initialize can be safely called multiple times
  NPError NP_Initialize();

  // NPAPI method to shutdown a Plugin.
  void NP_Shutdown(void);

  int instance_count() const { return instance_count_; }

 private:
  // Creates a new PluginLib.
  // |entry_points| is non-NULL for internal plugins.
  PluginLib(const WebPluginInfo& info,
            const PluginEntryPoints* entry_points);

  // Attempts to load the plugin from the library.
  // Returns true if it is a legitimate plugin, false otherwise
  bool Load();

  // Unloads the plugin library.
  void Unload();

  // Shutdown the plugin library.
  void Shutdown();

 private:
  bool internal_;  // Whether this an internal plugin.
  WebPluginInfo web_plugin_info_;  // supported mime types, description
  base::NativeLibrary library_;  // the opened library reference
  NPPluginFuncs plugin_funcs_;  // the struct of plugin side functions
  bool initialized_;  // is the plugin initialized
  NPSavedData *saved_data_;  // persisted plugin info for NPAPI
  int instance_count_;  // count of plugins in use

  // Function pointers to entry points into the plugin.
  PluginEntryPoints entry_points_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginLib);
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__
