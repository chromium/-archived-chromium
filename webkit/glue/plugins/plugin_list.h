// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Need mechanism to cleanup the static instance

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_LIST_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_LIST_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "webkit/glue/webplugin.h"

class GURL;

namespace NPAPI
{

// Used by plugins_test when testing the older WMP plugin to force the new
// plugin to not get loaded.
#define kUseOldWMPPluginSwitch L"use-old-wmp"
// Used for testing ActiveX shim. By default it's off. If this flag is specified
// we will use the native ActiveX shim.
#define kNoNativeActiveXShimSwitch L"no-activex"
// Internal file name for activex shim, used as a unique identifier.
#define kActiveXShimFileName L"activex-shim"
// Internal file name for windows media player.
#define kActivexShimFileNameForMediaPlayer \
    L"Microsoft® Windows Media Player Firefox Plugin"

#define kDefaultPluginDllName L"default_plugin"

class PluginLib;
class PluginInstance;

// The PluginList is responsible for loading our NPAPI based plugins.
// It loads plugins from a known directory by looking for DLLs
// which start with "NP", and checking to see if they are valid
// NPAPI libraries.
class PluginList : public base::RefCounted<PluginList> {
 public:
  // Gets the one instance of the PluginList.
  //
  // Accessing the singleton causes the PluginList to look on
  // disk for existing plugins.  It does not actually load
  // libraries, that will only happen when you initialize
  // the plugin for the first time.
  static PluginList* Singleton();

  // Add an extra plugin to load when we actually do the loading.  This is
  // static because we want to be able to add to it without searching the disk
  // for plugins.  Must be called before the plugins have been loaded.
  static void AddExtraPluginPath(const FilePath& plugin_path);

  virtual ~PluginList();

  // Find a plugin to by mime type, and clsid.
  // If clsid is empty, we will just find the plugin that supports mime type.
  // Otherwise, if mime_type is application/x-oleobject etc that supported by
  // by our activex shim, we need to check if the specified ActiveX exists. 
  // If not we will not return the activex shim, instead we will let the
  // default plugin handle activex installation.
  // The allow_wildcard parameter controls whether this function returns
  // plugins which support wildcard mime types (* as the mime type)
  PluginLib* FindPlugin(const std::string &mime_type, const std::string& clsid,
                        bool allow_wildcard);

  // Find a plugin to by extension. Returns the corresponding mime type
  PluginLib* FindPlugin(const GURL &url, std::string* actual_mime_type);

  // Check if we have any plugin for a given type.
  // mime_type must be all lowercase.
  bool SupportsType(const std::string &mime_type);

  // Returns true if the given WebPluginInfo supports a given file extension.
  // extension should be all lower case.
  // If mime_type is not NULL, it will be set to the mime type if found.
  // The mime type which corresponds to the extension is optionally returned
  // back.
  static bool SupportsExtension(const WebPluginInfo& info,
                                const std::string &extension,
                                std::string* actual_mime_type);

  // Shutdown all plugins.  Should be called at process teardown.
  void Shutdown();

  // Get all the plugins
  bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);

  // Returns true if a plugin is found for the given url and mime type.
  // The mime type which corresponds to the URL is optionally returned
  // back.
  // The allow_wildcard parameter controls whether this function returns
  // plugins which support wildcard mime types (* as the mime type)
  bool GetPluginInfo(const GURL& url,
                     const std::string& mime_type,
                     const std::string& clsid,
                     bool allow_wildcard,
                     WebPluginInfo* info,
                     std::string* actual_mime_type);

  // Get plugin info by plugin dll path. Returns true if the plugin is found and
  // WebPluginInfo has been filled in |info|
  bool GetPluginInfoByDllPath(const FilePath& dll_path,
                              WebPluginInfo* info);
 private:
  // Constructors are private for singletons
  PluginList();

  // Load all plugins from the default plugins directory
  void LoadPlugins(bool refresh);

  // Load all plugins from a specific directory
  void LoadPlugins(const FilePath& path);

  // Load a specific plugin with full path.
  void LoadPlugin(const FilePath& filename);

  // Returns true if we should load the given plugin, or false otherwise.
  bool ShouldLoadPlugin(const FilePath& filename);

  // Load internal plugins. Right now there is only one: activex_shim.
  void LoadInternalPlugins();

  // Find a plugin by filename.  Returns -1 if it's not found, otherwise its
  // index in plugins_.
  int FindPluginFile(const std::wstring& filename);

  // The application path where we expect to find plugins.
  static FilePath GetPluginAppDirectory();

  // The executable path where we expect to find plugins.
  static FilePath GetPluginExeDirectory();

  // Load plugins from the Firefox install path.  This is kind of
  // a kludge, but it helps us locate the flash player for users that
  // already have it for firefox.  Not having to download yet-another-plugin
  // is a good thing.
  void LoadFirefoxPlugins();

  // Hardcoded logic to detect and load acrobat plugins
  void LoadAcrobatPlugins();

  // Hardcoded logic to detect and load quicktime plugins
  void LoadQuicktimePlugins();

  // Hardcoded logic to detect and load Windows Media Player plugins
  void LoadWindowsMediaPlugins();

  // Hardcoded logic to detect and load Java plugins
  void LoadJavaPlugin();

#if defined(OS_WIN)
  // Search the registry at the given path and load plugins listed there.
  void LoadPluginsInRegistryFolder(HKEY root_key,
                                   const std::wstring& registry_folder);
#endif

  // true if we shouldn't load the new WMP plugin.
  bool dont_load_new_wmp_;

  bool use_internal_activex_shim_;

  static scoped_refptr<PluginList> singleton_;
  bool plugins_loaded_;
  std::vector<scoped_refptr<PluginLib> > plugins_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginList);
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_LIST_H__

