// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi.h"

#include <windows.h>
#include <stdlib.h>
#include <strsafe.h>

namespace {

const wchar_t kChromeRegKey[] = L"Software\\Google\\Update\\Clients\\{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kChromeRegLaunchCmd[] = L"InstallerSuccessLaunchCmdLine";
const wchar_t kChromeRegLastLaunchCmd[] = L"LastInstallerSuccessLaunchCmdLine";
const wchar_t kChromeRegVersion[] = L"pv";
const wchar_t kNoChromeOfferUntil[] = L"SOFTWARE\\Google\\No Chrome Offer Until";

// Remove any registry key with non-numeric value or with the numeric value
// equal or less than today's date represented in YYYYMMDD form.
void CleanUpRegistryValues() {
  HKEY key = NULL;
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kNoChromeOfferUntil,
      0, KEY_ALL_ACCESS, &key) != ERROR_SUCCESS)
    return;

  DWORD index = 0;
  wchar_t value_name[260];
  DWORD value_name_len = _countof(value_name);
  DWORD value_type = REG_DWORD;
  DWORD value_data = 0;
  DWORD value_len = sizeof(DWORD);

  // First, remove any value whose type is not DWORD
  index = 0;
  value_len = 0;
  while (RegEnumValue(key, index, value_name, &value_name_len,
      NULL, &value_type, NULL, &value_len) == ERROR_SUCCESS) {
    if (value_type == REG_DWORD)
      index++;
    else
      RegDeleteValue(key, value_name);

    value_name_len = _countof(value_name);
    value_type = REG_DWORD;
    value_len = sizeof(DWORD);
  }

  // Get today's date, and format it as YYYYMMDD numeric value
  SYSTEMTIME now;
  GetLocalTime(&now);
  DWORD expiration_date = now.wYear * 10000 + now.wMonth * 100 + now.wDay;

  // Remove any DWORD value smaller than the number represent the
  // expiration date (YYYYMMDD)
  index = 0;
  while (RegEnumValue(key, index, value_name, &value_name_len,
      NULL, &value_type, (LPBYTE) &value_data, &value_len) == ERROR_SUCCESS) {
    if (value_type == REG_DWORD && value_data > expiration_date)
      index++;  // move on to next value
    else
      RegDeleteValue(key, value_name);  // delete this value

    value_name_len = _countof(value_name);
    value_type = REG_DWORD;
    value_data = 0;
    value_len = sizeof(DWORD);
  }

  RegCloseKey(key);
}

// Return the company name specified in the file version info resource.
bool GetCompanyName(const wchar_t* filename, wchar_t* buffer, DWORD out_len) {
  wchar_t file_version_info[8192];
  DWORD handle = 0;
  DWORD buffer_size = 0;

  buffer_size = GetFileVersionInfoSize(filename, &handle);
  // Cannot stats the file or our buffer size is too small (very unlikely)
  if (buffer_size == 0 || buffer_size > _countof(file_version_info))
    return false;

  buffer_size = _countof(file_version_info);
  memset(file_version_info, 0, buffer_size);
  if (!GetFileVersionInfo(filename, handle, buffer_size, file_version_info))
    return false;

  DWORD data_len = 0;
  LPVOID data = NULL;
  // Retrieve the language and codepage code if exists.
  buffer_size = 0;
  if (!VerQueryValue(file_version_info, TEXT("\\VarFileInfo\\Translation"),
      reinterpret_cast<LPVOID *>(&data), reinterpret_cast<UINT *>(&data_len)))
    return false;
  if (data_len != 4)
    return false;

  wchar_t info_name[256];
  DWORD lang = 0;
  // Formulate the string to retrieve the company name of the specific
  // language codepage.
  memcpy(&lang, data, 4);
  ::StringCchPrintf(info_name, _countof(info_name),
      L"\\StringFileInfo\\%02X%02X%02X%02X\\CompanyName",
      (lang & 0xff00)>>8, (lang & 0xff), (lang & 0xff000000)>>24,
      (lang & 0xff0000)>>16);

  data_len = 0;
  if (!VerQueryValue(file_version_info, info_name,
      reinterpret_cast<LPVOID *>(&data), reinterpret_cast<UINT *>(&data_len)))
    return false;
  if (data_len <= 0 || data_len >= out_len)
    return false;

  memset(buffer, 0, out_len);
  ::StringCchCopyN(buffer, out_len, (const wchar_t*)data, data_len);
  return true;
}

// Return true if we can re-offer Chrome; false, otherwise.
// Each partner can only offer Chrome once every six months.
bool CanReOfferChrome() {
  wchar_t filename[MAX_PATH+1];
  wchar_t company[MAX_PATH];

  // If we cannot retrieve the version info of the executable or company
  // name, we allow the Chrome to be offered because there is no past
  // history to be found.
  if (GetModuleFileName(NULL, filename, MAX_PATH) == 0)
    return true;
  if (!GetCompanyName(filename, company, sizeof(company)))
    return true;

  bool can_re_offer = true;
  DWORD disposition = 0;
  HKEY key = NULL;
  if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, kNoChromeOfferUntil,
      0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE,
      NULL, &key, &disposition) == ERROR_SUCCESS) {
    // Cannot re-offer, if the timer already exists and is not expired yet
    if (RegQueryValueEx(key, company, 0, 0, 0, 0) == ERROR_SUCCESS) {
      // The expired timers were already removed in CleanUpRegistryValues.
      // So if the key is not found, we can offer the Chrome.
      can_re_offer = false;
    } else {
      // Set expiration date for offer as six months from today,
      // represented as a YYYYMMDD numeric value
      SYSTEMTIME timer;
      GetLocalTime(&timer);
      timer.wMonth = timer.wMonth + 6;
      if (timer.wMonth > 12) {
        timer.wMonth = timer.wMonth - 12;
        timer.wYear = timer.wYear + 1;
      }
      DWORD value = timer.wYear * 10000 + timer.wMonth * 100 + timer.wDay;
      RegSetValueEx(key, company, 0, REG_DWORD, (LPBYTE)&value, sizeof(DWORD));
    }

    RegCloseKey(key);
  }

  return can_re_offer;
}

// Helper function to read a value from registry. Returns true if value
// is read successfully and stored in parameter value. Returns false otherwise.
bool ReadValueFromRegistry(HKEY root_key, const wchar_t *sub_key,
                           const wchar_t *value_name, wchar_t *value,
                           size_t *size) {
  HKEY key;
  if ((::RegOpenKeyEx(root_key, sub_key, NULL,
                      KEY_READ, &key) == ERROR_SUCCESS) &&
      (::RegQueryValueEx(key, value_name, NULL, NULL,
                         reinterpret_cast<LPBYTE>(value),
                         reinterpret_cast<LPDWORD>(size)) == ERROR_SUCCESS)) {
    ::RegCloseKey(key);
    return true;
  }
  return false;
}

bool IsChromeInstalled(HKEY root_key) {
  wchar_t version[64];
  size_t size = _countof(version);
  if (ReadValueFromRegistry(root_key, kChromeRegKey, kChromeRegVersion,
                            version, &size))
    return true;
  return false;
}

bool IsWinXPSp1OrLater(bool* is_vista_or_later) {
  OSVERSIONINFOEX osviex = { sizeof(OSVERSIONINFOEX) };
  int r = ::GetVersionEx((LPOSVERSIONINFO)&osviex);
  // If this failed we're on Win9X or a pre NT4SP6 OS
  if (!r)
    return false;

  if (osviex.dwMajorVersion < 5)
    return false;

  if (osviex.dwMajorVersion > 5) {
    *is_vista_or_later = true;
    return true;    // way beyond Windows XP;
  }

  if (osviex.dwMinorVersion >= 1 && osviex.wServicePackMajor >= 1)
    return true;    // Windows XP SP1 or better

  return false;     // Windows 2000, WinXP no Service Pack
}

bool VerifyAdminGroup() {
  typedef BOOL (WINAPI *ALLOCATEANDINITIALIZESIDPROC)(
      PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority, BYTE nSubAuthorityCount,
      DWORD nSubAuthority0, DWORD nSubAuthority1, DWORD nSubAuthority2,
      DWORD nSubAuthority3, DWORD nSubAuthority4, DWORD nSubAuthority5,
      DWORD nSubAuthority6, DWORD nSubAuthority7, PSID *pSid);
  typedef BOOL (WINAPI *CHECKTOKENMEMBERSHIPPROC)(HANDLE TokenHandle,
                                                  PSID SidToCheck,
                                                  PBOOL IsMember);
  typedef PVOID (WINAPI *FREESIDPROC)(PSID pSid);
  // Load our admin-checking functions dynamically.
  HMODULE advapi_library = LoadLibrary(L"advapi32.dll");
  if (advapi_library != NULL)
    return false;

  ALLOCATEANDINITIALIZESIDPROC allocSid =
      reinterpret_cast<ALLOCATEANDINITIALIZESIDPROC>(GetProcAddress(
      advapi_library, "AllocateAndInitializeSid"));
  CHECKTOKENMEMBERSHIPPROC checkToken =
      reinterpret_cast<CHECKTOKENMEMBERSHIPPROC>(GetProcAddress(
      advapi_library, "CheckTokenMembership"));
  FREESIDPROC freeSid =
      reinterpret_cast<FREESIDPROC>(GetProcAddress(
      advapi_library, "FreeSid"));
  bool result = false;
  if (allocSid != NULL && checkToken != NULL && freeSid != NULL) {
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID Group;
    BOOL check = allocSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                          DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &Group); 
    if (check) {
      if (!checkToken(NULL, Group, &check))
        check = FALSE;
      freeSid(Group); 
    }
    result = !!check;
  }
  FreeLibrary(advapi_library);
  return result;
}

bool VerifyHKLMAccess(const wchar_t* sub_key) {
  HKEY root = HKEY_LOCAL_MACHINE;
  wchar_t str[] = L"test";
  bool result = false;
  DWORD disposition = 0;
  HKEY key = NULL;

  if (RegCreateKeyEx(root, sub_key, 0, NULL,
                     REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL,
                     &key, &disposition) == ERROR_SUCCESS) {
    if (RegSetValueEx(key, str, 0, REG_SZ, (LPBYTE)str,
        (DWORD)lstrlen(str)) == ERROR_SUCCESS) {
      result = true;
      RegDeleteValue(key, str);
    }

    //  If we create the main key, delete the entire key.
    if (disposition == REG_CREATED_NEW_KEY)
      RegDeleteKey(key, NULL);

    RegCloseKey(key);
  }

  return result;
}
}  // namespace

#pragma comment(linker, "/EXPORT:GoogleChromeCompatibilityCheck=_GoogleChromeCompatibilityCheck@4,PRIVATE")
DLLEXPORT BOOL __stdcall GoogleChromeCompatibilityCheck(DWORD *reasons) {
  DWORD local_reasons = 0;

  bool is_vista_or_later = false;
  // System requirements?
  if (!IsWinXPSp1OrLater(&is_vista_or_later))
    local_reasons |= GCCC_ERROR_OSNOTSUPPORTED;

  if (IsChromeInstalled(HKEY_LOCAL_MACHINE))
    local_reasons |= GCCC_ERROR_SYSTEMLEVELALREADYPRESENT;

  if (IsChromeInstalled(HKEY_CURRENT_USER))
    local_reasons |= GCCC_ERROR_USERLEVELALREADYPRESENT;

  if (!VerifyHKLMAccess(kChromeRegKey)) {
    local_reasons |= GCCC_ERROR_ACCESSDENIED;
  } else if (is_vista_or_later && !VerifyAdminGroup()) {
    local_reasons |= GCCC_ERROR_INTEGRITYLEVEL;
  }

  // First clean up the registry keys left over previously.
  // Then only check whether we can re-offer, if everything else is OK.
  CleanUpRegistryValues();
  if (local_reasons == 0 && !CanReOfferChrome())
    local_reasons |= GCCC_ERROR_ALREADYOFFERED;

  // Done. Copy/return results.
  if (reasons != NULL)
    *reasons = local_reasons;

  return (*reasons == 0);
}

#pragma comment(linker, "/EXPORT:LaunchGoogleChrome=_LaunchGoogleChrome@4,PRIVATE")
DLLEXPORT BOOL __stdcall LaunchGoogleChrome(HANDLE *proc_handle) {
  wchar_t launch_cmd[MAX_PATH];
  size_t size = _countof(launch_cmd);
  if (!ReadValueFromRegistry(HKEY_LOCAL_MACHINE, kChromeRegKey,
                             kChromeRegLastLaunchCmd, launch_cmd, &size)) {
    size = _countof(launch_cmd);
    if (!ReadValueFromRegistry(HKEY_LOCAL_MACHINE, kChromeRegKey,
                               kChromeRegLaunchCmd, launch_cmd, &size)) {
      return false;
    }
  }

  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!CreateProcess(NULL, const_cast<wchar_t*>(launch_cmd), NULL, NULL,
                     FALSE, 0, NULL, NULL, &si, &pi))
    return false;

  // Handles must be closed or they will leak
  CloseHandle(pi.hThread);

  // If the caller wants the process handle, we won't close it.
  if (proc_handle) {
    *proc_handle = pi.hProcess;
  } else {
    CloseHandle(pi.hProcess);
  }
  return true;
}