// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__

#include "build/build_config.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/plugins/nphostapi.h"
#include "webkit/glue/webplugin.h"
#include "third_party/npapi/bindings/npapi.h"

struct WebPluginInfo;

namespace NPAPI
{

class PluginInstance;

// This struct fully describes a plugin. For external plugins, it's read in from
// the version info of the dll; For internal plugins, it's predefined.
struct PluginVersionInfo {
  FilePath path;
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
  static PluginLib* CreatePluginLib(const FilePath& filename);
  virtual ~PluginLib();

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".  Returns false if
  // the library couldn't be found, or if it's not a plugin.  
  static bool ReadWebPluginInfo(const FilePath &filename, WebPluginInfo* info);

  // Unloads all the loaded plugin libraries and cleans up the plugin map.
  static void UnloadAllPlugins();

  // Shuts down all loaded plugin instances.
  static void ShutdownAllPlugins();

  // Get the Plugin's function pointer table.
  NPPluginFuncs *functions();

  // Creates a new instance of this plugin.
  PluginInstance *CreateInstance(const std::string &mime_type);

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
  PluginLib(const WebPluginInfo& info);

  // Attempts to load the plugin from the library.
  // Returns true if it is a legitimate plugin, false otherwise
  bool Load();

  // Unloads the plugin library.
  void Unload();

  // Shutdown the plugin library.
  void Shutdown();
  
  //
  // Platform functions
  //
  
  // Gets the list of internal plugins.
  static void GetInternalPlugins(const InternalPluginInfo** plugins,
                                 size_t* count);
  
 public:
#if defined(OS_WIN)
  typedef HMODULE NativeLibrary;
  typedef char* NativeLibraryFunctionNameType;
#define FUNCTION_NAME(x) x
#elif defined(OS_MACOSX)
  typedef CFBundleRef NativeLibrary;
  typedef CFStringRef NativeLibraryFunctionNameType;
#define FUNCTION_NAME(x) CFSTR(x)
#endif  // OS_*
  
  // Loads a native library from disk. NOTE: You must release it with
  // UnloadNativeLibrary when you're done.
  static NativeLibrary LoadNativeLibrary(const FilePath& library_path);
  
  // Unloads a native library.
  static void UnloadNativeLibrary(NativeLibrary library);
  
 private:
  // Gets a function pointer from a native library.
  static void* GetFunctionPointerFromNativeLibrary(
      NativeLibrary library,
      NativeLibraryFunctionNameType name);
 
  bool internal_;  // Whether this an internal plugin.
  WebPluginInfo web_plugin_info_;  // supported mime types, description
  NativeLibrary library_;  // the opened library reference
  NPPluginFuncs plugin_funcs_;  // the struct of plugin side functions
  bool initialized_;  // is the plugin initialized
  NPSavedData *saved_data_;  // persisted plugin info for NPAPI
  int instance_count_;  // count of plugins in use

  // C-style function pointers
  NP_InitializeFunc NP_Initialize_;
  NP_GetEntryPointsFunc NP_GetEntryPoints_;
  NP_ShutdownFunc NP_Shutdown_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginLib);
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_LIB_H__
