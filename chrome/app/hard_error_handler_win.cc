// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/hard_error_handler_win.h"

#include <delayimp.h>
#include <ntsecapi.h>
#include <string>

#include "base/basictypes.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"

namespace {

const int32 kExceptionModuleNotFound = VcppException(ERROR_SEVERITY_ERROR,
                                                     ERROR_MOD_NOT_FOUND);
const int32 kExceptionEntryPtNotFound = VcppException(ERROR_SEVERITY_ERROR,
                                                      ERROR_PROC_NOT_FOUND);

const int32 NT_STATUS_ENTRYPOINT_NOT_FOUND = 0xC0000139;
const int32 NT_STATUS_DLL_NOT_FOUND = 0xC0000135;

bool MakeNTUnicodeString(const std::wstring& str,
                         UNICODE_STRING* nt_string) {
  if (str.empty())
    return false;
  uint32 str_size_bytes = str.size() * sizeof(wchar_t);
  nt_string->Length = str_size_bytes;
  nt_string->MaximumLength = str_size_bytes;
  nt_string->Buffer = const_cast<wchar_t*>(str.c_str());
  return true;
}

// NT-level function (not a win32 api) used to tell CSRSS of a critical error
// in the program which results in a message box dialog.
// The |exception| parameter is a standard exception code, the |param_count|
// indicates the number of items in |payload_params|. |payload_params| is
// dependent on the |exception| type but is typically an array to pointers to
// strings. |error_mode| indicates the kind of dialog buttons to show.
typedef LONG (WINAPI *NtRaiseHardErrorPF)(LONG exception,
                                          ULONG param_count,
                                          ULONG undocumented,
                                          PVOID payload_params,
                                          UINT error_mode,
                                          PULONG response);

// Helper function to call NtRaiseHardError(). It takes the exception code
// and one or two strings which are dependent on the exception code. No
// effort is done to validate that they match.
void RaiseHardErrorMsg(int32 exception, const std::wstring& text1,
                                        const std::wstring& text2) {
  // Bind the entry point. We can do it here since this function is really
  // called at most once per session. Usually never called.
  NtRaiseHardErrorPF NtRaiseHardError =
      reinterpret_cast<NtRaiseHardErrorPF>(
      ::GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtRaiseHardError"));
  if (!NtRaiseHardError)
    return;

  UNICODE_STRING uni_str1;
  UNICODE_STRING uni_str2;
  // A message needs to be displayed or else it would be confusing to the user.
  if (!MakeNTUnicodeString(text1, &uni_str1))
    return;
  int num_params = 1;
  // The second string is optional.
  if (MakeNTUnicodeString(text2, &uni_str2))
    num_params = 2;

  UNICODE_STRING* args[] = {&uni_str1, &uni_str2};
  uint32 undoc_value = 3;   // Display message to user.
  uint32 error_mode = 1;    // Display OK button only.
  ULONG response;           // What user clicked in the dialog. Discarded.
  NtRaiseHardError(exception, num_params, 3, args, error_mode, &response);
}

}  // namespace.

// Using RaiseHardErrorMsg(), it generates the same message box that is seen
// when the loader cannot find a DLL that a module depends on. |module| is the
// DLL name and it cannot be empty. The Message box only has an 'ok' button.
void ModuleNotFoundHardError(const char* module) {
  if (!module)
    return;
  std::wstring mod_name(base::SysMultiByteToWide(module, CP_ACP));
  RaiseHardErrorMsg(NT_STATUS_DLL_NOT_FOUND, mod_name, std::wstring());
}

// Using RaiseHardErrorMsg(), it generates the same message box that seen
// when the loader cannot find an import a module depends on. |module| is the
// DLL name and it cannot be empty. |entry| is the name of the method that
// could not be found. The Message box only has an 'ok' button.
void EntryPointNotFoundHardError(const char* entry, const char* module) {
  if (!module || !entry)
    return;
  std::wstring entry_point(base::SysMultiByteToWide(entry, CP_ACP));
  std::wstring mod_name(base::SysMultiByteToWide(module, CP_ACP));
  RaiseHardErrorMsg(NT_STATUS_ENTRYPOINT_NOT_FOUND, entry_point, mod_name);
}

bool DelayLoadFailureExceptionMessageBox(EXCEPTION_POINTERS* ex_info) {
  if (!ex_info)
    return false;
  DelayLoadInfo* dli = reinterpret_cast<DelayLoadInfo*>(
      ex_info->ExceptionRecord->ExceptionInformation[0]);
  if (!dli)
    return false;
  if (ex_info->ExceptionRecord->ExceptionCode == kExceptionModuleNotFound) {
    ModuleNotFoundHardError(dli->szDll);
    return true;
  }
  if (ex_info->ExceptionRecord->ExceptionCode == kExceptionEntryPtNotFound) {
    if (dli->dlp.fImportByName) {
      EntryPointNotFoundHardError(dli->dlp.szProcName, dli->szDll);
      return true;
    }
  }
  return false;
}

