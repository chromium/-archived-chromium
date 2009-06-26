// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_importer_utils.h"

#include <shlobj.h>

#include "base/registry.h"

namespace {

typedef BOOL (WINAPI* SetDllDirectoryFunc)(LPCTSTR lpPathName);

// A helper class whose destructor calls SetDllDirectory(NULL) to undo the
// effects of a previous SetDllDirectory call.
class SetDllDirectoryCaller {
 public:
  explicit SetDllDirectoryCaller() : func_(NULL) { }

  ~SetDllDirectoryCaller() {
    if (func_)
      func_(NULL);
  }

  // Sets the SetDllDirectory function pointer to activates this object.
  void set_func(SetDllDirectoryFunc func) { func_ = func; }

 private:
  SetDllDirectoryFunc func_;
};

}  // namespace

// NOTE: Keep these in order since we need test all those paths according
// to priority. For example. One machine has multiple users. One non-admin
// user installs Firefox 2, which causes there is a Firefox2 entry under HKCU.
// One admin user installs Firefox 3, which causes there is a Firefox 3 entry
// under HKLM. So when the non-admin user log in, we should deal with Firefox 2
// related data instead of Firefox 3.
static const HKEY kFireFoxRegistryPaths[] = {
  HKEY_CURRENT_USER,
  HKEY_LOCAL_MACHINE
};

int GetCurrentFirefoxMajorVersionFromRegistry() {
  TCHAR ver_buffer[128];
  DWORD ver_buffer_length = sizeof(ver_buffer);
  int highest_version = 0;
  // When installing Firefox with admin account, the product keys will be
  // written under HKLM\Mozilla. Otherwise it the keys will be written under
  // HKCU\Mozilla.
  for (int i = 0; i < arraysize(kFireFoxRegistryPaths); ++i) {
    bool result = ReadFromRegistry(kFireFoxRegistryPaths[i],
        L"Software\\Mozilla\\Mozilla Firefox",
        L"CurrentVersion", ver_buffer, &ver_buffer_length);
    if (!result)
      continue;
    highest_version = std::max(highest_version, _wtoi(ver_buffer));
  }
  return highest_version;
}

std::wstring GetFirefoxInstallPathFromRegistry() {
  // Detects the path that Firefox is installed in.
  std::wstring registry_path = L"Software\\Mozilla\\Mozilla Firefox";
  TCHAR buffer[MAX_PATH];
  DWORD buffer_length = sizeof(buffer);
  bool result;
  result = ReadFromRegistry(HKEY_LOCAL_MACHINE, registry_path.c_str(),
                            L"CurrentVersion", buffer, &buffer_length);
  if (!result)
    return std::wstring();
  registry_path += L"\\" + std::wstring(buffer) + L"\\Main";
  buffer_length = sizeof(buffer);
  result = ReadFromRegistry(HKEY_LOCAL_MACHINE, registry_path.c_str(),
                            L"Install Directory", buffer, &buffer_length);
  if (!result)
    return std::wstring();
  return buffer;
}

FilePath GetProfilesINI() {
  FilePath ini_file;
  // The default location of the profile folder containing user data is
  // under the "Application Data" folder in Windows XP.
  wchar_t buffer[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
                                SHGFP_TYPE_CURRENT, buffer))) {
    ini_file = FilePath(buffer).Append(L"Mozilla\\Firefox\\profiles.ini");
  }
  if (file_util::PathExists(ini_file))
    return ini_file;

  return FilePath();
}

// static
const wchar_t NSSDecryptor::kNSS3Library[] = L"nss3.dll";
const wchar_t NSSDecryptor::kSoftokn3Library[] = L"softokn3.dll";
const wchar_t NSSDecryptor::kPLDS4Library[] = L"plds4.dll";
const wchar_t NSSDecryptor::kNSPR4Library[] = L"nspr4.dll";

bool NSSDecryptor::Init(const std::wstring& dll_path,
                        const std::wstring& db_path) {
  // We call SetDllDirectory to work around a Purify bug (GetModuleHandle
  // fails inside Purify under certain conditions).  SetDllDirectory only
  // exists on Windows XP SP1 or later, so we look up its address at run time.
  HMODULE kernel32_dll = GetModuleHandle(L"kernel32.dll");
  if (kernel32_dll == NULL)
    return false;
  SetDllDirectoryFunc set_dll_directory =
      (SetDllDirectoryFunc)GetProcAddress(kernel32_dll, "SetDllDirectoryW");
  SetDllDirectoryCaller caller;

  if (set_dll_directory != NULL) {
    if (!set_dll_directory(dll_path.c_str()))
      return false;
    caller.set_func(set_dll_directory);
    nss3_dll_ = LoadLibrary(kNSS3Library);
    if (nss3_dll_ == NULL)
      return false;
  } else {
    // Fall back on LoadLibraryEx if SetDllDirectory isn't available.  We
    // actually prefer this method because it doesn't change the DLL search
    // path, which is a process-wide property.
    std::wstring path = dll_path;
    file_util::AppendToPath(&path, kNSS3Library);
    nss3_dll_ = LoadLibraryEx(path.c_str(), NULL,
                              LOAD_WITH_ALTERED_SEARCH_PATH);
    if (nss3_dll_ == NULL)
      return false;

    // Firefox 2 uses NSS 3.11.  Firefox 3 uses NSS 3.12.  NSS 3.12 has two
    // changes in its DLLs:
    // 1. nss3.dll is not linked with softokn3.dll at build time, but rather
    //    loads softokn3.dll using LoadLibrary in NSS_Init.
    // 2. softokn3.dll has a new dependency sqlite3.dll.
    // NSS_Init's LoadLibrary call has trouble finding sqlite3.dll.  To help
    // it out, we preload softokn3.dll using LoadLibraryEx with the
    // LOAD_WITH_ALTERED_SEARCH_PATH flag.  This helps because LoadLibrary
    // doesn't load a DLL again if it's already loaded.  This workaround is
    // harmless for NSS 3.11.
    path = dll_path;
    file_util::AppendToPath(&path, kSoftokn3Library);
    softokn3_dll_ = LoadLibraryEx(path.c_str(), NULL,
                                  LOAD_WITH_ALTERED_SEARCH_PATH);
    if (softokn3_dll_ == NULL) {
      Free();
      return false;
    }
  }
  HMODULE plds4_dll = GetModuleHandle(kPLDS4Library);
  HMODULE nspr4_dll = GetModuleHandle(kNSPR4Library);

  return InitNSS(db_path, plds4_dll, nspr4_dll);
}
