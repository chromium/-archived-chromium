// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <psapi.h>

#include "base/basictypes.h"
#include "base/process_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_process_filter.h"
#include "chrome/test/perf/mem_usage.h"

bool GetMemoryInfo(uint32 process_id,
                   size_t *peak_virtual_size,
                   size_t *current_virtual_size,
                   size_t *peak_working_set_size,
                   size_t *current_working_set_size) {
  if (!peak_virtual_size || !current_virtual_size)
    return false;

  HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION |
                                      PROCESS_VM_READ,
                                      FALSE, process_id);
  if (!process_handle)
    return false;

  PROCESS_MEMORY_COUNTERS_EX pmc;
  bool result = false;
  if (GetProcessMemoryInfo(process_handle,
                           reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pmc),
                           sizeof(pmc))) {
    *peak_virtual_size = pmc.PeakPagefileUsage;
    *current_virtual_size = pmc.PagefileUsage;
    *peak_working_set_size = pmc.PeakWorkingSetSize;
    *current_working_set_size = pmc.WorkingSetSize;
    result = true;
  }

  CloseHandle(process_handle);
  return result;
}

// GetPerformanceInfo is not available on WIN2K.  So we'll
// load it on-the-fly.
const wchar_t kPsapiDllName[] = L"psapi.dll";
typedef BOOL (WINAPI *GetPerformanceInfoFunction) (
    PPERFORMANCE_INFORMATION pPerformanceInformation,
    DWORD cb);

static BOOL InternalGetPerformanceInfo(
    PPERFORMANCE_INFORMATION pPerformanceInformation, DWORD cb) {
  static GetPerformanceInfoFunction GetPerformanceInfo_func = NULL;
  if (!GetPerformanceInfo_func) {
    HMODULE psapi_dll = ::GetModuleHandle(kPsapiDllName);
    if (psapi_dll)
      GetPerformanceInfo_func = reinterpret_cast<GetPerformanceInfoFunction>(
          GetProcAddress(psapi_dll, "GetPerformanceInfo"));

    if (!GetPerformanceInfo_func) {
      // The function could be loaded!
      memset(pPerformanceInformation, 0, cb);
      return FALSE;
    }
  }
  return GetPerformanceInfo_func(pPerformanceInformation, cb);
}


size_t GetSystemCommitCharge() {
  // Get the System Page Size.
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  PERFORMANCE_INFORMATION info;
  if (InternalGetPerformanceInfo(&info, sizeof(info)))
    return info.CommitTotal * system_info.dwPageSize;
  return -1;
}

void PrintChromeMemoryUsageInfo() {
  printf("\n");
  BrowserProcessFilter chrome_filter(L"");
  base::NamedProcessIterator
      chrome_process_itr(chrome::kBrowserProcessExecutableName, &chrome_filter);

  const PROCESSENTRY32* chrome_entry;
  while (chrome_entry = chrome_process_itr.NextProcessEntry()) {
    uint32 pid = chrome_entry->th32ProcessID;
    size_t peak_virtual_size;
    size_t current_virtual_size;
    size_t peak_working_set_size;
    size_t current_working_set_size;
    if (GetMemoryInfo(pid, &peak_virtual_size, &current_virtual_size,
                      &peak_working_set_size, &current_working_set_size)) {
      if (pid == chrome_filter.browser_process_id()) {
        wprintf(L"browser_vm_peak = %d\n", peak_virtual_size);
        wprintf(L"browser_vm_current = %d\n", current_virtual_size);
        wprintf(L"browser_ws_peak = %d\n", peak_working_set_size);
        wprintf(L"browser_ws_final = %d\n", current_working_set_size);
      } else {
        wprintf(L"render_vm_peak = %d\n", peak_virtual_size);
        wprintf(L"render_vm_current = %d\n", current_virtual_size);
        wprintf(L"render_ws_peak = %d\n", peak_working_set_size);
        wprintf(L"render_ws_final = %d\n", current_working_set_size);
      }
    }
  };
}
