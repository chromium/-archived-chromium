// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <windows.h>
#include <winternl.h>
#include <psapi.h>

#include "base/histogram.h"
#include "base/logging.h"
#include "base/scoped_handle_win.h"
#include "base/scoped_ptr.h"

namespace {

// System pagesize. This value remains constant on x86/64 architectures.
const int PAGESIZE_KB = 4;

// HeapSetInformation function pointer.
typedef BOOL (WINAPI* HeapSetFn)(HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T);

}  // namespace

namespace base {

int GetCurrentProcId() {
  return ::GetCurrentProcessId();
}

ProcessHandle GetCurrentProcessHandle() {
  return ::GetCurrentProcess();
}

bool CloseProcessHandle(ProcessHandle process) {
  return ::CloseHandle(process) ? true : false;
}

// Helper for GetProcId()
bool GetProcIdViaGetProcessId(ProcessHandle process, DWORD* id) {
  // Dynamically get a pointer to GetProcessId().
  typedef DWORD (WINAPI *GetProcessIdFunction)(HANDLE);
  static GetProcessIdFunction GetProcessIdPtr = NULL;
  static bool initialize_get_process_id = true;
  if (initialize_get_process_id) {
    initialize_get_process_id = false;
    HMODULE kernel32_handle = GetModuleHandle(L"kernel32.dll");
    if (!kernel32_handle) {
      NOTREACHED();
      return false;
    }
    GetProcessIdPtr = reinterpret_cast<GetProcessIdFunction>(GetProcAddress(
        kernel32_handle, "GetProcessId"));
  }
  if (!GetProcessIdPtr)
    return false;
  // Ask for the process ID.
  *id = (*GetProcessIdPtr)(process);
  return true;
}

// Helper for GetProcId()
bool GetProcIdViaNtQueryInformationProcess(ProcessHandle process, DWORD* id) {
  // Dynamically get a pointer to NtQueryInformationProcess().
  typedef NTSTATUS (WINAPI *NtQueryInformationProcessFunction)(
      HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
  static NtQueryInformationProcessFunction NtQueryInformationProcessPtr = NULL;
  static bool initialize_query_information_process = true;
  if (initialize_query_information_process) {
    initialize_query_information_process = false;
    // According to nsylvain, ntdll.dll is guaranteed to be loaded, even though
    // the Windows docs seem to imply that you should LoadLibrary() it.
    HMODULE ntdll_handle = GetModuleHandle(L"ntdll.dll");
    if (!ntdll_handle) {
      NOTREACHED();
      return false;
    }
    NtQueryInformationProcessPtr =
        reinterpret_cast<NtQueryInformationProcessFunction>(GetProcAddress(
            ntdll_handle, "NtQueryInformationProcess"));
  }
  if (!NtQueryInformationProcessPtr)
    return false;
  // Ask for the process ID.
  PROCESS_BASIC_INFORMATION info;
  ULONG bytes_returned;
  NTSTATUS status = (*NtQueryInformationProcessPtr)(process,
                                                    ProcessBasicInformation,
                                                    &info, sizeof info,
                                                    &bytes_returned);
  if (!SUCCEEDED(status) || (bytes_returned != (sizeof info)))
    return false;

  *id = static_cast<DWORD>(info.UniqueProcessId);
  return true;
}

int GetProcId(ProcessHandle process) {
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, process, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    // Try to use GetProcessId(), if it exists.  Fall back on
    // NtQueryInformationProcess() otherwise (< Win XP SP1).
    DWORD id;
    bool success =
        GetProcIdViaGetProcessId(process_with_query_rights, &id) ||
        GetProcIdViaNtQueryInformationProcess(process_with_query_rights, &id);
    CloseHandle(process_with_query_rights);
    if (success)
      return id;
  }

  // We're screwed.
  NOTREACHED();
  return 0;
}

bool LaunchApp(const std::wstring& cmdline,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = start_hidden ? SW_HIDE : SW_SHOW;
  PROCESS_INFORMATION process_info;
  if (!CreateProcess(NULL,
                     const_cast<wchar_t*>(cmdline.c_str()), NULL, NULL,
                     FALSE, 0, NULL, NULL,
                     &startup_info, &process_info))
    return false;

  // Handles must be closed or they will leak
  CloseHandle(process_info.hThread);

  if (wait)
    WaitForSingleObject(process_info.hProcess, INFINITE);

  // If the caller wants the process handle, we won't close it.
  if (process_handle) {
    *process_handle = process_info.hProcess;
  } else {
    CloseHandle(process_info.hProcess);
  }
  return true;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  return LaunchApp(cl.command_line_string(), wait,
                   start_hidden, process_handle);
}

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code.
// Returns true if this is successful, false otherwise.
bool KillProcess(int process_id, int exit_code, bool wait) {
  HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE,
                               FALSE,  // Don't inherit handle
                               process_id);
  if (process)
    return KillProcess(process, exit_code, wait);
  return false;
}

bool KillProcess(HANDLE process, int exit_code, bool wait) {
  bool result = !!TerminateProcess(process, exit_code);
  if (result && wait) {
    // The process may not end immediately due to pending I/O
    if (WAIT_OBJECT_0 != WaitForSingleObject(process, 60 * 1000))
      DLOG(ERROR) << "Error waiting for process exit: " << GetLastError();
  } else {
    DLOG(ERROR) << "Unable to terminate process: " << GetLastError();
  }
  CloseHandle(process);
  return result;
}

bool DidProcessCrash(ProcessHandle handle) {
  DWORD exitcode = 0;
  if (!::GetExitCodeProcess(handle, &exitcode)) {
    NOTREACHED();
    return false;
  }
  if (exitcode == STILL_ACTIVE) {
    // The process is likely not dead or it used 0x103 as exit code.
    NOTREACHED();
    return false;
  }

  // Warning, this is not generic code; it heavily depends on the way
  // the rest of the code kills a process.

  if (exitcode == PROCESS_END_NORMAL_TERMINATON ||
      exitcode == PROCESS_END_KILLED_BY_USER ||
      exitcode == PROCESS_END_PROCESS_WAS_HUNG ||
      exitcode == 0xC0000354 ||     // STATUS_DEBUGGER_INACTIVE.
      exitcode == 0xC000013A ||     // Control-C/end session.
      exitcode == 0x40010004) {     // Debugger terminated process/end session.
    return false;
  }

  // All other exit codes indicate crashes.

  // TODO(jar): Remove histogramming code when UMA stats are consistent with
  // other crash metrics.
  // Histogram the low order 3 nibbles for UMA
  const int kLeastValue = 0;
  const int kMaxValue = 0xFFF;
  const int kBucketCount = kMaxValue - kLeastValue + 1;
  static LinearHistogram least_significant_histogram(L"ExitCodes.LSNibbles",
      kLeastValue + 1, kMaxValue, kBucketCount);
  least_significant_histogram.SetFlags(kUmaTargetedHistogramFlag |
                                       LinearHistogram::kHexRangePrintingFlag);
  least_significant_histogram.Add(exitcode & 0xFFF);

  // Histogram the high order 3 nibbles
  static LinearHistogram most_significant_histogram(L"ExitCodes.MSNibbles",
      kLeastValue + 1, kMaxValue, kBucketCount);
  most_significant_histogram.SetFlags(kUmaTargetedHistogramFlag |
                                      LinearHistogram::kHexRangePrintingFlag);
  // Avoid passing in negative numbers by shifting data into low end of dword.
  most_significant_histogram.Add((exitcode >> 20) & 0xFFF);

  // Histogram the middle order 2 nibbles
  static LinearHistogram mid_significant_histogram(L"ExitCodes.MidNibbles",
      1, 0xFF, 0x100);
  mid_significant_histogram.SetFlags(kUmaTargetedHistogramFlag |
                                      LinearHistogram::kHexRangePrintingFlag);
  mid_significant_histogram.Add((exitcode >> 12) & 0xFF);

  return true;
}

bool WaitForExitCode(ProcessHandle handle, int* exit_code) {
  ScopedHandle closer(handle);  // Ensure that we always close the handle.
  if (::WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0) {
    NOTREACHED();
    return false;
  }
  DWORD temp_code;  // Don't clobber out-parameters in case of failure.
  if (!::GetExitCodeProcess(handle, &temp_code))
    return false;
  *exit_code = temp_code;
  return true;
}

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter) :
  started_iteration_(false),
  executable_name_(executable_name),
  filter_(filter) {
    snapshot_ = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  }

NamedProcessIterator::~NamedProcessIterator() {
  CloseHandle(snapshot_);
}


const ProcessEntry* NamedProcessIterator::NextProcessEntry() {
  bool result = false;
  do {
    result = CheckForNextProcess();
  } while (result && !IncludeEntry());

  if (result) {
    return &entry_;
  }

  return NULL;
}

bool NamedProcessIterator::CheckForNextProcess() {
  InitProcessEntry(&entry_);

  if (!started_iteration_) {
    started_iteration_ = true;
    return !!Process32First(snapshot_, &entry_);
  }

  return !!Process32Next(snapshot_, &entry_);
}

bool NamedProcessIterator::IncludeEntry() {
  return _wcsicmp(executable_name_.c_str(), entry_.szExeFile) == 0 &&
                  (!filter_ || filter_->Includes(entry_.th32ProcessID,
                                                 entry_.th32ParentProcessID));
}

void NamedProcessIterator::InitProcessEntry(ProcessEntry* entry) {
  memset(entry, 0, sizeof(*entry));
  entry->dwSize = sizeof(*entry);
}

int GetProcessCount(const std::wstring& executable_name,
                    const ProcessFilter* filter) {
  int count = 0;

  NamedProcessIterator iter(executable_name, filter);
  while (iter.NextProcessEntry())
    ++count;
  return count;
}

bool KillProcesses(const std::wstring& executable_name, int exit_code,
                   const ProcessFilter* filter) {
  bool result = true;
  const ProcessEntry* entry;

  NamedProcessIterator iter(executable_name, filter);
  while (entry = iter.NextProcessEntry())
    result = KillProcess((*entry).th32ProcessID, exit_code, true) && result;

  return result;
}

bool WaitForProcessesToExit(const std::wstring& executable_name,
                            int wait_milliseconds,
                            const ProcessFilter* filter) {
  const ProcessEntry* entry;
  bool result = true;
  DWORD start_time = GetTickCount();

  NamedProcessIterator iter(executable_name, filter);
  while (entry = iter.NextProcessEntry()) {
    DWORD remaining_wait =
      std::max(0, wait_milliseconds -
          static_cast<int>(GetTickCount() - start_time));
    HANDLE process = OpenProcess(SYNCHRONIZE,
                                 FALSE,
                                 entry->th32ProcessID);
    DWORD wait_result = WaitForSingleObject(process, remaining_wait);
    CloseHandle(process);
    result = result && (wait_result == WAIT_OBJECT_0);
  }

  return result;
}

bool WaitForSingleProcess(ProcessHandle handle, int wait_milliseconds) {
  bool retval = WaitForSingleObject(handle, wait_milliseconds) == WAIT_OBJECT_0;
  CloseHandle(handle);
  return retval;
}

bool CleanupProcesses(const std::wstring& executable_name,
                      int wait_milliseconds,
                      int exit_code,
                      const ProcessFilter* filter) {
  bool exited_cleanly = WaitForProcessesToExit(executable_name,
                                               wait_milliseconds,
                                               filter);
  if (!exited_cleanly)
    KillProcesses(executable_name, exit_code, filter);
  return exited_cleanly;
}

///////////////////////////////////////////////////////////////////////////////
// ProcesMetrics

ProcessMetrics::ProcessMetrics(ProcessHandle process) : process_(process),
                                                        last_time_(0),
                                                        last_system_time_(0) {
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  processor_count_ = system_info.dwNumberOfProcessors;
}

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

ProcessMetrics::~ProcessMetrics() { }

size_t ProcessMetrics::GetPagefileUsage() {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.PagefileUsage;
  }
  return 0;
}

// Returns the peak space allocated for the pagefile, in bytes.
size_t ProcessMetrics::GetPeakPagefileUsage() {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.PeakPagefileUsage;
  }
  return 0;
}

// Returns the current working set size, in bytes.
size_t ProcessMetrics::GetWorkingSetSize() {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.WorkingSetSize;
  }
  return 0;
}

size_t ProcessMetrics::GetPrivateBytes() {
  // PROCESS_MEMORY_COUNTERS_EX is not supported until XP SP2.
  // GetProcessMemoryInfo() will simply fail on prior OS. So the requested
  // information is simply not available. Hence, we will return 0 on unsupported
  // OSes. Unlike most Win32 API, we don't need to initialize the "cb" member.
  PROCESS_MEMORY_COUNTERS_EX pmcx;
  if (GetProcessMemoryInfo(process_,
                          reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmcx),
                          sizeof(pmcx))) {
      return pmcx.PrivateUsage;
  }
  return 0;
}

void ProcessMetrics::GetCommittedKBytes(CommittedKBytes* usage) {
  MEMORY_BASIC_INFORMATION mbi = {0};
  size_t committed_private = 0;
  size_t committed_mapped = 0;
  size_t committed_image = 0;
  void* base_address = NULL;
  while (VirtualQueryEx(process_, base_address, &mbi,
         sizeof(MEMORY_BASIC_INFORMATION)) ==
         sizeof(MEMORY_BASIC_INFORMATION)) {
      if (mbi.State == MEM_COMMIT) {
        if (mbi.Type == MEM_PRIVATE) {
          committed_private += mbi.RegionSize;
        } else if (mbi.Type == MEM_MAPPED) {
          committed_mapped += mbi.RegionSize;
        } else if (mbi.Type == MEM_IMAGE) {
          committed_image += mbi.RegionSize;
        } else {
          NOTREACHED();
        }
      }
      base_address = (static_cast<BYTE*>(mbi.BaseAddress)) + mbi.RegionSize;
  }
  usage->image = committed_image / 1024;
  usage->mapped = committed_mapped / 1024;
  usage->priv = committed_private / 1024;
}

bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) {
  size_t ws_private = 0;
  size_t ws_shareable = 0;
  size_t ws_shared = 0;

  DCHECK(ws_usage);
  memset(ws_usage, 0, sizeof(*ws_usage));

  DWORD number_of_entries = 4096;  // Just a guess.
  PSAPI_WORKING_SET_INFORMATION* buffer = NULL;
  int retries = 5;
  for (;;) {
    DWORD buffer_size = sizeof(PSAPI_WORKING_SET_INFORMATION) +
                        (number_of_entries * sizeof(PSAPI_WORKING_SET_BLOCK));

    // if we can't expand the buffer, don't leak the previous
    // contents or pass a NULL pointer to QueryWorkingSet
    PSAPI_WORKING_SET_INFORMATION* new_buffer =
        reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(
            realloc(buffer, buffer_size));
    if (!new_buffer) {
      free(buffer);
      return false;
    }
    buffer = new_buffer;

    // Call the function once to get number of items
    if (QueryWorkingSet(process_, buffer, buffer_size))
      break;  // Success

    if (GetLastError() != ERROR_BAD_LENGTH) {
      free(buffer);
      return false;
    }

    number_of_entries = static_cast<DWORD>(buffer->NumberOfEntries);

    // Maybe some entries are being added right now. Increase the buffer to
    // take that into account.
    number_of_entries = static_cast<DWORD>(number_of_entries * 1.25);

    if (--retries == 0) {
      free(buffer);  // If we're looping, eventually fail.
      return false;
    }
  }

  // On windows 2000 the function returns 1 even when the buffer is too small.
  // The number of entries that we are going to parse is the minimum between the
  // size we allocated and the real number of entries.
  number_of_entries =
      std::min(number_of_entries, static_cast<DWORD>(buffer->NumberOfEntries));
  for (unsigned int i = 0; i < number_of_entries; i++) {
    if (buffer->WorkingSetInfo[i].Shared) {
      ws_shareable++;
      if (buffer->WorkingSetInfo[i].ShareCount > 1)
        ws_shared++;
    } else {
      ws_private++;
    }
  }

  ws_usage->priv = ws_private * PAGESIZE_KB;
  ws_usage->shareable = ws_shareable * PAGESIZE_KB;
  ws_usage->shared = ws_shared * PAGESIZE_KB;
  free(buffer);
  return true;
}

static uint64 FileTimeToUTC(const FILETIME& ftime) {
  LARGE_INTEGER li;
  li.LowPart = ftime.dwLowDateTime;
  li.HighPart = ftime.dwHighDateTime;
  return li.QuadPart;
}

int ProcessMetrics::GetCPUUsage() {
  FILETIME now;
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;

  GetSystemTimeAsFileTime(&now);

  if (!GetProcessTimes(process_, &creation_time, &exit_time,
                       &kernel_time, &user_time)) {
    // We don't assert here because in some cases (such as in the Task Manager)
    // we may call this function on a process that has just exited but we have
    // not yet received the notification.
    return 0;
  }
  int64 system_time = (FileTimeToUTC(kernel_time) + FileTimeToUTC(user_time)) /
                        processor_count_;
  int64 time = FileTimeToUTC(now);

  if ((last_system_time_ == 0) || (last_time_ == 0)) {
    // First call, just set the last values.
    last_system_time_ = system_time;
    last_time_ = time;
    return 0;
  }

  int64 system_time_delta = system_time - last_system_time_;
  int64 time_delta = time - last_time_;
  DCHECK(time_delta != 0);
  if (time_delta == 0)
    return 0;

  // We add time_delta / 2 so the result is rounded.
  int cpu = static_cast<int>((system_time_delta * 100 + time_delta / 2) /
                             time_delta);

  last_system_time_ = system_time;
  last_time_ = time;

  return cpu;
}

bool ProcessMetrics::GetIOCounters(IO_COUNTERS* io_counters) {
  return GetProcessIoCounters(process_, io_counters) != FALSE;
}

bool ProcessMetrics::CalculateFreeMemory(FreeMBytes* free) {
  const SIZE_T kTopAdress = 0x7F000000;
  const SIZE_T kMegabyte = 1024 * 1024;
  SIZE_T accumulated = 0;

  MEMORY_BASIC_INFORMATION largest = {0};
  UINT_PTR scan = 0;
  while (scan < kTopAdress) {
    MEMORY_BASIC_INFORMATION info;
    if (!::VirtualQueryEx(process_, reinterpret_cast<void*>(scan),
                          &info, sizeof(info)))
      return false;
    if (info.State == MEM_FREE) {
      accumulated += info.RegionSize;
      UINT_PTR end = scan + info.RegionSize;
      if (info.RegionSize > (largest.RegionSize))
        largest = info;
    }
    scan += info.RegionSize;
  }
  free->largest = largest.RegionSize / kMegabyte;
  free->largest_ptr = largest.BaseAddress;
  free->total = accumulated / kMegabyte;
  return true;
}

bool EnableLowFragmentationHeap() {
  HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
  HeapSetFn heap_set = reinterpret_cast<HeapSetFn>(GetProcAddress(
      kernel32,
      "HeapSetInformation"));

  // On Windows 2000, the function is not exported. This is not a reason to
  // fail.
  if (!heap_set)
    return true;

  unsigned number_heaps = GetProcessHeaps(0, NULL);
  if (!number_heaps)
    return false;

  // Gives us some extra space in the array in case a thread is creating heaps
  // at the same time we're querying them.
  static const int MARGIN = 8;
  scoped_array<HANDLE> heaps(new HANDLE[number_heaps + MARGIN]);
  number_heaps = GetProcessHeaps(number_heaps + MARGIN, heaps.get());
  if (!number_heaps)
    return false;

  for (unsigned i = 0; i < number_heaps; ++i) {
    ULONG lfh_flag = 2;
    // Don't bother with the result code. It may fails on heaps that have the
    // HEAP_NO_SERIALIZE flag. This is expected and not a problem at all.
    heap_set(heaps[i],
             HeapCompatibilityInformation,
             &lfh_flag,
             sizeof(lfh_flag));
  }
  return true;
}

void EnableTerminationOnHeapCorruption() {
  // Ignore the result code. Supported on XP SP3 and Vista.
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
}

void RaiseProcessToHighPriority() {
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
}

}  // namespace base
