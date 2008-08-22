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

#include "chrome/app/google_update_client.h"

#include <shlobj.h>
#include <strsafe.h>

#include "chrome/app/client_util.h"

namespace {
const wchar_t kRegistryClients[] = L"Software\\Google\\Update\\Clients\\";
const wchar_t kRegistryClientState[] =
    L"Software\\Google\\Update\\ClientState\\";
const wchar_t kRequestParamDidRun[] = L"dr";
const wchar_t kRegistryUpdate[] = L"Software\\Google\\Update\\";
const wchar_t kRegistryValueCrashReportPath[] = L"CrashReportPath";
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
  if (client_util::FileExists(dll_path_))
    return std::wstring(dll_path_) + L"\\" + dll_;

  // This is not an official build. Find the dll using the default
  // path order in LoadLibrary.
  wchar_t path[MAX_PATH] = {0};
  wchar_t* file_part = NULL;
  DWORD result = ::SearchPath(NULL, dll_.c_str(), NULL, MAX_PATH,
                              path, &file_part);
  if (result == 0 || result > MAX_PATH)
    return std::wstring();

  return path;
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
    std::wstring key_path = kRegistryClientState + guid_;
    HKEY reg_key;
    if (::RegOpenKeyEx(reg_root, key_path.c_str(), 0,
                       KEY_WRITE, &reg_key) == ERROR_SUCCESS) {
      const wchar_t kVal[] = L"1";
      ::RegSetValueEx(reg_key, kRequestParamDidRun, 0, REG_SZ,
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
  user_mode_ = client_util::IsUserModeInstall(dll_path_);

  guid_.assign(client_guid);
  dll_.assign(client_dll);
  bool ret = false;
  if (!guid_.empty()) {
    if (GoogleUpdateEnvQueryStr(kEnvProductVersionKey, &version_)) {
      ret = true;
    } else {
      std::wstring key(kRegistryClients);
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
