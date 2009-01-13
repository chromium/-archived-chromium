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
static const TCHAR kMozillaActiveXPlugin[] = _T("npmozax.dll");
static const TCHAR kNewWMPPlugin[] = _T("np-mswmp.dll");
static const TCHAR kOldWMPPlugin[] = _T("npdsplay.dll");
static const TCHAR kYahooApplicationStatePlugin[] = _T("npystate.dll");
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

  std::set<FilePath> directories_to_scan;


  // Load any plugins listed in the registry
  if (extra_plugin_paths_) {
    for (size_t i = 0; i < extra_plugin_paths_->size(); ++i)
      directories_to_scan.insert((*extra_plugin_paths_)[i].DirName());
  }

  // Load from the application-specific area
  GetAppDirectory(&directories_to_scan);

  // Load from the executable area
  GetExeDirectory(&directories_to_scan);

  // Load Java
  GetJavaDirectory(&directories_to_scan);

  // Load firefox plugins too.  This is mainly to try to locate 
  // a pre-installed Flash player.
  GetFirefoxDirectory(&directories_to_scan);

  // Firefox hard-codes the paths of some popular plugins to ensure that
  // the plugins are found.  We are going to copy this as well.
  GetAcrobatDirectory(&directories_to_scan);
  GetQuicktimeDirectory(&directories_to_scan);
  GetWindowsMediaDirectory(&directories_to_scan);

  for (std::set<FilePath>::const_iterator iter = directories_to_scan.begin();
       iter != directories_to_scan.end(); ++iter) {
    LoadPluginsFromDir(*iter);
  }

  if (webkit_glue::IsDefaultPluginEnabled()) {
    WebPluginInfo info;
    if (PluginLib::ReadWebPluginInfo(FilePath(kDefaultPluginLibraryName),
                                     &info)) {
      plugins_.push_back(info);
    }
  }

  TimeTicks end_time = TimeTicks::Now();
  TimeDelta elapsed = end_time - start_time;
  DLOG(INFO) << "Loaded plugin list in " << elapsed.InMilliseconds() << " ms.";
}

void PluginList::LoadPluginsFromDir(const FilePath &path) {
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

  if (!ShouldLoadPlugin(plugin_info.path))
    return;

  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (plugins_[i].path.BaseName() == path.BaseName() &&
        !IsNewerVersion(plugins_[i].version, plugin_info.version)) {
      return;  // The loaded version is newer.
    }
  }

  for (size_t i = 0; i < plugin_info.mime_types.size(); ++i) {
    // TODO: don't load global handlers for now.
    // WebKit hands to the Plugin before it tries
    // to handle mimeTypes on its own.
    const std::string &mime_type = plugin_info.mime_types[i].mime_type;
    if (mime_type == "*" ) {
#ifndef NDEBUG
      // Make an exception for NPSPY.
      if (path.BaseName().value() == L"npspy.dll")
        break;
#endif
      return;
    }
  }

  plugins_.push_back(plugin_info);
}

bool PluginList::ShouldLoadPlugin(const FilePath& path) {
  std::wstring filename = StringToLowerASCII(path.BaseName().value());
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

      for (size_t i = 0; i < plugins_.size(); ++i) {
        if (plugins_[i].path.BaseName().value() == kOldWMPPlugin) {
          plugins_.erase(plugins_.begin() + i);
          break;
        }
      }
    } else if (filename == kOldWMPPlugin) {
      for (size_t i = 0; i < plugins_.size(); ++i) {
        if (plugins_[i].path.BaseName().value() == kNewWMPPlugin)
          return false;
      }
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
    plugins_.push_back(info);
  }

  if (PluginLib::ReadWebPluginInfo(FilePath(kActivexShimFileNameForMediaPlayer),
                                   &info)) {
    plugins_.push_back(info);
  }
}  

bool PluginList::FindPlugin(const std::string& mime_type,
                            const std::string& clsid,
                            bool allow_wildcard,
                            WebPluginInfo* info) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));

  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (SupportsType(plugins_[i], mime_type, allow_wildcard)) {
      if (!clsid.empty() && plugins_[i].path.value() == kActiveXShimFileName) {
        // Special handling for ActiveX shim. If ActiveX is not installed, we
        // should use the default plugin to show the installation UI.
        if (!activex_shim::IsActiveXInstalled(clsid))
          continue;
      }
      *info = plugins_[i];
      return true;
    }
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

void PluginList::GetAppDirectory(std::set<FilePath>* plugin_dirs) {
  std::wstring app_path;
  // TODO(avi): use PathService directly
  if (!webkit_glue::GetApplicationDirectory(&app_path))
    return;

  app_path.append(L"\\plugins");
  plugin_dirs->insert(FilePath(app_path));
}

void PluginList::GetExeDirectory(std::set<FilePath>* plugin_dirs) {
  std::wstring exe_path;
  // TODO(avi): use PathService directly
  if (!webkit_glue::GetExeDirectory(&exe_path))
    return;

  exe_path.append(L"\\plugins");
  plugin_dirs->insert(FilePath(exe_path));
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

void PluginList::GetFirefoxDirectory(std::set<FilePath>* plugin_dirs) {
  std::vector<FilePath> paths;
  GetFirefoxInstalledPaths(&paths);
  for (unsigned int i = 0; i < paths.size(); ++i) {
    plugin_dirs->insert(paths[i].Append(L"plugins"));
  }

  GetPluginsInRegistryDirectory(
      HKEY_CURRENT_USER, kRegistryMozillaPlugins, plugin_dirs);
  GetPluginsInRegistryDirectory(
      HKEY_LOCAL_MACHINE, kRegistryMozillaPlugins, plugin_dirs);

  std::wstring firefox_app_data_plugin_path;
  if (PathService::Get(base::DIR_APP_DATA, &firefox_app_data_plugin_path)) {
    firefox_app_data_plugin_path += L"\\Mozilla\\plugins";
    plugin_dirs->insert(FilePath(firefox_app_data_plugin_path));
  }
}

void PluginList::GetAcrobatDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (!GetInstalledPath(kRegistryAcrobatReader, &path) &&
      !GetInstalledPath(kRegistryAcrobat, &path)) {
    return;
  }

  plugin_dirs->insert(path.Append(L"Browser"));
}

void PluginList::GetQuicktimeDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (GetInstalledPath(kRegistryQuickTime, &path))
    plugin_dirs->insert(path.Append(L"plugins"));
}

void PluginList::GetWindowsMediaDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (GetInstalledPath(kRegistryWindowsMedia, &path))
    plugin_dirs->insert(path);
}

void PluginList::GetJavaDirectory(std::set<FilePath>* plugin_dirs) {
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
      plugin_dirs->insert(FilePath(java_plugin_directory));
    }
  }
}

void PluginList::GetPluginsInRegistryDirectory(
    HKEY root_key,
    const std::wstring& registry_folder,
    std::set<FilePath>* plugin_dirs) {
  for (RegistryKeyIterator iter(root_key, registry_folder.c_str());
       iter.Valid(); ++iter) {
    // Use the registry to gather plugin across the file system.
    std::wstring reg_path = registry_folder;
    reg_path.append(L"\\");
    reg_path.append(iter.Name());
    RegKey key(root_key, reg_path.c_str());

    std::wstring path;
    if (key.ReadValue(kRegistryPath, &path))
      plugin_dirs->insert(FilePath(path).DirName());
  }
}

} // namespace NPAPI

