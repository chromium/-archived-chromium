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
