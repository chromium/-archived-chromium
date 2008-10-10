// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <tchar.h>

#include "webkit/glue/plugins/plugin_list.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "webkit/activex_shim/activex_shared.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "googleurl/src/gurl.h"

namespace NPAPI
{

scoped_refptr<PluginList> PluginList::singleton_;

static const TCHAR kRegistryApps[] =
    _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths");
static const TCHAR kRegistryFirefox[] = _T("firefox.exe");
static const TCHAR kRegistryAcrobat[] = _T("Acrobat.exe");
static const TCHAR kRegistryAcrobatReader[] = _T("AcroRd32.exe");
static const TCHAR kRegistryWindowsMedia[] = _T("wmplayer.exe");
static const TCHAR kRegistryQuickTime[] = _T("QuickTimePlayer.exe");
static const TCHAR kRegistryPath[] = _T("Path");
static const TCHAR kRegistryMozillaPlugins[] = _T("SOFTWARE\\MozillaPlugins");
static const TCHAR kRegistryFirefoxInstalled[] = 
    _T("SOFTWARE\\Mozilla\\Mozilla Firefox");
static const TCHAR kMozillaActiveXPlugin[] = _T("npmozax.dll");
static const TCHAR kNewWMPPlugin[] = _T("np-mswmp.dll");
static const TCHAR kOldWMPPlugin[] = _T("npdsplay.dll");
static const TCHAR kRegistryJava[] =
    _T("Software\\JavaSoft\\Java Runtime Environment");
static const TCHAR kRegistryBrowserJavaVersion[] = _T("BrowserJavaVersion");
static const TCHAR kRegistryCurrentJavaVersion[] = _T("CurrentVersion");
static const TCHAR kRegistryJavaHome[] = _T("JavaHome");

// Extra registry paths to search.
static std::vector<std::wstring>* extra_plugin_paths_ = NULL;

PluginList* PluginList::Singleton() {
  if (singleton_.get() == NULL) {
    singleton_ = new PluginList();
    singleton_->LoadPlugins(false);
  }

  return singleton_;
}

void PluginList::AddExtraPluginPath(const std::wstring& plugin_path) {
  DCHECK(!singleton_.get() || !singleton_->plugins_loaded_);

  if (!extra_plugin_paths_)
    extra_plugin_paths_ = new std::vector<std::wstring>;
  extra_plugin_paths_->push_back(plugin_path);
}

PluginList::PluginList() :
    plugins_loaded_(false) {
  CommandLine command_line;
  dont_load_new_wmp_ = command_line.HasSwitch(kUseOldWMPPluginSwitch);
  use_internal_activex_shim_ =
      !command_line.HasSwitch(kNoNativeActiveXShimSwitch);
}

PluginList::~PluginList() {
  plugins_.clear();
}

void PluginList::LoadPlugins(bool refresh) {
  if (plugins_loaded_ && !refresh)
    return;

  plugins_.clear();
  plugins_loaded_ = true;

  TimeTicks start_time = TimeTicks::Now();

  LoadInternalPlugins();

  // Load any plugins listed in the registry
  if (extra_plugin_paths_) {
    for (size_t i = 0; i < extra_plugin_paths_->size(); ++i) {
      LoadPlugin((*extra_plugin_paths_)[i]);
    }
  }

  // Load from the application-specific area
  LoadPlugins(GetPluginAppDirectory());

  // Load from the executable area
  LoadPlugins(GetPluginExeDirectory());

  // Load Java
  LoadJavaPlugin();

  // Load firefox plugins too.  This is mainly to try to locate 
  // a pre-installed Flash player.
  LoadFirefoxPlugins();

  // Firefox hard-codes the paths of some popular plugins to ensure that
  // the plugins are found.  We are going to copy this as well.
  LoadAcrobatPlugins();
  LoadQuicktimePlugins();
  LoadWindowsMediaPlugins();

  if (webkit_glue::IsDefaultPluginEnabled()) {
    scoped_refptr<PluginLib> default_plugin = PluginLib::CreatePluginLib(
        kDefaultPluginDllName);
    plugins_.push_back(default_plugin);
  }

  TimeTicks end_time = TimeTicks::Now();
  TimeDelta elapsed = end_time - start_time;
  DLOG(INFO) << "Loaded plugin list in " << elapsed.InMilliseconds() << " ms.";
}

void PluginList::LoadPlugins(const std::wstring &path) {
  WIN32_FIND_DATA find_file_data;
  HANDLE find_handle;

  std::wstring dir = path;
  // FindFirstFile requires that you specify a wildcard for directories.
  dir.append(L"\\NP*.DLL");

  find_handle = FindFirstFile(dir.c_str(), &find_file_data);
  if (find_handle == INVALID_HANDLE_VALUE)
    return;

  do {
    if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      std::wstring filename = path;
      filename.append(L"\\");
      filename.append(find_file_data.cFileName);

      LoadPlugin(filename);
    }
  } while (FindNextFile(find_handle, &find_file_data) != 0);

  DCHECK(GetLastError() == ERROR_NO_MORE_FILES);
  FindClose(find_handle);
}

void PluginList::LoadPlugin(const std::wstring &path) {
  std::wstring path_lc = StringToLowerASCII(path);
  if (!ShouldLoadPlugin(file_util::GetFilenameFromPath(path_lc)))
    return;

  scoped_refptr<PluginLib> new_plugin = PluginLib::CreatePluginLib(path_lc);
  if (!new_plugin.get())
    return;

  const WebPluginInfo& plugin_info = new_plugin->plugin_info();
  for (size_t i = 0; i < plugin_info.mime_types.size(); ++i) {
    // TODO: don't load global handlers for now.
    // WebKit hands to the Plugin before it tries
    // to handle mimeTypes on its own.
    const std::string &mime_type = plugin_info.mime_types[i].mime_type;
    if (mime_type == "*" ) {
#ifndef NDEBUG
      // Make an exception for NPSPY.
      if (plugin_info.file.find(L"npspy.dll") != std::wstring::npos) {
        // Put it at the beginning so it's used before the real plugin.
        plugins_.insert(plugins_.begin(), new_plugin.get());
      }
#endif
      continue;
    }

    if (!SupportsType(mime_type))
      plugins_.push_back(new_plugin);
  }
}

bool PluginList::ShouldLoadPlugin(const std::wstring& filename) {
  // Depends on XPCOM.
  if (filename == kMozillaActiveXPlugin)
    return false;

  // We will use activex shim to handle embeded wmp media.
  if (use_internal_activex_shim_) {
    if (filename == kNewWMPPlugin || filename == kOldWMPPlugin)
      return false;
  } else {
    // If both the new and old WMP plugins exist, only load the new one.
    if (filename == kNewWMPPlugin) {
      if (dont_load_new_wmp_)
        return false;

      int old_plugin = FindPluginFile(kOldWMPPlugin);
      if (old_plugin != -1)
        plugins_.erase(plugins_.begin() + old_plugin);
    } else if (filename == kOldWMPPlugin) {
      if (FindPluginFile(kNewWMPPlugin) != -1)
        return false;
    }
  }

  return true;
}

void PluginList::LoadInternalPlugins() {
  if (use_internal_activex_shim_) {
    scoped_refptr<PluginLib> new_plugin = PluginLib::CreatePluginLib(
        kActiveXShimFileName);
    plugins_.push_back(new_plugin);
    
    new_plugin = PluginLib::CreatePluginLib(
        kActivexShimFileNameForMediaPlayer);
    plugins_.push_back(new_plugin);
  }
}

int PluginList::FindPluginFile(const std::wstring& filename) {
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (file_util::GetFilenameFromPath(plugins_[i]->plugin_info().file) ==
        filename) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

PluginLib* PluginList::FindPlugin(const std::string& mime_type,
                                  const std::string& clsid,
                                  bool allow_wildcard) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));

  for (size_t idx = 0; idx < plugins_.size(); ++idx) {
    if (plugins_[idx]->SupportsType(mime_type, allow_wildcard)) {
      if (!clsid.empty() && 
          plugins_[idx]->plugin_info().file == kActiveXShimFileName) {
        // Special handling for ActiveX shim. If ActiveX is not installed, we
        // should use the default plugin to show the installation UI.
        if (!activex_shim::IsActiveXInstalled(clsid))
          continue;
      }
      return plugins_[idx];
    }
  }

  return NULL;
}

PluginLib* PluginList::FindPlugin(const GURL &url, std::string* actual_mime_type) {
  std::wstring path = base::SysNativeMBToWide(url.path());
  std::wstring extension_wide = file_util::GetFileExtensionFromPath(path);
  if (extension_wide.empty())
    return NULL;;

  std::string extension =
      StringToLowerASCII(base::SysWideToNativeMB(extension_wide));

  for (size_t idx = 0; idx < plugins_.size(); ++idx) {
    if (SupportsExtension(plugins_[idx]->plugin_info(), extension, actual_mime_type)) {
      return plugins_[idx];
    }
  }

  return NULL;
}

bool PluginList::SupportsType(const std::string &mime_type) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));
  bool allow_wildcard = true;
  return (FindPlugin(mime_type, "", allow_wildcard ) != 0);
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

  plugins->resize(plugins_.size());
  for (size_t i = 0; i < plugins->size(); ++i)
    (*plugins)[i] = plugins_[i]->plugin_info();

  return true;
}

bool PluginList::GetPluginInfo(const GURL& url,
                               const std::string& mime_type,
                               const std::string& clsid,
                               bool allow_wildcard,
                               WebPluginInfo* info,
                               std::string* actual_mime_type) {
  scoped_refptr<PluginLib> plugin = FindPlugin(mime_type, clsid, 
                                               allow_wildcard);

  if (plugin.get() == NULL ||
      (plugin->plugin_info().file == kDefaultPluginDllName && clsid.empty())) {
    scoped_refptr<PluginLib> default_plugin = plugin;
    plugin = FindPlugin(url, actual_mime_type);
    // url matches may not return the default plugin if no match is found.
    if (plugin.get() == NULL && default_plugin.get() != NULL)
      plugin = default_plugin;
  }

  if (plugin.get() == NULL)
    return false;

  *info = plugin->plugin_info();
  return true;
}

bool PluginList::GetPluginInfoByDllPath(const std::wstring& dll_path,
                                        WebPluginInfo* info) {
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (wcsicmp(plugins_[i]->plugin_info().file.c_str(),
                dll_path.c_str()) == 0) {
      *info = plugins_[i]->plugin_info();
      return true;
    }
  }

  return false;
}

void PluginList::Shutdown() {
  // TODO
}

std::wstring PluginList::GetPluginAppDirectory() {
  std::wstring app_path;
  if (webkit_glue::GetApplicationDirectory(&app_path))
    app_path.append(L"\\plugins");

  return app_path;
}

std::wstring PluginList::GetPluginExeDirectory() {
  std::wstring exe_path;
  if (webkit_glue::GetExeDirectory(&exe_path))
    exe_path.append(L"\\plugins");

  return exe_path;
}

// Gets the installed path for a registered app.
static bool GetInstalledPath(const TCHAR* app, std::wstring* out) {
  std::wstring reg_path(kRegistryApps);
  reg_path.append(L"\\");
  reg_path.append(app);

  RegKey key(HKEY_LOCAL_MACHINE, reg_path.c_str());
  return key.ReadValue(kRegistryPath, out);
}

// Enumerate through the registry key to find all installed FireFox paths.
// FireFox 3 beta and version 2 can coexist. See bug: 1025003
static void GetFirefoxInstalledPaths(std::vector<std::wstring>* out) {
  RegistryKeyIterator it(HKEY_LOCAL_MACHINE, kRegistryFirefoxInstalled);
  for (; it.Valid(); ++it) {
    std::wstring full_path = std::wstring(kRegistryFirefoxInstalled) + L"\\" + 
                             it.Name() + L"\\Main";
    RegKey key(HKEY_LOCAL_MACHINE, full_path.c_str(), KEY_READ);
    std::wstring install_dir;
    if (!key.ReadValue(L"Install Directory", &install_dir))
      continue;
    out->push_back(install_dir);
  }
}

void PluginList::LoadFirefoxPlugins() {
  std::vector<std::wstring> paths;
  GetFirefoxInstalledPaths(&paths);
  for (unsigned int i = 0; i < paths.size(); ++i) {
    std::wstring path = paths[i] + L"\\plugins";
    LoadPlugins(path);
  }

  LoadPluginsInRegistryFolder(HKEY_CURRENT_USER, kRegistryMozillaPlugins);
  LoadPluginsInRegistryFolder(HKEY_LOCAL_MACHINE, kRegistryMozillaPlugins);

  std::wstring firefox_app_data_plugin_path;
  if (PathService::Get(base::DIR_APP_DATA, &firefox_app_data_plugin_path)) {
    firefox_app_data_plugin_path += L"\\Mozilla\\plugins";
    LoadPlugins(firefox_app_data_plugin_path);
  }
}

void PluginList::LoadAcrobatPlugins() {
  std::wstring path;
  if (!GetInstalledPath(kRegistryAcrobatReader, &path) &&
      !GetInstalledPath(kRegistryAcrobat, &path)) {
    return;
  }

  path.append(L"\\Browser");
  LoadPlugins(path);
}

void PluginList::LoadQuicktimePlugins() {
  std::wstring path;
  if (GetInstalledPath(kRegistryQuickTime, &path)) {
    path.append(L"\\plugins");
    LoadPlugins(path);
  }
}

void PluginList::LoadWindowsMediaPlugins() {
  std::wstring path;
  if (GetInstalledPath(kRegistryWindowsMedia, &path)) {
    LoadPlugins(path);
  }
}

void PluginList::LoadJavaPlugin() {
  // Load the new NPAPI Java plugin
  // 1. Open the main JRE key under HKLM
  RegKey java_key(HKEY_LOCAL_MACHINE, kRegistryJava, KEY_QUERY_VALUE);

  // 2. Read the current Java version
  std::wstring java_version;
  if (!java_key.ReadValue(kRegistryBrowserJavaVersion, &java_version))
    java_key.ReadValue(kRegistryCurrentJavaVersion, &java_version);

  if (!java_version.empty()) {
    java_key.OpenKey(java_version.c_str(), KEY_QUERY_VALUE);

    // 3. Install path of the JRE binaries is specified in "JavaHome" 
    //    value under the Java version key.
    std::wstring java_plugin_directory;
    if (java_key.ReadValue(kRegistryJavaHome, &java_plugin_directory)) {

      // 4. The new plugin resides under the 'bin/new_plugin' 
      //    subdirectory.
      DCHECK(!java_plugin_directory.empty());
      java_plugin_directory.append(L"\\bin\\new_plugin");

      // 5. We don't know the exact name of the DLL but it's in the form
      //    NP*.dll so just invoke LoadPlugins on this path.
      LoadPlugins(java_plugin_directory);
    }
  }
}

void PluginList::LoadPluginsInRegistryFolder(
    HKEY root_key,
    const std::wstring& registry_folder) {
  for (RegistryKeyIterator iter(root_key, registry_folder.c_str());
       iter.Valid(); ++iter) {
    // Use the registry to gather plugin across the file system.
    std::wstring reg_path = registry_folder;
    reg_path.append(L"\\");
    reg_path.append(iter.Name());
    RegKey key(root_key, reg_path.c_str());

    std::wstring path;
    if (key.ReadValue(kRegistryPath, &path))
      LoadPlugin(path);
  }
}

} // namespace NPAPI

