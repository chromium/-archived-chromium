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

#include "chrome/installer/util/shell_util.h"

#include "base/command_line.h"
#include "base/file_path.h"
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
  static std::list<RegistryEntry*> GetAllEntries(
      const std::wstring& chrome_exe) {
    std::list<RegistryEntry*> entries;
    std::wstring icon_path(chrome_exe);
    ShellUtil::GetChromeIcon(icon_path);
    std::wstring quoted_exe_path = L"\"" + chrome_exe + L"\"";
    std::wstring open_cmd = ShellUtil::GetChromeShellOpenCmd(chrome_exe);

    entries.push_front(new RegistryEntry(L"Software\\Classes\\ChromeHTML",
                                         ShellUtil::kChromeHTMLProgIdDesc));
    entries.push_front(new RegistryEntry(
        L"Software\\Classes\\ChromeHTML\\DefaultIcon", icon_path));
    entries.push_front(new RegistryEntry(
        L"Software\\Classes\\ChromeHTML\\shell\\open\\command", open_cmd));

    std::wstring exe_name = file_util::GetFilenameFromPath(chrome_exe);
    std::wstring app_key = L"Software\\Classes\\Applications\\" + exe_name +
                           L"\\shell\\open\\command";
    entries.push_front(new RegistryEntry(app_key, open_cmd));
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      std::wstring open_with_key(L"Software\\Classes\\");
      open_with_key.append(ShellUtil::kFileAssociations[i]);
      open_with_key.append(L"\\OpenWithList\\" + exe_name);
      entries.push_front(new RegistryEntry(open_with_key, std::wstring()));
    }

    // Chrome extension installer
    std::wstring install_cmd =
        ShellUtil::GetChromeInstallExtensionCmd(chrome_exe);
    std::wstring prog_id = std::wstring(L"Software\\Classes\\") +
        ShellUtil::kChromeExtProgId;

    // Extension file handler
    entries.push_front(new RegistryEntry(prog_id,
                                         ShellUtil::kChromeExtProgIdDesc));
    entries.push_front(new RegistryEntry(
        prog_id + L"\\DefaultIcon", icon_path));
    entries.push_front(new RegistryEntry(
        prog_id + L"\\shell\\open\\command", install_cmd));

    // .crx file type extension
    std::wstring file_extension_key(L"Software\\Classes\\");
    file_extension_key.append(L".");
    file_extension_key.append(chrome::kExtensionFileExtension);
    entries.push_front(new RegistryEntry(file_extension_key,
                                         ShellUtil::kChromeExtProgId));

    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe",
        dist->GetApplicationName()));
    entries.push_front(new RegistryEntry(
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\shell\\open\\"
            L"command",
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
        L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities\\"
            L"StartMenu",
        L"StartMenuInternet", L"chrome.exe"));
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      entries.push_front(new RegistryEntry(
          L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities\\"
              L"FileAssociations",
          ShellUtil::kFileAssociations[i], ShellUtil::kChromeHTMLProgId));
    }
    for (int i = 0; ShellUtil::kProtocolAssociations[i] != NULL; i++) {
      entries.push_front(new RegistryEntry(
          L"Software\\Clients\\StartMenuInternet\\chrome.exe\\Capabilities\\"
              L"URLAssociations",
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
  LOG(INFO) << "Check for Chrome registration returned " << registered;
  return registered;
}

bool BindChromeAssociations(HKEY root_key, const std::wstring& chrome_exe) {
  // Create a list of registry entries to create so that we can rollback
  // in case of problem.
  scoped_ptr<WorkItemList> items(WorkItem::CreateWorkItemList());

  // file extension associations
  std::wstring classes_path(ShellUtil::kRegClasses);
  for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
    std::wstring key_path = classes_path + L"\\" +
                            ShellUtil::kFileAssociations[i];
    items->AddCreateRegKeyWorkItem(root_key, key_path);
    items->AddSetRegValueWorkItem(root_key, key_path, L"",
                                  ShellUtil::kChromeHTMLProgId, true);
  }

  // protocols associations
  std::wstring chrome_open = ShellUtil::GetChromeShellOpenCmd(chrome_exe);
  std::wstring chrome_icon(chrome_exe);
  ShellUtil::GetChromeIcon(chrome_icon);
  for (int i = 0; ShellUtil::kProtocolAssociations[i] != NULL; i++) {
    std::wstring key_path = classes_path + L"\\" +
                            ShellUtil::kProtocolAssociations[i];
    // <root hkey>\Software\Classes\<protocol>\DefaultIcon
    std::wstring icon_path = key_path + ShellUtil::kRegDefaultIcon;
    items->AddCreateRegKeyWorkItem(root_key, icon_path);
    items->AddSetRegValueWorkItem(root_key, icon_path, L"",
                                  chrome_icon, true);
    // <root hkey>\Software\Classes\<protocol>\shell\open\command
    std::wstring shell_path = key_path + ShellUtil::kRegShellOpen;
    items->AddCreateRegKeyWorkItem(root_key, shell_path);
    items->AddSetRegValueWorkItem(root_key, shell_path, L"",
                                  chrome_open, true);
    // <root hkey>\Software\Classes\<protocol>\shell\open\ddeexec
    std::wstring dde_path = key_path + L"\\shell\\open\\ddeexec";
    items->AddCreateRegKeyWorkItem(root_key, dde_path);
    items->AddSetRegValueWorkItem(root_key, dde_path, L"", L"", true);
    // <root hkey>\Software\Classes\<protocol>\shell\@
    std::wstring protocol_shell_path = key_path + ShellUtil::kRegShellPath;
    items->AddSetRegValueWorkItem(root_key, protocol_shell_path, L"",
                                  L"open", true);
  }

  // start->Internet shortcut.
  std::wstring exe_name = file_util::GetFilenameFromPath(chrome_exe);
  std::wstring start_internet(ShellUtil::kRegStartMenuInternet);
  items->AddCreateRegKeyWorkItem(root_key, start_internet);
  items->AddSetRegValueWorkItem(root_key, start_internet, L"",
                                exe_name, true);

  // Apply all the registry changes and if there is a problem, rollback
  if (!items->Do()) {
    LOG(ERROR) << "Error while registering Chrome as default browser";
    items->Rollback();
    return false;
  }
  return true;
}

// Populate work_item_list with WorkItem entries that will add chrome.exe to
// the set of App Paths registry keys so that ShellExecute can find it. Note
// that this is done in HKLM, regardless of whether this is a single-user
// install or not. For non-admin users, this will fail.
// chrome_exe: full path to chrome.exe
// work_item_list: pointer to the WorkItemList that will be populated
void AddChromeAppPathWorkItems(const std::wstring& chrome_exe,
                               WorkItemList* item_list) {
  FilePath chrome_path(chrome_exe);
  std::wstring app_path_key(ShellUtil::kAppPathsRegistryKey);
  file_util::AppendToPath(&app_path_key, chrome_path.BaseName().value());
  item_list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, app_path_key);
  item_list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, app_path_key, L"",
                                    chrome_exe, true);
  item_list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, app_path_key,
                                    ShellUtil::kAppPathsRegistryPathName,
                                    chrome_path.DirName().value(), true);
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

  // Append the App Paths registry entries. Do this only if we are an admin,
  // since they are always written to HKLM.
  if (IsUserAnAdmin())
    AddChromeAppPathWorkItems(chrome_exe, items.get());

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
      CommandLine command_line(L"");
      command_line.ParseFromString(exe_path);
      exe_path = command_line.program();
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

// Remove unnecessary "URL Protocol" entry from shell registration.  This value
// was written by older installers so ignoring error conditions.
void RemoveUrlProtocol(HKEY root) {
  RegKey key(root, L"Software\\Classes\\ChromeHTML", KEY_ALL_ACCESS);
  key.DeleteValue(ShellUtil::kRegUrlProtocol);
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
const wchar_t* ShellUtil::kRegVistaUrlPrefs =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\"
    L"http\\UserChoice";
const wchar_t* ShellUtil::kAppPathsRegistryKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";
const wchar_t* ShellUtil::kAppPathsRegistryPathName = L"Path";

const wchar_t* ShellUtil::kChromeHTMLProgId = L"ChromeHTML";
const wchar_t* ShellUtil::kChromeHTMLProgIdDesc = L"Chrome HTML";
const wchar_t* ShellUtil::kFileAssociations[] = {L".htm", L".html", L".shtml",
    L".xht", L".xhtml", NULL};
const wchar_t* ShellUtil::kProtocolAssociations[] = {L"ftp", L"http", L"https",
    NULL};
const wchar_t* ShellUtil::kRegUrlProtocol = L"URL Protocol";

const wchar_t* ShellUtil::kChromeExtProgId = L"ChromeExt";
const wchar_t* ShellUtil::kChromeExtProgIdDesc = L"Chrome Extension Installer";

ShellUtil::RegisterStatus ShellUtil::AddChromeToSetAccessDefaults(
    const std::wstring& chrome_exe, bool skip_if_not_admin) {
  RemoveUrlProtocol(HKEY_LOCAL_MACHINE);
  RemoveUrlProtocol(HKEY_CURRENT_USER);

  if (IsChromeRegistered(chrome_exe))
    return ShellUtil::SUCCESS;

  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA)
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

std::wstring ShellUtil::GetChromeShellOpenCmd(const std::wstring& chrome_exe) {
  return L"\"" + chrome_exe + L"\" -- \"%1\"";
}

std::wstring ShellUtil::GetChromeInstallExtensionCmd(
    const std::wstring& chrome_exe) {
  return L"\"" + chrome_exe + L"\" --install-extension=\"%1\"";
}

bool ShellUtil::GetChromeShortcutName(std::wstring* shortcut) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  shortcut->assign(dist->GetApplicationName());
  shortcut->append(L".lnk");
  return true;
}

bool ShellUtil::GetDesktopPath(bool system_level, std::wstring* path) {
  wchar_t desktop[MAX_PATH];
  int dir = system_level ? CSIDL_COMMON_DESKTOPDIRECTORY :
                           CSIDL_DESKTOPDIRECTORY;
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
    if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
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
                                            const std::wstring& description,
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
      ret = ShellUtil::UpdateChromeShortcut(chrome_exe, shortcut_path,
                                            description, create_new);
    } else {
      ret = false;
    }
  }
  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring shortcut_path;
    if (ShellUtil::GetDesktopPath(true, &shortcut_path)) {
      file_util::AppendToPath(&shortcut_path, shortcut_name);
      // Note we need to call the create operation and then AND the result
      // with the create operation of user level shortcut.
      ret = ShellUtil::UpdateChromeShortcut(chrome_exe, shortcut_path,
                                            description, create_new) && ret;
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
      ret = ShellUtil::UpdateChromeShortcut(chrome_exe, user_ql_path,
                                            L"", create_new);
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
      ret = ShellUtil::UpdateChromeShortcut(chrome_exe, default_ql_path,
                                            L"", create_new) && ret;
    } else {
      ret = false;
    }
  }

  return ret;
}

bool ShellUtil::MakeChromeDefault(int shell_change,
                                  const std::wstring chrome_exe) {
  bool ret = true;
  // First use the new "recommended" way on Vista to make Chrome default
  // browser.
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    LOG(INFO) << "Registering Chrome as default browser on Vista.";
    IApplicationAssociationRegistration* pAAR;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration,
        NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration),
        (void**)&pAAR);
    if (SUCCEEDED(hr)) {
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      hr = pAAR->SetAppAsDefaultAll(dist->GetApplicationName().c_str());
      pAAR->Release();
    }
    if (!SUCCEEDED(hr)) {
      ret = false;
      LOG(ERROR) << "Could not make Chrome default browser.";
    }
  }

  // Now use the old way to associate Chrome with supported protocols and file
  // associations. This should not be required on Vista but since some
  // applications still read Software\Classes\http key directly, we have to do
  // this on Vista also.
  // Change the default browser for current user.
  if ((shell_change & ShellUtil::CURRENT_USER) &&
      !BindChromeAssociations(HKEY_CURRENT_USER, chrome_exe))
    ret = false;

  // Chrome as default browser at system level.
  if ((shell_change & ShellUtil::SYSTEM_LEVEL) &&
      !BindChromeAssociations(HKEY_LOCAL_MACHINE, chrome_exe))
    ret = false;

  // Send Windows notification event so that it can update icons for
  // file associations.
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
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
      ret = file_util::Delete(shortcut_path, false);
    } else {
      ret = false;
    }
  }

  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring shortcut_path;
    if (ShellUtil::GetDesktopPath(true, &shortcut_path)) {
      file_util::AppendToPath(&shortcut_path, shortcut_name);
      ret = file_util::Delete(shortcut_path, false) && ret;
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
      ret = file_util::Delete(user_ql_path, false);
    } else {
      ret = false;
    }
  }

  // Delete shortcut in Default User's profile
  if (shell_change & ShellUtil::SYSTEM_LEVEL) {
    std::wstring default_ql_path;
    if (ShellUtil::GetQuickLaunchPath(true, &default_ql_path)) {
      file_util::AppendToPath(&default_ql_path, shortcut_name);
      ret = file_util::Delete(default_ql_path, false) && ret;
    } else {
      ret = false;
    }
  }

  return ret;
}

bool ShellUtil::UpdateChromeShortcut(const std::wstring& chrome_exe,
                                     const std::wstring& shortcut,
                                     const std::wstring& description,
                                     bool create_new) {
  std::wstring chrome_path = file_util::GetDirectoryFromPath(chrome_exe);
  if (create_new) {
    return file_util::CreateShortcutLink(chrome_exe.c_str(),      // target
                                         shortcut.c_str(),        // shortcut
                                         chrome_path.c_str(),     // working dir
                                         NULL,                    // arguments
                                         description.c_str(),     // description
                                         chrome_exe.c_str(),      // icon file
                                         0);                      // icon index
  } else {
    return file_util::UpdateShortcutLink(chrome_exe.c_str(),      // target
                                         shortcut.c_str(),        // shortcut
                                         chrome_path.c_str(),     // working dir
                                         NULL,                    // arguments
                                         description.c_str(),     // description
                                         chrome_exe.c_str(),      // icon file
                                         0);                      // icon index
  }
}

