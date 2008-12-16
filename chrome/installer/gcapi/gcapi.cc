// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi.h"

#include <atlbase.h>
#include <atlcom.h>
#include <windows.h>
#include <sddl.h>
#include <stdlib.h>
#include <strsafe.h>
#include <tlhelp32.h>

#include "google_update_idl.h"

namespace {

const wchar_t kChromeRegClientsKey[] = L"Software\\Google\\Update\\Clients\\{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kChromeRegClientStateKey[] = L"Software\\Google\\Update\\ClientState\\{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kChromeRegLaunchCmd[] = L"InstallerSuccessLaunchCmdLine";
const wchar_t kChromeRegLastLaunchCmd[] = L"LastInstallerSuccessLaunchCmdLine";
const wchar_t kChromeRegVersion[] = L"pv";
const wchar_t kNoChromeOfferUntil[] = L"SOFTWARE\\Google\\No Chrome Offer Until";

// Remove any registry key with non-numeric value or with the numeric value
// equal or less than today's date represented in YYYYMMDD form.
void CleanUpRegistryValues() {
  HKEY key = NULL;
  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, kNoChromeOfferUntil,
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
  while (::RegEnumValue(key, index, value_name, &value_name_len,
      NULL, &value_type, NULL, &value_len) == ERROR_SUCCESS) {
    if (value_type == REG_DWORD)
      index++;
    else
      ::RegDeleteValue(key, value_name);

    value_name_len = _countof(value_name);
    value_type = REG_DWORD;
    value_len = sizeof(DWORD);
  }

  // Get today's date, and format it as YYYYMMDD numeric value
  SYSTEMTIME now;
  ::GetLocalTime(&now);
  DWORD expiration_date = now.wYear * 10000 + now.wMonth * 100 + now.wDay;

  // Remove any DWORD value smaller than the number represent the
  // expiration date (YYYYMMDD)
  index = 0;
  while (::RegEnumValue(key, index, value_name, &value_name_len,
      NULL, &value_type, (LPBYTE) &value_data, &value_len) == ERROR_SUCCESS) {
    if (value_type == REG_DWORD && value_data > expiration_date)
      index++;  // move on to next value
    else
      ::RegDeleteValue(key, value_name);  // delete this value

    value_name_len = _countof(value_name);
    value_type = REG_DWORD;
    value_data = 0;
    value_len = sizeof(DWORD);
  }

  ::RegCloseKey(key);
}

// Return the company name specified in the file version info resource.
bool GetCompanyName(const wchar_t* filename, wchar_t* buffer, DWORD out_len) {
  wchar_t file_version_info[8192];
  DWORD handle = 0;
  DWORD buffer_size = 0;

  buffer_size = ::GetFileVersionInfoSize(filename, &handle);
  // Cannot stats the file or our buffer size is too small (very unlikely)
  if (buffer_size == 0 || buffer_size > _countof(file_version_info))
    return false;

  buffer_size = _countof(file_version_info);
  memset(file_version_info, 0, buffer_size);
  if (!::GetFileVersionInfo(filename, handle, buffer_size, file_version_info))
    return false;

  DWORD data_len = 0;
  LPVOID data = NULL;
  // Retrieve the language and codepage code if exists.
  buffer_size = 0;
  if (!::VerQueryValue(file_version_info, TEXT("\\VarFileInfo\\Translation"),
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
  if (!::VerQueryValue(file_version_info, info_name,
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
  if (::GetModuleFileName(NULL, filename, MAX_PATH) == 0)
    return true;
  if (!GetCompanyName(filename, company, sizeof(company)))
    return true;

  bool can_re_offer = true;
  DWORD disposition = 0;
  HKEY key = NULL;
  if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, kNoChromeOfferUntil,
      0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE,
      NULL, &key, &disposition) == ERROR_SUCCESS) {
    // Cannot re-offer, if the timer already exists and is not expired yet
    if (::RegQueryValueEx(key, company, 0, 0, 0, 0) == ERROR_SUCCESS) {
      // The expired timers were already removed in CleanUpRegistryValues.
      // So if the key is not found, we can offer the Chrome.
      can_re_offer = false;
    } else {
      // Set expiration date for offer as six months from today,
      // represented as a YYYYMMDD numeric value
      SYSTEMTIME timer;
      ::GetLocalTime(&timer);
      timer.wMonth = timer.wMonth + 6;
      if (timer.wMonth > 12) {
        timer.wMonth = timer.wMonth - 12;
        timer.wYear = timer.wYear + 1;
      }
      DWORD value = timer.wYear * 10000 + timer.wMonth * 100 + timer.wDay;
      ::RegSetValueEx(key, company, 0, REG_DWORD, (LPBYTE)&value, sizeof(DWORD));
    }

    ::RegCloseKey(key);
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
  if (ReadValueFromRegistry(root_key, kChromeRegClientsKey, kChromeRegVersion,
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

// Note this function should not be called on old Windows versions where these
// Windows API are not available. We always invoke this function after checking
// that current OS is Vista or later.
bool VerifyAdminGroup() {
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  PSID Group;
  BOOL check = ::AllocateAndInitializeSid(&NtAuthority, 2,
                                          SECURITY_BUILTIN_DOMAIN_RID,
                                          DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0,
                                          0, 0, 0,
                                          &Group); 
  if (check) {
    if (!::CheckTokenMembership(NULL, Group, &check))
      check = FALSE;
  }
  ::FreeSid(Group); 
  return (check == TRUE);
}

bool VerifyHKLMAccess(const wchar_t* sub_key) {
  HKEY root = HKEY_LOCAL_MACHINE;
  wchar_t str[] = L"test";
  bool result = false;
  DWORD disposition = 0;
  HKEY key = NULL;

  if (::RegCreateKeyEx(root, sub_key, 0, NULL,
                       REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL,
                       &key, &disposition) == ERROR_SUCCESS) {
    if (::RegSetValueEx(key, str, 0, REG_SZ, (LPBYTE)str,
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

bool IsRunningElevated() {
  // This method should be called only for Vista or later.
  bool is_vista_or_later = false;
  IsWinXPSp1OrLater(&is_vista_or_later);
  if (!is_vista_or_later || !VerifyAdminGroup())
    return false;

  HANDLE process_token;
  if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token))
    return false;

  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(process_token, TokenElevationType,
                             &elevation_type, sizeof(elevation_type),
                             &size_returned)) {
    ::CloseHandle(process_token);
    return false;
  }

  ::CloseHandle(process_token);
  return (elevation_type == TokenElevationTypeFull);
}

bool GetUserIdForProcess(size_t pid, wchar_t** user_sid) {
  HANDLE process_handle = ::OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, pid);
  if (process_handle == NULL)
    return false;

  HANDLE process_token;
  bool result = false;
  if (::OpenProcessToken(process_handle, TOKEN_QUERY, &process_token)) {
    DWORD size = 0;
    ::GetTokenInformation(process_token, TokenUser, NULL, 0, &size);
    if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER ||
        ::GetLastError() == ERROR_SUCCESS) {
      DWORD actual_size = 0;
      BYTE* token_user = new BYTE[size];
      if ((::GetTokenInformation(process_token, TokenUser, token_user, size,
                                &actual_size)) &&
          (actual_size <= size)) {
        PSID sid = reinterpret_cast<TOKEN_USER*>(token_user)->User.Sid;
        if (::ConvertSidToStringSid(sid, user_sid))
          result = true;
      }
      delete[] token_user;
    }
    ::CloseHandle(process_token);
  }
  ::CloseHandle(process_handle);
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

  if (!VerifyHKLMAccess(kChromeRegClientsKey)) {
    local_reasons |= GCCC_ERROR_ACCESSDENIED;
  } else if (is_vista_or_later && !VerifyAdminGroup()) {
    // For Vista or later check for elevation since even for admin user we could
    // be running in non-elevated mode. We require integrity level High.
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

#pragma comment(linker, "/EXPORT:LaunchGoogleChrome=_LaunchGoogleChrome@0,PRIVATE")
DLLEXPORT BOOL __stdcall LaunchGoogleChrome() {
  wchar_t launch_cmd[MAX_PATH];
  size_t size = _countof(launch_cmd);
  if (!ReadValueFromRegistry(HKEY_LOCAL_MACHINE, kChromeRegClientStateKey,
                             kChromeRegLastLaunchCmd, launch_cmd, &size)) {
    size = _countof(launch_cmd);
    if (!ReadValueFromRegistry(HKEY_LOCAL_MACHINE, kChromeRegClientStateKey,
                               kChromeRegLaunchCmd, launch_cmd, &size)) {
      return false;
    }
  }

  HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (hr != S_OK) {
    if (hr == S_FALSE)
      ::CoUninitialize();
    return false;
  }

  if (::CoInitializeSecurity(NULL, -1, NULL, NULL,
                             RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                             RPC_C_IMP_LEVEL_IDENTIFY, NULL,
                             EOAC_DYNAMIC_CLOAKING, NULL) != S_OK) {
    ::CoUninitialize();
    return false;
  }

  bool impersonation_success = false;
  if (IsRunningElevated()) {
    wchar_t* curr_proc_sid;
    if (!GetUserIdForProcess(GetCurrentProcessId(), &curr_proc_sid)) {
      ::CoUninitialize();
      return false;
    }

    DWORD pid = 0;
    ::GetWindowThreadProcessId(::GetShellWindow(), &pid);
    if (pid <= 0) {
      ::LocalFree(curr_proc_sid);
      ::CoUninitialize();
      return false;
    }

    wchar_t* exp_proc_sid;
    if (GetUserIdForProcess(pid, &exp_proc_sid)) {
      if (_wcsicmp(curr_proc_sid, exp_proc_sid) == 0) {
        HANDLE process_handle = ::OpenProcess(
            PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, TRUE, pid);
        if (process_handle != NULL) {
          HANDLE process_token;
          HANDLE user_token;
          if (::OpenProcessToken(process_handle, TOKEN_DUPLICATE | TOKEN_QUERY,
                                 &process_token) &&
              ::DuplicateTokenEx(process_token,
                                 TOKEN_IMPERSONATE | TOKEN_QUERY |
                                     TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE,
                                 NULL, SecurityImpersonation,
                                 TokenPrimary, &user_token) &&
              (::ImpersonateLoggedOnUser(user_token) != 0)) {
            impersonation_success = true;
          }
          ::CloseHandle(user_token);
          ::CloseHandle(process_token);
          ::CloseHandle(process_handle);
        }
      }
      ::LocalFree(exp_proc_sid);
    }

    ::LocalFree(curr_proc_sid);
    if (!impersonation_success) {
      ::CoUninitialize();
      return false;
    }
  }

  bool ret = false;
  CComPtr<IProcessLauncher> ipl;
  if (!FAILED(ipl.CoCreateInstance(__uuidof(ProcessLauncherClass), NULL,
                                   CLSCTX_LOCAL_SERVER))) {
    if (!FAILED(ipl->LaunchCmdLine(launch_cmd)))
      ret = true;
    ipl.Release();
  }

  if (impersonation_success)
    ::RevertToSelf();
  ::CoUninitialize();
  return ret;
}
