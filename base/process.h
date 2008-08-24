// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_H__
#define BASE_PROCESS_H__

#include "base/basictypes.h"

#ifdef OS_WIN
#include <windows.h>
#endif

// ProcessHandle is a platform specific type which represents the underlying OS
// handle to a process.
#if defined(OS_WIN)
typedef HANDLE ProcessHandle;
#elif defined(OS_POSIX)
typedef int ProcessHandle;
#endif

// A Process
// TODO(mbelshe): Replace existing code which uses the ProcessHandle w/ the
//                Process object where relevant.
class Process {
 public:
  Process() : process_(0), last_working_set_size_(0) {}
  explicit Process(ProcessHandle handle) :
    process_(handle), last_working_set_size_(0) {}

  // A handle to the current process.
  static Process Current();

  // Get/Set the handle for this process.
  ProcessHandle handle() { return process_; }
  void set_handle(ProcessHandle handle) { process_ = handle; }

  // Get the PID for this process.
  int32 pid() const;

  // Is the this process the current process.
  bool is_current() const;

  // Close the Process Handle.
  void Close() {
#ifdef OS_WIN
    CloseHandle(process_);
#endif
    process_ = 0;
  }

  // A process is backgrounded when it's priority is lower than normal.
  // Return true if this process is backgrounded, false otherwise.
  bool IsProcessBackgrounded();

  // Set a prcess as backgrounded.  If value is true, the priority
  // of the process will be lowered.  If value is false, the priority
  // of the process will be made "normal" - equivalent to default
  // process priority.
  // Returns true if the priority was changed, false otherwise.
  bool SetProcessBackgrounded(bool value);

  // Reduces the working set of memory used by the process.
  // The algorithm used by this function is intentionally vague.  Repeated calls
  // to this function consider the process' previous required Working Set sizes
  // to determine a reasonable reduction.  This helps give memory back to the OS
  // in increments without over releasing memory.
  // When the WorkingSet is reduced, it is permanent, until the caller calls
  // UnReduceWorkingSet.
  // Returns true if successful, false otherwise.
  bool ReduceWorkingSet();

  // Undoes the effects of prior calls to ReduceWorkingSet().
  // Returns true if successful, false otherwise.
  bool UnReduceWorkingSet();

  // Releases as much of the working set back to the OS as possible.
  // Returns true if successful, false otherwise.
  bool EmptyWorkingSet();

 private:
  ProcessHandle process_;
  size_t last_working_set_size_;
};

#endif  // BASE_PROCESS_H__

