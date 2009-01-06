// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__

#include "build/build_config.h"

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/plugins/nphostapi.h"
#include "third_party/npapi/bindings/npapi.h"

struct WebPluginInfo;

namespace NPAPI
{

class PluginInstance;

// This struct fully describes a plugin. For dll plugins, it's read in from
// the version info of the dll; For internal plugins, it's predefined.
struct PluginVersionInfo {
  std::wstring file_name;
  std::wstring product_name;
  std::wstring file_description;
  std::wstring file_version;
  std::wstring mime_types;
  std::wstring file_extents;
  std::wstring file_open_names;
};

// This struct contains information of an internal plugin and addresses of
// entry functions.
struct InternalPluginInfo {
  PluginVersionInfo version_info;
  NP_GetEntryPointsFunc np_getentrypoints;
  NP_InitializeFunc np_initialize;
  NP_ShutdownFunc np_shutdown;
};

// A PluginLib is a single NPAPI Plugin Library, and is the lifecycle
// manager for new PluginInstances.
class PluginLib : public base::RefCounted<PluginLib> {
 public:
  virtual ~PluginLib();
  static PluginLib* CreatePluginLib(const FilePath& filename);

  // Unloads all the loaded plugin dlls and cleans up the plugin map.
  static void UnloadAllPlugins();

  // Shuts down all loaded plugin instances.
  static void ShutdownAllPlugins();

  // Get the Plugin's function pointer table.
  NPPluginFuncs *functions();

  // Returns true if this Plugin supports a given mime-type.
  // mime_type should be all lower case.
  bool SupportsType(const std::string &mime_type, bool allow_wildcard);

  // Creates a new instance of this plugin.
  PluginInstance *CreateInstance(const std::string &mime_type);

  // Called by the instance when the instance is tearing down.
  void CloseInstance();

  // Gets information about this plugin and the mime types that it
  // supports.
  const WebPluginInfo& plugin_info() { return *web_plugin_info_; }

  //
  // NPAPI functions
  //

  // NPAPI method to initialize a Plugin.
  // Initialize can be safely called multiple times
  NPError NP_Initialize();

  // NPAPI method to shutdown a Plugin.
  void NP_Shutdown(void);

#if defined(OS_WIN)
  // Helper function to load a plugin.
  // Returns the module handle on success.
  static HMODULE LoadPluginHelper(const FilePath plugin_file);
#endif

  int instance_count() const { return instance_count_; }

 private:
  // Creates a new PluginLib.  The WebPluginInfo object is owned by this
  // object. If internal_plugin_info is not NULL, this Lib is an internal
  // plugin thus doesn't need to load dll.
  PluginLib(WebPluginInfo* info,
            const InternalPluginInfo* internal_plugin_info);

  // Attempts to load the plugin from the DLL.
  // Returns true if it is a legitimate plugin, false otherwise
  bool Load();

  // Unloading the plugin DLL.
  void Unload();

  // Shutdown the plugin DLL.
  void Shutdown();

  // Returns a WebPluginInfo structure given a plugin's path.  Returns NULL if
  // the dll couldn't be found, or if it's not a plugin.
  static WebPluginInfo* ReadWebPluginInfo(const FilePath &filename);
  // Creates WebPluginInfo structure based on read in or built in
  // PluginVersionInfo.
  static WebPluginInfo* CreateWebPluginInfo(const PluginVersionInfo& info);

  bool             internal_;         // Whether this an internal plugin.
  scoped_ptr<WebPluginInfo> web_plugin_info_;  // supported mime types, description
#if defined(OS_WIN)
  HMODULE          module_;           // the opened DLL handle
#endif
  NPPluginFuncs    plugin_funcs_;     // the struct of plugin side functions
  bool             initialized_;      // is the plugin initialized
  NPSavedData     *saved_data_;       // persisted plugin info for NPAPI
  int              instance_count_;   // count of plugins in use

  // A map of all the instantiated plugins.
  typedef base::hash_map<FilePath, scoped_refptr<PluginLib> > PluginMap;
  static PluginMap* loaded_libs_;

  // C-style function pointers
  NP_InitializeFunc       NP_Initialize_;
  NP_GetEntryPointsFunc   NP_GetEntryPoints_;
  NP_ShutdownFunc         NP_Shutdown_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginLib);
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__
