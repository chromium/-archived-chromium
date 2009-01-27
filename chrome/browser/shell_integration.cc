// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "chrome/browser/shell_integration.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/win_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

bool ShellIntegration::SetAsDefaultBrowser() {
  std::wstring chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    LOG(ERROR) << "Error getting app exe path";
    return false;
  }

  ShellUtil::RegisterStatus register_status =
      ShellUtil::AddChromeToSetAccessDefaults(chrome_exe, false);
  if (register_status == ShellUtil::FAILURE) {
    LOG(ERROR) << "Chrome could not be registered on the machine.";
    return false;
  }

  // From UI currently we only allow setting default browser for current user.
  if (!ShellUtil::MakeChromeDefault(ShellUtil::CURRENT_USER, chrome_exe)) {
    LOG(ERROR) << "Chrome could not be set as default browser.";
    return false;
  }

  LOG(INFO) << "Chrome registered as default browser.";
  return true;
}

bool ShellIntegration::IsDefaultBrowser() {
  // First determine the app path. If we can't determine what that is, we have
  // bigger fish to fry...
  std::wstring app_path;
  if (!PathService::Get(base::FILE_EXE, &app_path)) {
    LOG(ERROR) << "Error getting app exe path";
    return false;
  }
  // When we check for default browser we don't necessarily want to count file
  // type handlers and icons as having changed the default browser status,
  // since the user may have changed their shell settings to cause HTML files
  // to open with a text editor for example. We also don't want to aggressively
  // claim FTP, since the user may have a separate FTP client. It is an open
  // question as to how to "heal" these settings. Perhaps the user should just
  // re-run the installer or run with the --set-default-browser command line
  // flag. There is doubtless some other key we can hook into to cause "Repair"
  // to show up in Add/Remove programs for us.
  const std::wstring kChromeProtocols[] = {L"http", L"https"};

  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
    IApplicationAssociationRegistration* pAAR;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration,
        NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration),
        (void**)&pAAR);
    if (!SUCCEEDED(hr))
      return false;

    for (int i = 0; i < _countof(kChromeProtocols); i++) {
      BOOL result = TRUE;
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      hr = pAAR->QueryAppIsDefault(kChromeProtocols[i].c_str(), AT_URLPROTOCOL,
        AL_EFFECTIVE, dist->GetApplicationName().c_str(), &result);
      if (!SUCCEEDED(hr) || (result == FALSE)) {
        pAAR->Release();
        return false;
      }
    }
    pAAR->Release();
  } else {
    std::wstring short_app_path;
    GetShortPathName(app_path.c_str(), WriteInto(&short_app_path, MAX_PATH),
                     MAX_PATH);

    // open command for protocol associations
    for (int i = 0; i < _countof(kChromeProtocols); i++) {
      // Check in HKEY_CLASSES_ROOT that is the result of merge between
      // HKLM and HKCU
      HKEY root_key = HKEY_CLASSES_ROOT;
      // Check <protocol>\shell\open\command
      std::wstring key_path(kChromeProtocols[i] + ShellUtil::kRegShellOpen);
      RegKey key(root_key, key_path.c_str(), KEY_READ);
      std::wstring value;
      if (!key.Valid() || !key.ReadValue(L"", &value))
        return false;
      // Need to normalize path in case it's been munged.
      CommandLine command_line(L"");
      command_line.ParseFromString(value);
      std::wstring short_path;
      GetShortPathName(command_line.program().c_str(),
                       WriteInto(&short_path, MAX_PATH), MAX_PATH);
      if (short_path != short_app_path)
        return false;
    }
  }
  return true;
}

// There is no reliable way to say which browser is default on a machine (each
// browser can have some of the protocols/shortcuts). So we look for only HTTP
// protocol handler. Even this handler is located at different places in
// registry on XP and Vista:
// - HKCR\http\shell\open\command (XP)
// - HKCU\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\
//   http\UserChoice (Vista)
// This method checks if Firefox is defualt browser by checking these
// locations and returns true if Firefox traces are found there. In case of
// error (or if Firefox is not found)it returns the default value which
// is false.
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  bool ff_default = false;
  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
    std::wstring app_cmd;
    RegKey key(HKEY_CURRENT_USER, ShellUtil::kRegVistaUrlPrefs, KEY_READ);
    if (key.Valid() && key.ReadValue(L"Progid", &app_cmd) &&
        app_cmd == L"FirefoxURL")
      ff_default = true;
  } else {
    std::wstring key_path(L"http");
    key_path.append(ShellUtil::kRegShellOpen);
    RegKey key(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ);
    std::wstring app_cmd;
    if (key.Valid() && key.ReadValue(L"", &app_cmd) &&
        std::wstring::npos != StringToLowerASCII(app_cmd).find(L"firefox"))
      ff_default = true;
  }
  return ff_default;
}

