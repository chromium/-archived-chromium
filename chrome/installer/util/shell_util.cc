// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines functions that integrate Chrome in Windows shell. These
// functions can be used by Chrome as well as Chrome installer. All of the
// work is done by the local functions defined in anonymous namespace in
// this class.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include "shell_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"

#include "installer_util_strings.h"

namespace {

// This class represents a single registry entry. The objective is to
// encapsulate all the registry entries required for registering Chrome at one
// place. This class can not be instantiated outside the class and the objects
// of this class type can be obtained only by calling a static method of this
// class.
class RegistryEntry {
 public:
  // This method returns a list of all the registry entries that are needed
  // to register Chrome.
  static std::list<RegistryEntry*> GetAllEntries(const std::wstring& chrome_exe) {
    std::list<RegistryEntry*> entries;
    std::wstring icon_path(chrome_exe);
    ShellUtil::GetChromeIcon(icon_path);
    std::wstring quoted_exe_path = L"\"" + chrome_exe + L"\"";
    std::wstring open_cmd = quoted_exe_path + L" \"%1\"";

    entries.push_front(new RegistryEntry(L"Software\\Classes\\ChromeHTML",
                                         L"Chrome HTML"));
    entries.push_front(new RegistryEntry(
        L"Software\\Classes\\ChromeHTML\\DefaultIcon", icon_path));
    entries.push_front(new RegistryEntry(
        L"Software\\Classes\\ChromeHTML\\shell\\open\\command", open_cmd));

    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe",
        dist->GetApplicationName()));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\shell\\open\\command",
        quoted_exe_path));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\DefaultIcon",
        icon_path));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\InstallInfo",
        L"ReinstallCommand",
        quoted_exe_path + L" --" + switches::kMakeDefaultBrowser));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\InstallInfo",
        L"HideIconsCommand",
        quoted_exe_path + L" --" + switches::kHideIcons));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\InstallInfo",
        L"ShowIconsCommand",
        quoted_exe_path + L" --" + switches::kShowIcons));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\InstallInfo",
        L"IconsVisible", 1));

    entries.push_front(new RegistryEntry(
        ShellUtil::kRegRegisteredApplications,
        dist->GetApplicationName(),
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities"));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities",
        L"ApplicationDescription", dist->GetApplicationName()));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities",
        L"ApplicationIcon", icon_path));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities",
        L"ApplicationName", dist->GetApplicationName()));

    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities\\StartMenu",
        L"StartMenuInternet", L"chrome.exe"));
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      entries.push_front(new RegistryEntry(
          L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities\\FileAssociations",
          ShellUtil::kFileAssociations[i], ShellUtil::kChromeHTMLProgId));
    }
    for (int i = 0; ShellUtil::kProtocolAssociations[i] != NULL; i++) {
      entries.push_front(new RegistryEntry(
          L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities\\URLAssociations",
          ShellUtil::kProtocolAssociations[i], ShellUtil::kChromeHTMLProgId));
    }
    return entries;
  }

  // Generate work_item tasks required to create current regitry entry and
  // add them to the given work item list.
  void AddToWorkItemList(HKEY root, WorkItemList *items) {
    items->AddCreateRegKeyWorkItem(root, _key_path);
    if (_is_string) {
      items->AddSetRegValueWorkItem(root, _key_path, _name, _value, true);
    } else {
      items->AddSetRegValueWorkItem(root, _key_path, _name, _int_value, true);
    }
  }

  // Check if the current registry entry exists in HKLM registry.
  bool ExistsInHKLM() {
    RegKey key(HKEY_LOCAL_MACHINE, _key_path.c_str());
    bool found = false;
    if (_is_string) {
      std::wstring read_value;
      found = key.ReadValue(_name.c_str(), &read_value) &&
              read_value == _value;
    } else {
      DWORD read_value;
      found = key.ReadValueDW(_name.c_str(), &read_value) &&
              read_value == _int_value;
    }
    key.Close();
    return found;
  }

 private:
  // Create a object that represent default value of a key
  RegistryEntry(const std::wstring& key_path, const std::wstring& value) :
                _key_path(key_path), _name(L""), _value(value),
                _is_string(true) {
  }

  // Create a object that represent a key of type REG_SZ
  RegistryEntry(const std::wstring& key_path, const std::wstring& name,
                const std::wstring& value) : _key_path(key_path),
                _name(name), _value(value), _is_string(true) {
  }

  // Create a object that represent a key of integer type
  RegistryEntry(const std::wstring& key_path, const std::wstring& name,
                DWORD value) : _key_path(key_path),
                _name(name), _int_value(value), _is_string(false) {
  }

  bool _is_string;         // true if current registry entry is of type REG_SZ
  std::wstring _key_path;  // key path for the registry entry
  std::wstring _name;      // name of the registry entry
  std::wstring _value;     // string value (useful if _is_string = true)
  DWORD _int_value;        // integer value (useful if _is_string = false)
}; // class RegistryEntry


// This method checks if Chrome is already registered on the local machine.
// It gets all the required registry entries for Chrome and then checks if
// they exist in HKLM. Returns true if all the entries exist, otherwise false.
bool IsChromeRegistered(const std::wstring& chrome_exe) {
  bool registered = true;
  std::list<RegistryEntry*> entries = RegistryEntry::GetAllEntries(chrome_exe);
  for (std::list<RegistryEntry*>::iterator itr = entries.begin();
      itr != entries.end(); ++itr) {
    if (registered && !(*itr)->ExistsInHKLM())
      registered = false;
    delete (*itr);
  }
  LOG(INFO) << "Check for Chrome registeration returned " << registered;
  return registered;
}


// This method creates the registry entries required for Add/Remove Programs->
// Set Program Access and Defaults, Start->Default Programs on Windows Vista
// and Chrome ProgIds for file extension and protocol handler. root_key is
// the root registry (HKLM or HKCU).
bool SetAccessDefaultRegEntries(HKEY root_key,
                                const std::wstring& chrome_exe) {
  LOG(INFO) << "Registering Chrome browser " << chrome_exe;
  // Create a list of registry entries work items so that we can rollback
  // in case of problem.
  scoped_ptr<WorkItemList> items(WorkItem::CreateWorkItemList());

  std::list<RegistryEntry*> entries = RegistryEntry::GetAllEntries(chrome_exe);
  for (std::list<RegistryEntry*>::iterator itr = entries.begin();
      itr != entries.end(); ++itr) {
    (*itr)->AddToWorkItemList(root_key, items.get());
    delete (*itr);
  }

  // Apply all the registry changes and if there is a problem, rollback.
  if (!items->Do()) {
    LOG(ERROR) << "Failed to add Chrome to Set Program Access and Defaults";
    items->Rollback();
    return false;
  }

  return true;
}


// This method registers Chrome on Vista. It checks if we are currently
// running as admin and if not, it launches setup.exe as administrator which
// will show user standard Vista elevation prompt. If user accepts it
// the new process will make the necessary changes and return SUCCESS that
// we capture and return.
ShellUtil::RegisterStatus RegisterOnVista(const std::wstring& chrome_exe,
                                          bool skip_if_not_admin) {
  if (IsUserAnAdmin() &&
      SetAccessDefaultRegEntries(HKEY_LOCAL_MACHINE, chrome_exe))
    return ShellUtil::SUCCESS;

  if (!skip_if_not_admin) {
    std::wstring exe_path(file_util::GetDirectoryFromPath(chrome_exe));
    file_util::AppendToPath(&exe_path, installer_util::kSetupExe);
    if (!file_util::PathExists(exe_path)) {
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      RegKey key(HKEY_CURRENT_USER, dist->GetUninstallRegPath().c_str());
      key.ReadValue(installer_util::kUninstallStringField, &exe_path);
      exe_path = exe_path.substr(0, exe_path.find_first_of(L" --"));
      TrimString(exe_path, L" \"", &exe_path);
    }
    if (file_util::PathExists(exe_path)) {
      std::wstring params(L"--");
      params.append(installer_util::switches::kRegisterChromeBrowser);
      params.append(L"=\"" + chrome_exe + L"\"");
      DWORD ret_val = ShellUtil::SUCCESS;
      InstallUtil::ExecuteExeAsAdmin(exe_path, params, &ret_val);
      if (ret_val == ShellUtil::SUCCESS)
        return ShellUtil::SUCCESS;
    }
  }
  return ShellUtil::FAILURE;
}

}  // namespace


const wchar_t* ShellUtil::kRegDefaultIcon = L"\\DefaultIcon";
const wchar_t* ShellUtil::kRegShellPath = L"\\shell";
const wchar_t* ShellUtil::kRegShellOpen = L"\\shell\\open\\command";
const wchar_t* ShellUtil::kRegStartMenuInternet =
    L"Software\\Clients\\StartMenuInternet";
const wchar_t* ShellUtil::kRegClasses = L"Software\\Classes";
const wchar_t* ShellUtil::kRegRegisteredApplications =
    L"Software\\RegisteredApplications";
const wchar_t* ShellUtil::kRegShellChromeHTML = L"\\shell\\ChromeHTML";
const wchar_t* ShellUtil::kRegShellChromeHTMLCommand =
    L"\\shell\\ChromeHTML\\command";

const wchar_t* ShellUtil::kChromeHTMLProgId = L"ChromeHTML";
const wchar_t* ShellUtil::kFileAssociations[] = {L".htm", L".html", L".shtml",
    L".xht", L".xhtml", NULL};
const wchar_t* ShellUtil::kProtocolAssociations[] = {L"ftp", L"http", L"https",
    NULL};

ShellUtil::RegisterStatus ShellUtil::AddChromeToSetAccessDefaults(
    const std::wstring& chrome_exe, bool skip_if_not_admin) {
  if (IsChromeRegistered(chrome_exe))
    return ShellUtil::SUCCESS;

  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA)
    return RegisterOnVista(chrome_exe, skip_if_not_admin);

  // Try adding these entries to HKLM first and if that fails try adding
  // to HKCU.
  if (SetAccessDefaultRegEntries(HKEY_LOCAL_MACHINE, chrome_exe))
    return ShellUtil::SUCCESS;

  if (!skip_if_not_admin &&
      SetAccessDefaultRegEntries(HKEY_CURRENT_USER, chrome_exe))
    return ShellUtil::REGISTERED_PER_USER;

  return ShellUtil::FAILURE;
}

bool ShellUtil::GetChromeIcon(std::wstring& chrome_icon) {
  if (chrome_icon.empty())
    return false;

  chrome_icon.append(L",0");
  return true;
}

bool ShellUtil::GetChromeShortcutName(std::wstring* shortcut) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  shortcut->assign(dist->GetApplicationName());
  shortcut->append(L".lnk");
  return true;
}

bool ShellUtil::GetDesktopPath(bool system_level, std::wstring* path) {
  wchar_t desktop[MAX_PATH];
  int dir = system_level ? CSIDL_COMMON_DESKTOPDIRECTORY : CSIDL_DESKTOP;
  if (FAILED(SHGetFolderPath(NULL, dir, NULL, SHGFP_TYPE_CURRENT, desktop)))
    return false;
  *path = desktop;
  return true;
}

bool ShellUtil::GetQuickLaunchPath(bool system_level, std::wstring* path) {
  const static wchar_t* kQuickLaunchPath =
      L"Microsoft\\Internet Explorer\\Quick Launch";
  wchar_t qlaunch[MAX_PATH];
  if (system_level) {
    // We are accessing GetDefaultUserProfileDirectory this way so that we do
    // not have to declare dependency to Userenv.lib for chrome.exe
    typedef BOOL (WINAPI *PROFILE_FUNC)(LPWSTR, LPDWORD);
    HMODULE module = LoadLibrary(L"Userenv.dll");
    PROFILE_FUNC p = reinterpret_cast<PROFILE_FUNC>(GetProcAddress(module,
        "GetDefaultUserProfileDirectoryW"));
    DWORD size = _countof(qlaunch);
    if ((p == NULL) || ((p)(qlaunch, &size) != TRUE))
      return false;
    *path = qlaunch;
    if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
      file_util::AppendToPath(path, L"AppData\\Roaming");
    } else {
      file_util::AppendToPath(path, L"Application Data");
    }
  } else {
    if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
                               SHGFP_TYPE_CURRENT, qlaunch)))
      return false;
    *path = qlaunch;
  }
  file_util::AppendToPath(path, kQuickLaunchPath);
  return true;
}

bool ShellUtil::CreateChromeDesktopShortcut(const std::wstring& chrome_exe,
                                            int shell_change,
                                            bool create_new) {
  std::wstring shortcut_name;
  if (!ShellUtil::GetChromeShortcutName(&shortcut_name))
    return false;

  bool ret = true;
  if (shell_change & ShellUtil::CURRENT_USER) {
    std::wstring shortcut_path;
    if (ShellUtil::GetDesktopPath(false, &shortcut_path)) {
      file_util::AppendToPath(&shortcut_path, shortcut_name);
      ret = ret && ShellUtil::UpdateChromeShortcut(chrome_exe, shortcut_path,
                                                   create_new);
    } else {
      ret = false;
    }
  }

  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring shortcut_path;
    if (ShellUtil::GetDesktopPath(true, &shortcut_path)) {
      file_util::AppendToPath(&shortcut_path, shortcut_name);
      ret = ret && ShellUtil::UpdateChromeShortcut(chrome_exe, shortcut_path,
                                                   create_new);
    } else {
      ret = false;
    }
  }
  return ret;
}

bool ShellUtil::CreateChromeQuickLaunchShortcut(const std::wstring& chrome_exe,
                                                int shell_change,
                                                bool create_new) {
  std::wstring shortcut_name;
  if (!ShellUtil::GetChromeShortcutName(&shortcut_name))
    return false;

  bool ret = true;
  // First create shortcut for the current user.
  if (shell_change & ShellUtil::CURRENT_USER) {
    std::wstring user_ql_path;
    if (ShellUtil::GetQuickLaunchPath(false, &user_ql_path)) {
      file_util::AppendToPath(&user_ql_path, shortcut_name);
      ret = ret && ShellUtil::UpdateChromeShortcut(chrome_exe, user_ql_path,
                                                   create_new);
    } else {
      ret = false;
    }
  }

  // Add a shortcut to Default User's profile so that all new user profiles
  // get it.
  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring default_ql_path;
    if (ShellUtil::GetQuickLaunchPath(true, &default_ql_path)) {
      file_util::AppendToPath(&default_ql_path, shortcut_name);
      ret = ret && ShellUtil::UpdateChromeShortcut(chrome_exe, default_ql_path,
                                                   create_new);
    } else {
      ret = false;
    }
  }

  return ret;
}

bool ShellUtil::RemoveChromeDesktopShortcut(int shell_change) {
  std::wstring shortcut_name;
  if (!ShellUtil::GetChromeShortcutName(&shortcut_name))
    return false;

  bool ret = true;
  if (shell_change & ShellUtil::CURRENT_USER) {
    std::wstring shortcut_path;
    if (ShellUtil::GetDesktopPath(false, &shortcut_path)) {
      file_util::AppendToPath(&shortcut_path, shortcut_name);
      ret = ret && file_util::Delete(shortcut_path, false);
    } else {
      ret = false;
    }
  }

  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring shortcut_path;
    if (ShellUtil::GetDesktopPath(true, &shortcut_path)) {
      file_util::AppendToPath(&shortcut_path, shortcut_name);
      ret = ret && file_util::Delete(shortcut_path, false);
    } else {
      ret = false;
    }
  }
  return ret;
}

bool ShellUtil::RemoveChromeQuickLaunchShortcut(int shell_change) {
  std::wstring shortcut_name;
  if (!ShellUtil::GetChromeShortcutName(&shortcut_name))
    return false;

  bool ret = true;
  // First remove shortcut for the current user.
  if (shell_change & ShellUtil::CURRENT_USER) {
    std::wstring user_ql_path;
    if (ShellUtil::GetQuickLaunchPath(false, &user_ql_path)) {
      file_util::AppendToPath(&user_ql_path, shortcut_name);
      ret = ret && file_util::Delete(user_ql_path, false);
    } else {
      ret = false;
    }
  }

  // Delete shortcut in Default User's profile
  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring default_ql_path;
    if (ShellUtil::GetQuickLaunchPath(true, &default_ql_path)) {
      file_util::AppendToPath(&default_ql_path, shortcut_name);
      ret = ret && file_util::Delete(default_ql_path, false);
    } else {
      ret = false;
    }
  }

  return ret;
}

bool ShellUtil::UpdateChromeShortcut(const std::wstring& chrome_exe,
                                     const std::wstring& shortcut,
                                     bool create_new) {
  std::wstring chrome_path = file_util::GetDirectoryFromPath(chrome_exe);
  if (create_new) {
    return file_util::CreateShortcutLink(chrome_exe.c_str(),      // target
                                         shortcut.c_str(),        // shortcut
                                         chrome_path.c_str(),     // working dir
                                         NULL,                    // arguments
                                         NULL,                    // description
                                         chrome_exe.c_str(),      // icon file
                                         0);                      // icon index
  } else {
    return file_util::UpdateShortcutLink(chrome_exe.c_str(),      // target
                                         shortcut.c_str(),        // shortcut
                                         chrome_path.c_str(),     // working dir
                                         NULL,                    // arguments
                                         NULL,                    // description
                                         chrome_exe.c_str(),      // icon file
                                         0);                      // icon index
  }
}

