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

namespace google_update {

const wchar_t kRegistryClients[] = L"Software\\Google\\Update\\Clients\\";
const wchar_t kRegistryClientState[] =
    L"Software\\Google\\Update\\ClientState\\";
const wchar_t kRequestParamProductVersion[] = L"pv";
const wchar_t kRequestParamDidRun[] = L"dr";
const wchar_t kRegistryUpdate[] = L"Software\\Google\\Update\\";
const wchar_t kRegistryValueCrashReportPath[] = L"CrashReportPath";
const wchar_t kEnvProductVersionKey[] = L"CHROME_VERSION";

// We're using raw wchar_t everywhere rather than relying on CString or wstring
// to reduce dependencies and make it easier for different apps and libraries
// to use this source code.  For similar reasons, we're avoiding using msvcrt
// functions.  This is also why this is implemented rather than just using
// _tcsdup.
// TODO(erikkay): add unit test for this function
static HRESULT StringCchDup(wchar_t** dst, const wchar_t* src) {
  // TODO(erikkay): ASSERT(src), ASSERT(dst)
  size_t len = 0;
  *dst = NULL;
  HRESULT hr = ::StringCchLength(src, STRSAFE_MAX_CCH, &len);
  if (SUCCEEDED(hr)) {
    len++;
    *dst = new wchar_t[len];
    hr = ::StringCchCopy(*dst, len, src);
    if (FAILED(hr)) {
      delete[] *dst;
      *dst = NULL;
    }
  }
  return hr;
}

static bool FileExists(const wchar_t* filename) {
  WIN32_FILE_ATTRIBUTE_DATA attrs;
  return ::GetFileAttributesEx(filename, GetFileExInfoStandard, &attrs) != 0;
}

// Allocates the out param on success.
static bool GoogleUpdateRegQueryStr(HKEY key, const wchar_t* val,
                                    wchar_t** out) {
  DWORD size = 0;
  LONG ret;
  ret = ::RegQueryValueEx(key, val, NULL, NULL, NULL, &size);
  if (ERROR_SUCCESS == ret) {
    DWORD len = 1 + (size / sizeof(wchar_t));
    *out = new wchar_t[len];
    ret = ::RegQueryValueEx(key, val, NULL, NULL,
                            reinterpret_cast<BYTE* >(*out), &size);
    if (ERROR_SUCCESS == ret) {
      return true;
    } else {
      delete[] *out;
    }
  }
  return false;
}

// Allocates the out param on success.
static bool GoogleUpdateEnvQueryStr(const wchar_t* key_name, wchar_t** out) {
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

const wchar_t* GoogleUpdateClient::GetVersion() const {
  return version_;
}

void GoogleUpdateClient::GetExePathAndInstallMode() {
  dll_path_[0] = 0;
  user_mode_ = true;

  wchar_t buffer[MAX_PATH] = {0};
  DWORD len = ::GetModuleFileName(NULL, dll_path_, MAX_PATH);
  if (!FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                              SHGFP_TYPE_CURRENT, buffer))) {
    if (dll_path_ == wcsstr(dll_path_, buffer)) {
      user_mode_ = false;
    }
  }
  // TODO(erikkay): ASSERT on len == 0
  wchar_t* t = dll_path_ + len - 1;
  while (t >= dll_path_ && *t != L'\\') {
    t--;
  }
  if (t > dll_path_) {
    t++;
    *t = 0;
  }
}

std::wstring GoogleUpdateClient::GetDLLPath() {
  if (FileExists(dll_path_))
    return std::wstring(dll_path_) + L"\\" + dll_;

  // This is not an official build. Find the dll using the default
  // path order in LoadLibrary.
  wchar_t path[MAX_PATH] = {0};
  wchar_t* file_part = NULL;
  DWORD result = ::SearchPath(NULL, dll_, NULL, MAX_PATH, path, &file_part);
  if (result == 0 || result > MAX_PATH)
    return std::wstring();

  return path;
}

bool GoogleUpdateClient::Launch(HINSTANCE instance, HINSTANCE prev_instance,
                                wchar_t* command_line, int show_command,
                                const char* entry_name, int* ret) {
  // TODO(erikkay): ASSERT(guid_)

  bool did_launch = false;
  if (FileExists(dll_path_)) {
    const size_t dll_path_len = wcslen(dll_path_);
    ::SetCurrentDirectory(dll_path_);
    // Setting the version on the environment block is a 'best effort' deal.
    // It enables Google Update running on a child to load the same DLL version.
    ::SetEnvironmentVariableW(kEnvProductVersionKey, version_);
  }

  // The dll can be in the exe's directory or in the current directory.
  // Use the alternate search path to be sure that it's not trying to load it
  // calling application's directory.
  HINSTANCE dll_handle = ::LoadLibraryEx(dll_, NULL,
                                         LOAD_WITH_ALTERED_SEARCH_PATH);
  if (NULL != dll_handle) {
    GoogleUpdateEntry entry = reinterpret_cast<GoogleUpdateEntry>
      (::GetProcAddress(dll_handle, entry_name));
    if (NULL != entry) {
      // record did_run in client state
      HKEY reg_client_state;
      HKEY reg_root = HKEY_LOCAL_MACHINE;
      if (user_mode_) {
        reg_root = HKEY_CURRENT_USER;
      }

      if (::RegOpenKeyEx(reg_root, kRegistryClientState, 0,
                         KEY_READ, &reg_client_state) == ERROR_SUCCESS) {
        HKEY reg_client;
        if (::RegOpenKeyEx(reg_client_state, guid_, 0,
                           KEY_WRITE, &reg_client) == ERROR_SUCCESS) {
          const wchar_t kVal[] = L"1";
          const size_t len = sizeof(kVal);  // we want the size in bytes
          const BYTE *bytes = reinterpret_cast<const BYTE *>(kVal);
          ::RegSetValueEx(reg_client, kRequestParamDidRun, 0, REG_SZ,
                          bytes, len);
          ::RegCloseKey(reg_client);
        }
        ::RegCloseKey(reg_client_state);
      }

      int rc = (entry)(instance, prev_instance, command_line, show_command);
      if (ret) {
        *ret = rc;
      }
      did_launch = true;
    }
    ::FreeLibrary(dll_handle);
  } else {
    unsigned long err = GetLastError();
    if (err) {
      WCHAR message[500] = {0};
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0,
                    reinterpret_cast<LPWSTR>(&message), 500, NULL);
      ::OutputDebugStringW(message);
    }
  }
  return did_launch;
}

bool GoogleUpdateClient::Init(const wchar_t* client_guid, const wchar_t* client_dll) {
  GetExePathAndInstallMode();

  StringCchDup(&guid_, client_guid);
  StringCchDup(&dll_, client_dll);
  // TODO(erikkay): ASSERT(guid_)
  bool ret = false;
  if (guid_) {
    if (GoogleUpdateEnvQueryStr(kEnvProductVersionKey, &version_)) {
      ret = true;
    } else {
      // Look up the version from Google Update registry.
      HKEY reg_clients;
      HKEY reg_root = HKEY_LOCAL_MACHINE;
      if (user_mode_) {
        reg_root = HKEY_CURRENT_USER;
      }
      if (::RegOpenKeyEx(reg_root, kRegistryClients, 0,
                         KEY_READ, &reg_clients) == ERROR_SUCCESS) {
        HKEY reg_client;
        if (::RegOpenKeyEx(reg_clients, guid_, 0, KEY_READ, &reg_client) ==
            ERROR_SUCCESS) {
          if (GoogleUpdateRegQueryStr(reg_client, kRequestParamProductVersion,
                                      &version_)) {
              ret = true;
          }
          ::RegCloseKey(reg_client);
        }
        ::RegCloseKey(reg_clients);
      }
    }
  }

  if (version_) {
    ::StringCchCat(dll_path_, MAX_PATH, version_);
  }
  return ret;
}

GoogleUpdateClient::GoogleUpdateClient()
    : guid_(NULL), dll_(NULL), version_(NULL) {
}

GoogleUpdateClient::~GoogleUpdateClient() {
  delete[] guid_;
  delete[] dll_;
  delete[] version_;
}

}  // namespace google_update
