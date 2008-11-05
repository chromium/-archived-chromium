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

namespace {

const wchar_t kAppInstallKey[] =
    L"Applications\\chrome.exe\\shell\\open\\command";

// Append to an extension (preceeded by a dot) to add us to the "Open With"
// list for a file. For example ".html".
const wchar_t kOpenWithUs[] = L"\\OpenWithList\\chrome.exe";

// Wait this long after startup before verifying registry keys.
const int kVerifyTimeoutMs = 5000;

bool VerifyApplicationKey();
bool VerifyAssociations();

// Include the dot in the extension.
bool AddToOpenWithList(const wchar_t* extension);

// There should be a key HKEY_CLASSES_ROOT\Applications\<appname>, the
// OpenWithList for files refers to this key
bool VerifyApplicationKey() {
  // we want to make Applications\<appname>\shell\open\command = <path> "%1"
  RegKey key(HKEY_CLASSES_ROOT, kAppInstallKey, KEY_WRITE);
  if (!key.Valid())
    return false;

  std::wstring app_path;
  if (!PathService::Get(base::FILE_EXE, &app_path))
    return false;
  app_path.append(L" -- \"%1\"");
  return key.WriteValue(NULL, app_path.c_str());
}

// This just checks that we are installed as a handler for HTML files. We
// don't currently check for defaultness, only that we appear in the
// "Open With" list. This will need to become more elaborate in the future.
bool VerifyAssociations() {
  if (!AddToOpenWithList(L".html"))
    return false;
  if (!AddToOpenWithList(L".htm"))
    return false;
  return true;
}

bool AddToOpenWithList(const wchar_t* extension) {
  std::wstring path(extension);
  path.append(kOpenWithUs);
  RegKey key(HKEY_CLASSES_ROOT, path.c_str(), KEY_WRITE);
  return key.Valid();
}

class InstallationVerifyTask : public Task {
 public:
  virtual void Run() {
    ShellIntegration::VerifyInstallationNow();
  }
};

const wchar_t kVistaUrlPrefs[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice";

}  // namespace

void ShellIntegration::VerifyInstallation() {
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new InstallationVerifyTask(),
                                          kVerifyTimeoutMs);
}

bool ShellIntegration::VerifyInstallationNow() {
  // Currently we only install ourselves as a verb for HTML files, and not as
  // the default handler. We don't prompt the user. In the future, we will
  // want to set as the default and prompt the user if something changed. We
  // will also care about more file types.
  //
  // MSDN's description of file associations:
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_basics/shell_basics_extending/fileassociations/fileassoc.asp
  if (!VerifyApplicationKey())
    return false;
  return VerifyAssociations();
}

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
      CommandLine command_line(value);
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
    RegKey key(HKEY_CURRENT_USER, kVistaUrlPrefs, KEY_READ);
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

