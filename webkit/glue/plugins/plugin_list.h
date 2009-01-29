// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_LIST_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_LIST_H__

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/nphostapi.h"

class GURL;

namespace base {

template <typename T>
struct DefaultLazyInstanceTraits;

}  // namespace base

namespace NPAPI
{

#define kDefaultPluginLibraryName FILE_PATH_LITERAL("default_plugin")
#define kGearsPluginLibraryName FILE_PATH_LITERAL("gears")

class PluginInstance;

// This struct fully describes a plugin. For external plugins, it's read in from
// the version info of the dll; For internal plugins, it's predefined and
// includes addresses of entry functions. (Yes, it's Win32 NPAPI-centric, but
// it'll do for holding descriptions of internal plugins cross-platform.)
struct PluginVersionInfo {
  FilePath path;
  // Info about the plugin itself.
  std::wstring product_name;
  std::wstring file_description;
  std::wstring file_version;
  // Info about the data types that the plugin supports.
  std::wstring mime_types;
  std::wstring file_extensions;
  std::wstring type_descriptions;
  // Entry points for internal plugins, NULL for external ones.
  NP_GetEntryPointsFunc np_getentrypoints;
  NP_InitializeFunc np_initialize;
  NP_ShutdownFunc np_shutdown;
};

// The PluginList is responsible for loading our NPAPI based plugins. It does
// so in whatever manner is appropriate for the platform. On Windows, it loads
// plugins from a known directory by looking for DLLs which start with "NP",
// and checking to see if they are valid NPAPI libraries. On the Mac, it walks
// the machine-wide and user plugin directories and loads anything that has
// the correct types. On Linux, it walks the plugin directories as well
// (e.g. /usr/lib/browser-plugins/).
class PluginList {
 public:
  // Gets the one instance of the PluginList.  Accessing the singleton causes
  // the PluginList to look on disk for existing plugins.  It does not actually
  // load libraries, that will only happen when you initialize the plugin for
  // the first time.
  static PluginList* Singleton();

  // Add an extra plugin to load when we actually do the loading.  This is
  // static because we want to be able to add to it without searching the disk
  // for plugins.  Must be called before the plugins have been loaded.
  static void AddExtraPluginPath(const FilePath& plugin_path);

  // Register an internal plugin with the specified plugin information and
  // function pointers.  An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  static void RegisterInternalPlugin(const PluginVersionInfo& info);

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".  If it's an
  // internal plugin, the function pointers are returned as well.
  // Returns false if the library couldn't be found, or if it's not a plugin.
  static bool ReadPluginInfo(const FilePath& filename,
                             WebPluginInfo* info,
                             NP_GetEntryPointsFunc* np_getentrypoints,
                             NP_InitializeFunc* np_initialize,
                             NP_ShutdownFunc* np_shutdown);

  // Populate a WebPluginInfo from a PluginVersionInfo.
  static bool CreateWebPluginInfo(const PluginVersionInfo& pvi,
                                  WebPluginInfo* info);

  // Shutdown all plugins.  Should be called at process teardown.
  void Shutdown();

  // Get all the plugins
  bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);

  // Returns true if a plugin is found for the given url and mime type.
  // The mime type which corresponds to the URL is optionally returned
  // back.
  // The allow_wildcard parameter controls whether this function returns
  // plugins which support wildcard mime types (* as the mime type).
  bool GetPluginInfo(const GURL& url,
                     const std::string& mime_type,
                     const std::string& clsid,
                     bool allow_wildcard,
                     WebPluginInfo* info,
                     std::string* actual_mime_type);

  // Get plugin info by plugin path. Returns true if the plugin is found and
  // WebPluginInfo has been filled in |info|.
  bool GetPluginInfoByPath(const FilePath& plugin_path,
                           WebPluginInfo* info);

  // Load a specific plugin with full path.
  void LoadPlugin(const FilePath& filename);

 private:
  // Constructors are private for singletons
  PluginList();

  // Load all plugins from the default plugins directory
  void LoadPlugins(bool refresh);

  // Load all plugins from a specific directory
  void LoadPluginsFromDir(const FilePath& path);

  // Returns true if we should load the given plugin, or false otherwise.
  bool ShouldLoadPlugin(const WebPluginInfo& info);

  // Load internal plugins.
  void LoadInternalPlugins();

  // Find a plugin by mime type, and clsid.
  // If clsid is empty, we will just find the plugin that supports mime type.
  // Otherwise, if mime_type is application/x-oleobject etc that's supported by
  // by our activex shim, we need to check if the specified ActiveX exists.
  // If not we will not return the activex shim, instead we will let the
  // default plugin handle activex installation.
  // The allow_wildcard parameter controls whether this function returns
  // plugins which support wildcard mime types (* as the mime type)
  bool FindPlugin(const std::string &mime_type, const std::string& clsid,
                  bool allow_wildcard, WebPluginInfo* info);

  // Find a plugin by extension. Returns the corresponding mime type.
  bool FindPlugin(const GURL &url, std::string* actual_mime_type,
                  WebPluginInfo* info);

  // Returns true if the given WebPluginInfo supports "mime-type".
  // mime_type should be all lower case.
  static bool SupportsType(const WebPluginInfo& info,
                           const std::string &mime_type,
                           bool allow_wildcard);

  // Returns true if the given WebPluginInfo supports a given file extension.
  // extension should be all lower case.
  // If mime_type is not NULL, it will be set to the mime type if found.
  // The mime type which corresponds to the extension is optionally returned
  // back.
  static bool SupportsExtension(const WebPluginInfo& info,
                                const std::string &extension,
                                std::string* actual_mime_type);

  //
  // Platform functions
  //

  // Do any initialization.
  void PlatformInit();

  // Get the ordered list of directories from which to load plugins
  void GetPluginDirectories(std::vector<FilePath>* plugin_dirs);

  //
  // Command-line switches
  //

#if defined(OS_WIN)
  // true if we shouldn't load the new WMP plugin.
  bool dont_load_new_wmp_;

  // true if we should use our internal ActiveX shim
  bool use_internal_activex_shim_;
#endif

  //
  // Internals
  //

  bool plugins_loaded_;

  // Contains information about the available plugins.
  std::vector<WebPluginInfo> plugins_;

  // Extra plugin paths that we want to search when loading.
  std::vector<FilePath> extra_plugin_paths_;

  // Holds information about internal plugins.
  std::vector<PluginVersionInfo> internal_plugins_;

  friend struct base::DefaultLazyInstanceTraits<PluginList>;

  DISALLOW_EVIL_CONSTRUCTORS(PluginList);
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_LIST_H__

