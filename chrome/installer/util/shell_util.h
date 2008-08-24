// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares methods that are useful for integrating Chrome in
// Windows shell. These methods are all static and currently part of
// ShellUtil class.

#ifndef CHROME_INSTALLER_UTIL_SHELL_UTIL_H__
#define CHROME_INSTALLER_UTIL_SHELL_UTIL_H__

#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "chrome/installer/util/work_item_list.h"

// This is a utility class that provides common shell integration methods
// that can be used by installer as well as Chrome.
class ShellUtil {
 public:
  // Return value of AddChromeToSetAccessDefaults.
  enum RegisterStatus {
    SUCCESS,             // Registration of Chrome successful (in HKLM)
    FAILURE,             // Registration failed (no changes made)
    REGISTERED_PER_USER  // Registered Chrome as per user (in HKCU)
  };

  // Relative path of DefaultIcon registry entry (prefixed with '\').
  static const wchar_t* kRegDefaultIcon;

  // Relative path of "shell" registry key.
  static const wchar_t* kRegShellPath;

  // Relative path of shell open command in Windows registry
  // (i.e. \\shell\\open\\command).
  static const wchar_t* kRegShellOpen;

  // Relative path of registry key under which applications need to register
  // to control Windows Start menu links.
  static const wchar_t* kRegStartMenuInternet;

  // Relative path of Classes registry entry under which file associations
  // are added on Windows.
  static const wchar_t* kRegClasses;

  // Relative path of RegisteredApplications registry entry under which
  // we add Chrome as a Windows application
  static const wchar_t* kRegRegisteredApplications;

  // Name that we give to Chrome file association handler ProgId.
  static const wchar_t* kChromeHTMLProgId;

  // Relative path of shell Chrome Progid in Windows registry
  // (i.e. \\shell\\ChromeHTML).
  static const wchar_t* kRegShellChromeHTML;

  // Relative path of shell Chrome Progid command in Windows registry
  // (i.e. \\shell\\ChromeHTML\\command).
  static const wchar_t* kRegShellChromeHTMLCommand;

  // File extensions that Chrome registers itself for.
  static const wchar_t* kFileAssociations[];

  // Protocols that Chrome registers itself for.
  static const wchar_t* kProtocolAssociations[];

  // This method adds Chrome to the list that shows up in Add/Remove Programs->
  // Set Program Access and Defaults and also creates Chrome ProgIds under
  // Software\Classes. This method requires write access to HKLM so is just
  // best effort deal. If write to HKLM fails and skip_if_not_admin is false,
  // this method will:
  // - add the ProgId entries to HKCU on XP. HKCU entries will not make
  //   Chrome show in Set Program Access and Defaults but they are still useful
  //   because we can make Chrome run when user clicks on http link or html
  //   file.
  // - will try to launch setup.exe with admin priviledges on Vista to do
  //   these tasks. Users will see standard Vista elevation prompt and if they
  //   enter the right credentials, the write operation will work.
  // Currently skip_if_not_admin is false only when user tries to make Chrome
  // default browser and Chrome is not registered on the machine.
  //
  // chrome_exe: full path to chrome.exe.
  // skip_if_not_admin: if false will make this method try alternate methods
  //                    as described above.
  static RegisterStatus AddChromeToSetAccessDefaults(
      const std::wstring& chrome_exe, bool skip_if_not_admin);

  // This method appends the Chrome icon index inside chrome.exe to the
  // chrome.exe path passed in as input, to generate the full path for
  // Chrome icon that can be used as value for Windows registry keys.
  // chrome_icon: full path to chrome.exe.
  static bool GetChromeIcon(std::wstring& chrome_icon);

  // Returns the localized name of Chrome shortcut.
  static bool GetChromeShortcutName(std::wstring* shortcut);

  // Gets the desktop path for the current user and returns it in 'path'
  // argument. Return true if successful, otherwise returns false.
  static bool GetDesktopPath(std::wstring* path);

  // Gets the Quick Launch shortcuts path for the current user and
  // returns it in 'path' argument. Return true if successful, otherwise
  // returns false.
  static bool GetQuickLaunchPath(std::wstring* path);

  // Updates shortcut (or creates a new shortcut) at destination given by
  // shortcut to a target given by chrome_exe. The arguments is left NULL
  // for the target and icon is set as icon at index 0 from exe.
  // If create_new is set to true, the function will create a new shortcut if
  // if doesn't exist.
  static bool UpdateChromeShortcut(const std::wstring& chrome_exe,
                                   const std::wstring& shortcut,
                                   bool create_new);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(ShellUtil);
};


#endif  // CHROME_INSTALLER_UTIL_SHELL_UTIL_H__

