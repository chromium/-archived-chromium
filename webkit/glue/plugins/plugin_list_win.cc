// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tchar.h>

#include "webkit/glue/plugins/plugin_list.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "webkit/activex_shim/npp_impl.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/webkit_glue.h"

namespace {

const TCHAR kRegistryApps[] =
    _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths");
const TCHAR kRegistryFirefox[] = _T("firefox.exe");
const TCHAR kRegistryAcrobat[] = _T("Acrobat.exe");
const TCHAR kRegistryAcrobatReader[] = _T("AcroRd32.exe");
const TCHAR kRegistryWindowsMedia[] = _T("wmplayer.exe");
const TCHAR kRegistryQuickTime[] = _T("QuickTimePlayer.exe");
const TCHAR kRegistryPath[] = _T("Path");
const TCHAR kRegistryMozillaPlugins[] = _T("SOFTWARE\\MozillaPlugins");
const TCHAR kRegistryFirefoxInstalled[] =
    _T("SOFTWARE\\Mozilla\\Mozilla Firefox");
const TCHAR kMozillaActiveXPlugin[] = _T("npmozax.dll");
const TCHAR kNewWMPPlugin[] = _T("np-mswmp.dll");
const TCHAR kOldWMPPlugin[] = _T("npdsplay.dll");
const TCHAR kYahooApplicationStatePlugin[] = _T("npystate.dll");
const TCHAR kWanWangProtocolHandlerPlugin[] = _T("npww.dll");
const TCHAR kRegistryJava[] =
    _T("Software\\JavaSoft\\Java Runtime Environment");
const TCHAR kRegistryBrowserJavaVersion[] = _T("BrowserJavaVersion");
const TCHAR kRegistryCurrentJavaVersion[] = _T("CurrentVersion");
const TCHAR kRegistryJavaHome[] = _T("JavaHome");

#ifdef GEARS_STATIC_LIB
// defined in gears/base/common/module.cc
NPError API_CALL Gears_NP_GetEntryPoints(NPPluginFuncs* funcs);
NPError API_CALL Gears_NP_Initialize(NPNetscapeFuncs* funcs);
NPError API_CALL Gears_NP_Shutdown(void);
#endif

// The application path where we expect to find plugins.
void GetAppDirectory(std::set<FilePath>* plugin_dirs) {
  std::wstring app_path;
  // TODO(avi): use PathService directly
  if (!webkit_glue::GetApplicationDirectory(&app_path))
    return;

  app_path.append(L"\\plugins");
  plugin_dirs->insert(FilePath(app_path));
}

// The executable path where we expect to find plugins.
void GetExeDirectory(std::set<FilePath>* plugin_dirs) {
  std::wstring exe_path;
  // TODO(avi): use PathService directly
  if (!webkit_glue::GetExeDirectory(&exe_path))
    return;

  exe_path.append(L"\\plugins");
  plugin_dirs->insert(FilePath(exe_path));
}

// Gets the installed path for a registered app.
bool GetInstalledPath(const TCHAR* app, FilePath* out) {
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

// Search the registry at the given path and detect plugin directories.
void GetPluginsInRegistryDirectory(
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

// Enumerate through the registry key to find all installed FireFox paths.
// FireFox 3 beta and version 2 can coexist. See bug: 1025003
void GetFirefoxInstalledPaths(std::vector<FilePath>* out) {
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

// Get plugin directory locations from the Firefox install path.  This is kind
// of a kludge, but it helps us locate the flash player for users that
// already have it for firefox.  Not having to download yet-another-plugin
// is a good thing.
void GetFirefoxDirectory(std::set<FilePath>* plugin_dirs) {
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

// Hardcoded logic to detect Acrobat plugins locations.
void GetAcrobatDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (!GetInstalledPath(kRegistryAcrobatReader, &path) &&
      !GetInstalledPath(kRegistryAcrobat, &path)) {
    return;
  }

  plugin_dirs->insert(path.Append(L"Browser"));
}

// Hardcoded logic to detect QuickTime plugin location.
void GetQuicktimeDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (GetInstalledPath(kRegistryQuickTime, &path))
    plugin_dirs->insert(path.Append(L"plugins"));
}

// Hardcoded logic to detect Windows Media Player plugin location.
void GetWindowsMediaDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (GetInstalledPath(kRegistryWindowsMedia, &path))
    plugin_dirs->insert(path);
}

// Hardcoded logic to detect Java plugin location.
void GetJavaDirectory(std::set<FilePath>* plugin_dirs) {
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

}

namespace NPAPI
{

void PluginList::PlatformInit() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  dont_load_new_wmp_ = command_line.HasSwitch(kUseOldWMPPluginSwitch);
  use_internal_activex_shim_ =
      !command_line.HasSwitch(kNoNativeActiveXShimSwitch);

  const PluginVersionInfo builtin_plugins[] = {
  {
    FilePath(kActiveXShimFileName),
    L"ActiveX Plug-in",
    L"ActiveX Plug-in provides a shim to support ActiveX controls",
    L"1, 0, 0, 1",
    L"application/x-oleobject|application/oleobject",
    L"*|*",
    L"",
    {
      activex_shim::ActiveX_Shim_NP_GetEntryPoints,
      activex_shim::ActiveX_Shim_NP_Initialize,
      activex_shim::ActiveX_Shim_NP_Shutdown
    }
  },
  {
    FilePath(kActiveXShimFileNameForMediaPlayer),
    kActiveXShimFileNameForMediaPlayer,
    L"Windows Media Player",
    L"1, 0, 0, 1",
    L"application/x-ms-wmp|application/asx|video/x-ms-asf-plugin|"
        L"application/x-mplayer2|video/x-ms-asf|video/x-ms-wm|audio/x-ms-wma|"
        L"audio/x-ms-wax|video/x-ms-wmv|video/x-ms-wvx",
    L"*|*|*|*|asf,asx,*|wm,*|wma,*|wax,*|wmv,*|wvx,*",
    L"",
    {
      activex_shim::ActiveX_Shim_NP_GetEntryPoints,
      activex_shim::ActiveX_Shim_NP_Initialize,
      activex_shim::ActiveX_Shim_NP_Shutdown
    }
  },
#ifdef GEARS_STATIC_LIB
  {
    FilePath(kGearsPluginLibraryName),
    L"Gears",
    L"Statically linked Gears",
    L"1, 0, 0, 1",
    L"application/x-googlegears",
    L"",
    L"",
    {
      Gears_NP_GetEntryPoints,
      Gears_NP_Initialize,
      Gears_NP_Shutdown
    }
  },
#endif
  };

  for (int i = 0; i < arraysize(builtin_plugins); ++i)
    internal_plugins_.push_back(builtin_plugins[i]);
}

void PluginList::GetPluginDirectories(std::vector<FilePath>* plugin_dirs) {
  // We use a set for uniqueness, which we require, over order, which we do not.
  std::set<FilePath> dirs;

  // Load from the application-specific area
  GetAppDirectory(&dirs);

  // Load from the executable area
  GetExeDirectory(&dirs);

  // Load Java
  GetJavaDirectory(&dirs);

  // Load firefox plugins too.  This is mainly to try to locate
  // a pre-installed Flash player.
  GetFirefoxDirectory(&dirs);

  // Firefox hard-codes the paths of some popular plugins to ensure that
  // the plugins are found.  We are going to copy this as well.
  GetAcrobatDirectory(&dirs);
  GetQuicktimeDirectory(&dirs);
  GetWindowsMediaDirectory(&dirs);

  for (std::set<FilePath>::iterator i = dirs.begin(); i != dirs.end(); ++i)
    plugin_dirs->push_back(*i);
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

bool PluginList::ShouldLoadPlugin(const WebPluginInfo& info) {

  // Version check

  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (plugins_[i].path.BaseName() == info.path.BaseName() &&
        !IsNewerVersion(plugins_[i].version, info.version)) {
      return false;  // We already have a loaded plugin whose version is newer.
    }
  }

  // Troublemakers

  std::wstring filename = StringToLowerASCII(info.path.BaseName().value());
  // Depends on XPCOM.
  if (filename == kMozillaActiveXPlugin)
    return false;

  // Disable the Yahoo Application State plugin as it crashes the plugin
  // process on return from NPObjectStub::OnInvoke. Please refer to
  // http://b/issue?id=1372124 for more information.
  if (filename == kYahooApplicationStatePlugin)
    return false;

  // Disable the WangWang protocol handler plugin (npww.dll) as it crashes
  // chrome during shutdown. Firefox also disables this plugin.
  // Please refer to http://code.google.com/p/chromium/issues/detail?id=3953
  // for more information.
  if (filename == kWanWangProtocolHandlerPlugin)
    return false;

  // Special WMP handling

  // We will use the ActiveX shim to handle embedded WMP media.
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
#ifdef GEARS_STATIC_LIB
  LoadPlugin(FilePath(kGearsPluginLibraryName));
#endif

  if (!use_internal_activex_shim_)
    return;

  LoadPlugin(FilePath(kActiveXShimFileName));
  LoadPlugin(FilePath(kActiveXShimFileNameForMediaPlayer));
}

} // namespace NPAPI
