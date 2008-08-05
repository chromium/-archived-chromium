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

size_t GetSystemCommitCharge() {
  // Get the System Page Size.
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  PERFORMANCE_INFORMATION info;
  if (GetPerformanceInfo(&info, sizeof(info)))
    return info.CommitTotal * system_info.dwPageSize;
  return -1;
}

void PrintChromeMemoryUsageInfo() {
  printf("\n");
  BrowserProcessFilter chrome_filter(L"");
  process_util::NamedProcessIterator
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
