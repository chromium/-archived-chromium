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

// Completely undocumented from Microsoft. You can find this information by
// disassembling Vista's SP1 kernel32.dll with your favorite disassembler.
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

  // Try documented ways first.
  // Only available on Vista SP1 and Windows 2008.
  // http://msdn.microsoft.com/en-us/library/bb736299.aspx
  FnSetProcessDEPPolicy SetProcDEP =
      reinterpret_cast<FnSetProcessDEPPolicy>(
          GetProcAddress(GetModuleHandle(L"kernel32.dll"),
                                         "SetProcessDEPPolicy"));

  if (SetProcDEP) {
    ULONG dep_flags;
    switch (enforcement) {
      case DEP_DISABLED:
        dep_flags = 0;
        break;
      case DEP_ENABLED:
        dep_flags = PROCESS_DEP_ENABLE |
                    PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION;
        break;
      case DEP_ENABLED_ATL7_COMPAT:
        dep_flags = PROCESS_DEP_ENABLE;
        break;
      default:
        NOTREACHED();
        return false;
    }
    return 0 != SetProcDEP(dep_flags);
  }

  // Go in darker areas.
  // Only available on Windows XP SP2 and Windows Server 2003 SP1.
  // http://www.uninformed.org/?v=2&a=4
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
