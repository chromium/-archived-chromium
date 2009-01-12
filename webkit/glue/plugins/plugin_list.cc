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
#include "net/base/mime_util.h"
#include "webkit/activex_shim/activex_shared.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"
#include "googleurl/src/gurl.h"

using base::TimeDelta;
using base::TimeTicks;

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
static const char  kMozillaActiveXPlugin[] = "npmozax.dll";
static const char  kNewWMPPlugin[] = "np-mswmp.dll";
static const char  kOldWMPPlugin[] = "npdsplay.dll";
static const char  kYahooApplicationStatePlugin[] = "npystate.dll";
static const TCHAR kRegistryJava[] =
    _T("Software\\JavaSoft\\Java Runtime Environment");
static const TCHAR kRegistryBrowserJavaVersion[] = _T("BrowserJavaVersion");
static const TCHAR kRegistryCurrentJavaVersion[] = _T("CurrentVersion");
static const TCHAR kRegistryJavaHome[] = _T("JavaHome");

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
  CommandLine command_line;
  dont_load_new_wmp_ = command_line.HasSwitch(kUseOldWMPPluginSwitch);
  use_internal_activex_shim_ =
      !command_line.HasSwitch(kNoNativeActiveXShimSwitch);
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
    WebPluginInfo info;
    if (PluginLib::ReadWebPluginInfo(FilePath(kDefaultPluginLibraryName),
                                     &info)) {
      plugins_[WideToUTF8(kDefaultPluginLibraryName)] = info;
    }
  }

  TimeTicks end_time = TimeTicks::Now();
  TimeDelta elapsed = end_time - start_time;
  DLOG(INFO) << "Loaded plugin list in " << elapsed.InMilliseconds() << " ms.";
}

void PluginList::LoadPlugins(const FilePath &path) {
  WIN32_FIND_DATA find_file_data;
  HANDLE find_handle;

  std::wstring dir = path.value();
  // FindFirstFile requires that you specify a wildcard for directories.
  dir.append(L"\\NP*.DLL");

  find_handle = FindFirstFile(dir.c_str(), &find_file_data);
  if (find_handle == INVALID_HANDLE_VALUE)
    return;

  do {
    if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      FilePath filename = path.Append(find_file_data.cFileName);
      LoadPlugin(filename);
    }
  } while (FindNextFile(find_handle, &find_file_data) != 0);

  DCHECK(GetLastError() == ERROR_NO_MORE_FILES);
  FindClose(find_handle);
}


// Compares Windows style version strings (i.e. 1,2,3,4).  Returns true if b's
// version is newer than a's, or false if it's equal or older.
bool IsNewerVersion(const std::wstring& a, const std::wstring& b) {
  std::vector<std::wstring> a_ver, b_ver;
  SplitString(a, ',', &a_ver);
  SplitString(b, ',', &b_ver);
  if (a_ver.size() != b_ver.size())
    return false;
  for (size_t i = 0; i < a_ver.size(); i++) {
    int cur_a = StringToInt(a_ver[i]);
    int cur_b = StringToInt(b_ver[i]);
    if (cur_a > cur_b)
      return false;
    if (cur_a < cur_b)
      return true;
  }
  return false;
}

void PluginList::LoadPlugin(const FilePath &path) {
  WebPluginInfo plugin_info;
  if (!PluginLib::ReadWebPluginInfo(path, &plugin_info))
    return;

  // Canonicalize names on Windows in case different versions of the plugin
  // have different case in the version string.
  std::string filename_lc = StringToLowerASCII(plugin_info.filename);
  if (!ShouldLoadPlugin(filename_lc))
    return;

  PluginMap::const_iterator iter = plugins_.find(filename_lc);
  if (iter != plugins_.end() &&
      !IsNewerVersion(iter->second.version, plugin_info.version))
    return;  // The loaded version is newer.

  for (size_t i = 0; i < plugin_info.mime_types.size(); ++i) {
    // TODO: don't load global handlers for now.
    // WebKit hands to the Plugin before it tries
    // to handle mimeTypes on its own.
    const std::string &mime_type = plugin_info.mime_types[i].mime_type;
    if (mime_type == "*" ) {
#ifndef NDEBUG
      // Make an exception for NPSPY.
      if (filename_lc == "npspy.dll")
        break;
#endif
      return;
    }
  }

  plugins_[filename_lc] = plugin_info;
}

bool PluginList::ShouldLoadPlugin(const std::string& filename) {
  // Depends on XPCOM.
  if (filename == kMozillaActiveXPlugin)
    return false;

  // Disable the yahoo application state plugin as it crashes the plugin
  // process on return from NPObjectStub::OnInvoke. Please refer to
  // http://b/issue?id=1372124 for more information.
  if (filename == kYahooApplicationStatePlugin)
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

      if (plugins_.find(kOldWMPPlugin) != plugins_.end())
        plugins_.erase(kOldWMPPlugin);
    } else if (filename == kOldWMPPlugin) {
      if (plugins_.find(kOldWMPPlugin) != plugins_.end())
        return false;
    }
  }

  return true;
}

void PluginList::LoadInternalPlugins() {
  if (!use_internal_activex_shim_)
    return;

  WebPluginInfo info;
  if (PluginLib::ReadWebPluginInfo(FilePath(kActiveXShimFileName),
                                   &info)) {
    plugins_[WideToUTF8(kActiveXShimFileName)] = info;
  }

  if (PluginLib::ReadWebPluginInfo(FilePath(kActivexShimFileNameForMediaPlayer),
                                   &info)) {
    plugins_[WideToUTF8(kActivexShimFileNameForMediaPlayer)] = info;
  }
}  

bool PluginList::FindPlugin(const std::string& mime_type,
                            const std::string& clsid,
                            bool allow_wildcard,
                            WebPluginInfo* info) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));

  PluginMap::const_iterator default_iter = plugins_.end();
  for (PluginMap::const_iterator iter = plugins_.begin();
       iter != plugins_.end(); ++iter) {
    if (SupportsType(iter->second, mime_type, allow_wildcard)) {
      if (iter->second.path.value() == kDefaultPluginLibraryName) {
        // Only use the default plugin if it's the only one that's found.
        default_iter = iter;
        continue;
      }

      if (!clsid.empty() && iter->second.path.value() == kActiveXShimFileName) {
        // Special handling for ActiveX shim. If ActiveX is not installed, we
        // should use the default plugin to show the installation UI.
        if (!activex_shim::IsActiveXInstalled(clsid))
          continue;
      }
      *info = iter->second;
      return true;
    }
  }

  if (default_iter != plugins_.end()) {
    *info = default_iter->second;
    return true;
  }

  return false;
}

bool PluginList::FindPlugin(const GURL &url, std::string* actual_mime_type,
                            WebPluginInfo* info) {
  std::wstring path = base::SysNativeMBToWide(url.path());
  std::wstring extension_wide = file_util::GetFileExtensionFromPath(path);
  if (extension_wide.empty())
    return false;

  std::string extension =
      StringToLowerASCII(base::SysWideToNativeMB(extension_wide));

  for (PluginMap::const_iterator iter = plugins_.begin();
       iter != plugins_.end(); ++iter) {
    if (SupportsExtension(iter->second, extension, actual_mime_type)) {
      *info = iter->second;
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

  int i = 0;
  plugins->resize(plugins_.size());
  for (PluginMap::const_iterator iter = plugins_.begin();
       iter != plugins_.end(); ++iter) {
    (*plugins)[i++] = iter->second;
  }

  return true;
}

bool PluginList::GetPluginInfo(const GURL& url,
                               const std::string& mime_type,
                               const std::string& clsid,
                               bool allow_wildcard,
                               WebPluginInfo* info,
                               std::string* actual_mime_type) {
  bool found = FindPlugin(mime_type, clsid, allow_wildcard, info);
  if (!found ||
      (info->path.value() == kDefaultPluginLibraryName && clsid.empty())) {
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
  for (PluginMap::const_iterator iter = plugins_.begin();
       iter != plugins_.end(); ++iter) {
    if (iter->second.path == plugin_path) {
      *info = iter->second;
      return true;
    }
  }

  return false;
}

void PluginList::Shutdown() {
  // TODO
}

FilePath PluginList::GetPluginAppDirectory() {
  std::wstring app_path;
  // TODO(avi): use PathService directly
  if (webkit_glue::GetApplicationDirectory(&app_path))
    app_path.append(L"\\plugins");

  return FilePath(app_path);
}

FilePath PluginList::GetPluginExeDirectory() {
  std::wstring exe_path;
  // TODO(avi): use PathService directly
  if (webkit_glue::GetExeDirectory(&exe_path))
    exe_path.append(L"\\plugins");

  return FilePath(exe_path);
}

// Gets the installed path for a registered app.
static bool GetInstalledPath(const TCHAR* app, FilePath* out) {
  std::wstring reg_path(kRegistryApps);
  reg_path.append(L"\\");
  reg_path.append(app);

  RegKey key(HKEY_LOCAL_MACHINE, reg_path.c_str());
  std::wstring path;
  if (key.ReadValue(kRegistryPath, &path)) {
    *out = FilePath(path);
    return true;
  }

  return false;
}

// Enumerate through the registry key to find all installed FireFox paths.
// FireFox 3 beta and version 2 can coexist. See bug: 1025003
static void GetFirefoxInstalledPaths(std::vector<FilePath>* out) {
  RegistryKeyIterator it(HKEY_LOCAL_MACHINE, kRegistryFirefoxInstalled);
  for (; it.Valid(); ++it) {
    std::wstring full_path = std::wstring(kRegistryFirefoxInstalled) + L"\\" + 
                             it.Name() + L"\\Main";
    RegKey key(HKEY_LOCAL_MACHINE, full_path.c_str(), KEY_READ);
    std::wstring install_dir;
    if (!key.ReadValue(L"Install Directory", &install_dir))
      continue;
    out->push_back(FilePath(install_dir));
  }
}

void PluginList::LoadFirefoxPlugins() {
  std::vector<FilePath> paths;
  GetFirefoxInstalledPaths(&paths);
  for (unsigned int i = 0; i < paths.size(); ++i) {
    FilePath path = paths[i].Append(L"plugins");
    LoadPlugins(path);
  }

  LoadPluginsInRegistryFolder(HKEY_CURRENT_USER, kRegistryMozillaPlugins);
  LoadPluginsInRegistryFolder(HKEY_LOCAL_MACHINE, kRegistryMozillaPlugins);

  std::wstring firefox_app_data_plugin_path;
  if (PathService::Get(base::DIR_APP_DATA, &firefox_app_data_plugin_path)) {
    firefox_app_data_plugin_path += L"\\Mozilla\\plugins";
    LoadPlugins(FilePath(firefox_app_data_plugin_path));
  }
}

void PluginList::LoadAcrobatPlugins() {
  FilePath path;
  if (!GetInstalledPath(kRegistryAcrobatReader, &path) &&
      !GetInstalledPath(kRegistryAcrobat, &path)) {
    return;
  }

  path = path.Append(L"Browser");
  LoadPlugins(path);
}

void PluginList::LoadQuicktimePlugins() {
  FilePath path;
  if (GetInstalledPath(kRegistryQuickTime, &path)) {
    path = path.Append(L"plugins");
    LoadPlugins(path);
  }
}

void PluginList::LoadWindowsMediaPlugins() {
  FilePath path;
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
      LoadPlugins(FilePath(java_plugin_directory));
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
      LoadPlugin(FilePath(path));
  }
}

} // namespace NPAPI

