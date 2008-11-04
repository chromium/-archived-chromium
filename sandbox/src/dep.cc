// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/dep.h"

#include "base/logging.h"
#include "base/win_util.h"

namespace sandbox {

namespace {

// These values are in the Windows 2008 SDK but not in the previous ones. Define
// the values here until we're sure everyone updated their SDK.
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE                          0x00000001
#endif
#ifndef PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION     0x00000002
#endif

// SetProcessDEPPolicy is declared in the Windows 2008 SDK.
typedef BOOL (WINAPI *FnSetProcessDEPPolicy)(DWORD dwFlags);

enum PROCESS_INFORMATION_CLASS {
  ProcessExecuteFlags = 0x22,
};

// Flags named as per their usage.
const int MEM_EXECUTE_OPTION_ENABLE = 1;
const int MEM_EXECUTE_OPTION_DISABLE = 2;
const int MEM_EXECUTE_OPTION_ATL7_THUNK_EMULATION = 4;
const int MEM_EXECUTE_OPTION_PERMANENT = 8;

// Not exactly the right signature but that will suffice.
typedef HRESULT (WINAPI *FnNtSetInformationProcess)(
    HANDLE ProcessHandle,
    PROCESS_INFORMATION_CLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength);

}  // namespace

bool SetCurrentProcessDEP(DepEnforcement enforcement) {
#ifdef _WIN64
  // DEP is always on in x64.
  return enforcement != DEP_DISABLED;
#endif
  // Only available on Windows XP SP2 and Windows Server 2003 SP1.
  // For reference: http://www.uninformed.org/?v=2&a=4
  FnNtSetInformationProcess NtSetInformationProc =
      reinterpret_cast<FnNtSetInformationProcess>(
          GetProcAddress(GetModuleHandle(L"ntdll.dll"),
                                         "NtSetInformationProcess"));

  if (!NtSetInformationProc)
    return false;

  // Flags being used as per SetProcessDEPPolicy on Vista SP1.
  ULONG dep_flags;
  switch (enforcement) {
    case DEP_DISABLED:
      // 2
      dep_flags = MEM_EXECUTE_OPTION_DISABLE;
      break;
    case DEP_ENABLED:
      // 9
      dep_flags = MEM_EXECUTE_OPTION_PERMANENT | MEM_EXECUTE_OPTION_ENABLE;
      break;
    case DEP_ENABLED_ATL7_COMPAT:
      // 0xD
      dep_flags = MEM_EXECUTE_OPTION_PERMANENT | MEM_EXECUTE_OPTION_ENABLE |
                  MEM_EXECUTE_OPTION_ATL7_THUNK_EMULATION;
      break;
    default:
      NOTREACHED();
      return false;
  }

  HRESULT status = NtSetInformationProc(GetCurrentProcess(),
                                        ProcessExecuteFlags,
                                        &dep_flags,
                                        sizeof(dep_flags));
  return SUCCEEDED(status);
}

}  // namespace sandbox

