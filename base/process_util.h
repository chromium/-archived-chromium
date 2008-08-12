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

// This file/namespace contains utility functions for enumerating, ending and
// computing statistics of processes.

#ifndef BASE_PROCESS_UTIL_H__
#define BASE_PROCESS_UTIL_H__

#include "base/basictypes.h"

#ifdef OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

#include <string>

#include "base/process.h"

// ProcessHandle is a platform specific type which represents the underlying OS
// handle to a process.
#if defined(OS_WIN)
typedef PROCESSENTRY32 ProcessEntry;
typedef IO_COUNTERS IoCounters;
#elif defined(OS_POSIX)
typedef int ProcessEntry;
typedef int IoCounters;  //TODO(awalker): replace with struct when available
#endif

namespace process_util {

// Returns the id of the current process.
int GetCurrentProcId();

// Returns the unique ID for the specified process.  This is functionally the
// same as Windows' GetProcessId(), but works on versions of Windows before
// Win XP SP1 as well.
int GetProcId(ProcessHandle process);

// Runs the given application name with the given command line. Normally, the
// first command line argument should be the path to the process, and don't
// forget to quote it.
//
// If wait is true, it will block and wait for the other process to finish,
// otherwise, it will just continue asynchronously.
//
// Example (including literal quotes)
//  cmdline = "c:\windows\explorer.exe" -foo "c:\bar\"
//
// If process_handle is non-NULL, the process handle of the launched app will be
// stored there on a successful launch.
// NOTE: In this case, the caller is responsible for closing the handle so
//       that it doesn't leak!
bool LaunchApp(const std::wstring& cmdline,
               bool wait, bool start_hidden, ProcessHandle* process_handle);

// Used to filter processes by process ID.
class ProcessFilter {
 public:
  // Returns true to indicate set-inclusion and false otherwise.  This method
  // should not have side-effects and should be idempotent.
  virtual bool Includes(uint32 pid, uint32 parent_pid) const = 0;
  virtual ~ProcessFilter() {}
};

// Returns the number of processes on the machine that are running from the
// given executable name.  If filter is non-null, then only processes selected
// by the filter will be counted.
int GetProcessCount(const std::wstring& executable_name,
                    const ProcessFilter* filter);

// Attempts to kill all the processes on the current machine that were launched
// from the given executable name, ending them with the given exit code.  If
// filter is non-null, then only processes selected by the filter are killed.
// Returns false if all processes were able to be killed off, false if at least
// one couldn't be killed.
bool KillProcesses(const std::wstring& executable_name, int exit_code,
                   const ProcessFilter* filter);

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code. If |wait| is true, wait
// for the process to be actually terminated before returning.
// Returns true if this is successful, false otherwise.
bool KillProcess(int process_id, int exit_code, bool wait);

// Get the termination status (exit code) of the process and return true if the
// status indicates the process crashed.  It is an error to call this if the
// process hasn't terminated yet.
bool DidProcessCrash(ProcessHandle handle);

// Wait for all the processes based on the named executable to exit.  If filter
// is non-null, then only processes selected by the filter are waited on.
// Returns after all processes have exited or wait_milliseconds have expired.
// Returns true if all the processes exited, false otherwise.
bool WaitForProcessesToExit(const std::wstring& executable_name,
                            int wait_milliseconds,
                            const ProcessFilter* filter);

// Waits a certain amount of time (can be 0) for all the processes with a given
// executable name to exit, then kills off any of them that are still around.
// If filter is non-null, then only processes selected by the filter are waited
// on.  Killed processes are ended with the given exit code.  Returns false if
// any processes needed to be killed, true if they all exited cleanly within
// the wait_milliseconds delay.
bool CleanupProcesses(const std::wstring& executable_name,
                      int wait_milliseconds,
                      int exit_code,
                      const ProcessFilter* filter);

// This class provides a way to iterate through the list of processes
// on the current machine that were started from the given executable
// name.  To use, create an instance and then call NextProcessEntry()
// until it returns false.
class NamedProcessIterator {
 public:
  NamedProcessIterator(const std::wstring& executable_name,
                       const ProcessFilter* filter);
  ~NamedProcessIterator();

  // If there's another process that matches the given executable name,
  // returns a const pointer to the corresponding PROCESSENTRY32.
  // If there are no more matching processes, returns NULL.
  // The returned pointer will remain valid until NextProcessEntry()
  // is called again or this NamedProcessIterator goes out of scope.
  const ProcessEntry* NextProcessEntry();

 private:
  // Determines whether there's another process (regardless of executable)
  // left in the list of all processes.  Returns true and sets entry_ to
  // that process's info if there is one, false otherwise.
  bool CheckForNextProcess();

  bool IncludeEntry();

  // Initializes a PROCESSENTRY32 data structure so that it's ready for
  // use with Process32First/Process32Next.
  void InitProcessEntry(ProcessEntry* entry);

  std::wstring executable_name_;
#ifdef OS_WIN
  HANDLE snapshot_;
#endif
  bool started_iteration_;
  ProcessEntry entry_;
  const ProcessFilter* filter_;

  DISALLOW_EVIL_CONSTRUCTORS(NamedProcessIterator);
};

// Working Set (resident) memory usage broken down by
// priv (private): These pages (kbytes) cannot be shared with any other process.
// shareable:      These pages (kbytes) can be shared with other processes under
//                 the right circumstances.
// shared :        These pages (kbytes) are currently shared with at least one
//                 other process.
struct WorkingSetKBytes {
  size_t priv;
  size_t shareable;
  size_t shared;
};

// Committed (resident + paged) memory usage broken down by
// private: These pages cannot be shared with any other process.
// mapped:  These pages are mapped into the view of a section (backed by
//          pagefile.sys)
// image:   These pages are mapped into the view of an image section (backed by
//          file system)
struct CommittedKBytes {
  size_t priv;
  size_t mapped;
  size_t image;
};

// Free memory (Megabytes marked as free) in the 2G process address space.
// total : total amount in megabytes marked as free. Maximum value is 2048.
// largest : size of the largest contiguous amount of memory found. It is
//   always smaller or equal to FreeMBytes::total.
// largest_ptr: starting address of the largest memory block.
struct FreeMBytes {
  size_t total;
  size_t largest;
  void* largest_ptr;
};

// Provides performance metrics for a specified process (CPU usage, memory and
// IO counters). To use it, invoke CreateProcessMetrics() to get an instance
// for a specific process, then access the information with the different get
// methods.
class ProcessMetrics {
 public:
  // Creates a ProcessMetrics for the specified process.
  // The caller owns the returned object.
  static ProcessMetrics* CreateProcessMetrics(ProcessHandle process);

  ~ProcessMetrics();

  // Returns the current space allocated for the pagefile, in bytes (these pages
  // may or may not be in memory).
  size_t GetPagefileUsage();
  // Returns the peak space allocated for the pagefile, in bytes.
  size_t GetPeakPagefileUsage();
  // Returns the current working set size, in bytes.
  size_t GetWorkingSetSize();
  // Returns private usage, in bytes. Private bytes is the amount
  // of memory currently allocated to a process that cannot be shared.
  // Note: returns 0 on unsupported OSes: prior to XP SP2.
  size_t GetPrivateBytes();
  // Fills a CommittedKBytes with both resident and paged
  // memory usage as per definition of CommittedBytes.
  void GetCommittedKBytes(CommittedKBytes* usage);
  // Fills a WorkingSetKBytes containing resident private and shared memory
  // usage in bytes, as per definition of WorkingSetBytes.
  bool GetWorkingSetKBytes(WorkingSetKBytes* ws_usage);

  // Computes the current process available memory for allocation.
  // It does a linear scan of the address space querying each memory region
  // for its free (unallocated) status. It is useful for estimating the memory
  // load and fragmentation.
  bool CalculateFreeMemory(FreeMBytes* free);

  // Returns the CPU usage in percent since the last time this method was
  // called. The first time this method is called it returns 0 and will return
  // the actual CPU info on subsequent calls.
  // Note that on multi-processor machines, the CPU usage value is for all
  // CPUs. So if you have 2 CPUs and your process is using all the cycles
  // of 1 CPU and not the other CPU, this method returns 50.
  int GetCPUUsage();

  // Retrieves accounting information for all I/O operations performed by the
  // process.
  // If IO information is retrieved successfully, the function returns true
  // and fills in the IO_COUNTERS passed in. The function returns false
  // otherwise.
  bool GetIOCounters(IoCounters* io_counters);

 private:
  explicit ProcessMetrics(ProcessHandle process);

  ProcessHandle process_;

  int processor_count_;

  // Used to store the previous times so we can compute the CPU usage.
  int64 last_time_;
  int64 last_system_time_;

  DISALLOW_EVIL_CONSTRUCTORS(ProcessMetrics);
};

// Enables low fragmentation heap (LFH) for every heaps of this process. This
// won't have any effect on heaps created after this function call. It will not
// modify data allocated in the heaps before calling this function. So it is
// better to call this function early in initialization and again before
// entering the main loop.
// Note: Returns true on Windows 2000 without doing anything.
bool EnableLowFragmentationHeap();

}  // namespace process_util


#endif  // BASE_PROCESS_UTIL_H__
