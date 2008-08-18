// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
  app_path.append(L" \"%1\"");
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

  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
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
    if (!SUCCEEDED(hr))
      LOG(ERROR) << "Could not make Chrome default browser.";
  } else {
    // when we support system wide installs this will need to change to HKLM
    HKEY root_key = HKEY_CURRENT_USER;
    // Create a list of registry entries to create so that we can rollback
    // in case of problem.
    scoped_ptr<WorkItemList> items(WorkItem::CreateWorkItemList());
    std::wstring classes_path(ShellUtil::kRegClasses);

    std::wstring exe_name = file_util::GetFilenameFromPath(chrome_exe);
    std::wstring chrome_open = L"\"" + chrome_exe + L"\" \"%1\"";
    std::wstring chrome_icon(chrome_exe);
    ShellUtil::GetChromeIcon(chrome_icon);

    // Create Software\Classes\ChromeHTML
    std::wstring html_prog_id = classes_path + L"\\" +
                                ShellUtil::kChromeHTMLProgId;
    items->AddCreateRegKeyWorkItem(root_key, html_prog_id);
    std::wstring default_icon = html_prog_id + ShellUtil::kRegDefaultIcon;
    items->AddCreateRegKeyWorkItem(root_key, default_icon);
    items->AddSetRegValueWorkItem(root_key, default_icon, L"",
                                  chrome_icon, true);
    std::wstring open_cmd = html_prog_id + ShellUtil::kRegShellOpen;
    items->AddCreateRegKeyWorkItem(root_key, open_cmd);
    items->AddSetRegValueWorkItem(root_key, open_cmd, L"",
                                  chrome_open, true);

    // file extension associations
    for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; i++) {
      std::wstring key_path = classes_path + L"\\" +
                              ShellUtil::kFileAssociations[i];
      items->AddCreateRegKeyWorkItem(root_key, key_path);
      items->AddSetRegValueWorkItem(root_key, key_path, L"",
                                    ShellUtil::kChromeHTMLProgId, true);
    }

    // protocols associations
    for (int i = 0; ShellUtil::kProtocolAssociations[i] != NULL; i++) {
      std::wstring key_path = classes_path + L"\\" +
                              ShellUtil::kProtocolAssociations[i];
      // HKCU\Software\Classes\<protocol>\DefaultIcon
      std::wstring icon_path = key_path + ShellUtil::kRegDefaultIcon;
      items->AddCreateRegKeyWorkItem(root_key, icon_path);
      items->AddSetRegValueWorkItem(root_key, icon_path, L"",
                                    chrome_icon, true);
      // HKCU\Software\Classes\<protocol>\shell\open\command
      std::wstring shell_path = key_path + ShellUtil::kRegShellOpen;
      items->AddCreateRegKeyWorkItem(root_key, shell_path);
      items->AddSetRegValueWorkItem(root_key, shell_path, L"",
                                    chrome_open, true);
      // HKCU\Software\Classes\<protocol>\shell\open\ddeexec
      std::wstring dde_path = key_path + L"\\shell\\open\\ddeexec";
      items->AddCreateRegKeyWorkItem(root_key, dde_path);
      items->AddSetRegValueWorkItem(root_key, dde_path, L"", L"", true);
      // HKCU\Software\Classes\<protocol>\shell\@
      std::wstring protocol_shell_path = key_path + ShellUtil::kRegShellPath;
      items->AddSetRegValueWorkItem(root_key, protocol_shell_path, L"",
                                    L"open", true);
    }

    // start->Internet shortcut. This works only if we have already
    // added needed entries in HKLM registry. So unless the Chrome
    // registration status is SUCCESS don't bother.
    if (register_status == ShellUtil::SUCCESS) {
      std::wstring start_internet(ShellUtil::kRegStartMenuInternet);
      items->AddCreateRegKeyWorkItem(root_key, start_internet);
      items->AddSetRegValueWorkItem(root_key, start_internet, L"",
                                    exe_name, true);
    }

    // Apply all the registry changes and if there is a problem, rollback
    if (!items->Do()) {
      LOG(ERROR) << "Error while registering Chrome as default browser";
      items->Rollback();
      return false;
    }
  }

  // Send Windows notification event so that it can update icons for
  // file associations.
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

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
      // For now, we only ever set in HKCU, so no need to check HKLM since we'll
      // never be present there...
      HKEY root_key = HKEY_CURRENT_USER;

      // Check Software\Classes\<protocol>\shell\open\command
      std::wstring key_path(ShellUtil::kRegClasses);
      key_path.append(L"\\" + kChromeProtocols[i] + ShellUtil::kRegShellOpen);
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
