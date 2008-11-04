// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/google_update_client.h"

#include <shlobj.h>
#include <strsafe.h>

#include "chrome/app/client_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"

namespace {
const wchar_t kEnvProductVersionKey[] = L"CHROME_VERSION";

// Allocates the out param on success.
bool GoogleUpdateEnvQueryStr(const wchar_t* key_name, wchar_t** out) {
  DWORD count = ::GetEnvironmentVariableW(key_name, NULL, 0);
  if (!count) {
    return false;
  }
  wchar_t* value = new wchar_t[count + 1];
  if (!::GetEnvironmentVariableW(key_name, value, count)) {
    delete[] value;
    return false;
  }
  *out = value;
  return true;
}
}  // anonymous namespace

namespace google_update {

GoogleUpdateClient::GoogleUpdateClient() : version_(NULL) {
}

GoogleUpdateClient::~GoogleUpdateClient() {
  delete[] version_;
}

std::wstring GoogleUpdateClient::GetDLLPath() {
  return client_util::GetDLLPath(dll_, dll_path_);
}

const wchar_t* GoogleUpdateClient::GetVersion() const {
  return version_;
}

bool GoogleUpdateClient::Launch(HINSTANCE instance,
                                sandbox::SandboxInterfaceInfo* sandbox,
                                wchar_t* command_line,
                                int show_command,
                                const char* entry_name,
                                int* ret) {
  if (client_util::FileExists(dll_path_)) {
    ::SetCurrentDirectory(dll_path_);
    // Setting the version on the environment block is a 'best effort' deal.
    // It enables Google Update running on a child to load the same DLL version.
    ::SetEnvironmentVariableW(kEnvProductVersionKey, version_);
  }

  // The dll can be in the exe's directory or in the current directory.
  // Use the alternate search path to be sure that it's not trying to load it
  // calling application's directory.
  HINSTANCE dll_handle = ::LoadLibraryEx(dll_.c_str(), NULL,
                                         LOAD_WITH_ALTERED_SEARCH_PATH);
  if (NULL == dll_handle) {
    unsigned long err = GetLastError();
    if (err) {
      WCHAR message[500] = {0};
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0,
                    reinterpret_cast<LPWSTR>(&message), 500, NULL);
      ::OutputDebugStringW(message);
    }
    return false;
  }

  bool did_launch = false;
  client_util::DLL_MAIN entry = reinterpret_cast<client_util::DLL_MAIN>(
      ::GetProcAddress(dll_handle, entry_name));
  if (NULL != entry) {
    // record did_run "dr" in client state
    HKEY reg_root = (user_mode_) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    std::wstring key_path = google_update::kRegPathClientState + guid_;
    HKEY reg_key;
    if (::RegOpenKeyEx(reg_root, key_path.c_str(), 0,
                       KEY_WRITE, &reg_key) == ERROR_SUCCESS) {
      const wchar_t kVal[] = L"1";
      ::RegSetValueEx(reg_key, google_update::kRegDidRunField, 0, REG_SZ,
                      reinterpret_cast<const BYTE *>(kVal), sizeof(kVal));
      ::RegCloseKey(reg_key);
    }

    int rc = (entry)(instance, sandbox, command_line, show_command);
    if (ret) {
      *ret = rc;
    }
    did_launch = true;
  }
#ifdef PURIFY
  // We should never unload the dll. There is only risk and no gain from
  // doing so. The singleton dtors have been already run by AtExitManager.
  ::FreeLibrary(dll_handle);
#endif
  return did_launch;
}

bool GoogleUpdateClient::Init(const wchar_t* client_guid,
                              const wchar_t* client_dll) {
  client_util::GetExecutablePath(dll_path_);
  user_mode_ = InstallUtil::IsPerUserInstall(dll_path_);

  guid_.assign(client_guid);
  dll_.assign(client_dll);
  bool ret = false;
  if (!guid_.empty()) {
    if (GoogleUpdateEnvQueryStr(kEnvProductVersionKey, &version_)) {
      ret = true;
    } else {
      std::wstring key(google_update::kRegPathClients);
      key.append(guid_);
      if (client_util::GetChromiumVersion(dll_path_, key.c_str(), &version_))
        ret = true;
    }
  }

  if (version_) {
    ::StringCchCat(dll_path_, MAX_PATH, version_);
  }
  return ret;
}
}  // namespace google_update

