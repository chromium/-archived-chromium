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
