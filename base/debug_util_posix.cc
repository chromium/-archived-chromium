// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "base/debug_util.h"

#if defined(OS_LINUX)
#include <unistd.h>
#include <execinfo.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_piece.h"

// static
bool DebugUtil::SpawnDebuggerOnProcess(unsigned /* process_id */) {
  NOTIMPLEMENTED();
  return false;
}

#if defined(OS_MACOSX)

// Based on Apple's recommended method as described in
// http://developer.apple.com/qa/qa2004/qa1361.html
// static
bool DebugUtil::BeingDebugged() {
  // Initialize mib, which tells sysctl what info we want.  In this case,
  // we're looking for information about a specific process ID.
  int mib[] = {
    CTL_KERN,
    KERN_PROC,
    KERN_PROC_PID,
    getpid()
  };

  // Caution: struct kinfo_proc is marked __APPLE_API_UNSTABLE.  The source and
  // binary interfaces may change.
  struct kinfo_proc info;
  size_t info_size = sizeof(info);

  int sysctl_result = sysctl(mib, arraysize(mib), &info, &info_size, NULL, 0);
  DCHECK(sysctl_result == 0);
  if (sysctl_result != 0)
    return false;

  // This process is being debugged if the P_TRACED flag is set.
  return (info.kp_proc.p_flag & P_TRACED) != 0;
}

#elif defined(OS_LINUX)

// We can look in /proc/self/status for TracerPid.  We are likely used in crash
// handling, so we are careful not to use the heap or have side effects.
// Another option that is common is to try to ptrace yourself, but then we
// can't detach without forking(), and that's not so great.
// static
bool DebugUtil::BeingDebugged() {
  int status_fd = open("/proc/self/status", O_RDONLY);
  if (status_fd == -1)
    return false;

  // We assume our line will be in the first 1024 characters and that we can
  // read this much all at once.  In practice this will generally be true.
  // This simplifies and speeds up things considerably.
  char buf[1024];

  ssize_t num_read = read(status_fd, buf, sizeof(buf));
  close(status_fd);

  if (num_read <= 0)
    return false;

  StringPiece status(buf, num_read);
  StringPiece tracer("TracerPid:\t");

  StringPiece::size_type pid_index = status.find(tracer);
  if (pid_index == StringPiece::npos)
    return false;

  // Our pid is 0 without a debugger, assume this for any pid starting with 0.
  pid_index += tracer.size();
  return pid_index < status.size() && status[pid_index] != '0';
}

#endif  // OS_LINUX

// static
void DebugUtil::BreakDebugger() {
  asm ("int3");
}

#if defined(OS_LINUX)

StackTrace::StackTrace() {
  static const unsigned kMaxCallers = 256;

  void* callers[kMaxCallers];
  int count = backtrace(callers, kMaxCallers);
  trace_.resize(count);
  memcpy(&trace_[0], callers, sizeof(void*) * count);
}

void StackTrace::PrintBacktrace() {
  fflush(stderr);
  backtrace_symbols_fd(&trace_[0], trace_.size(), STDERR_FILENO);
}

#elif defined(OS_MACOSX)

// TODO(port): complete this code
StackTrace::StackTrace() { }

void StackTrace::PrintBacktrace() {
  NOTIMPLEMENTED();
}

#endif  // defined(OS_MACOSX)

const void *const *StackTrace::Addresses(size_t* count) {
  *count = trace_.size();
  if (trace_.size())
    return &trace_[0];
  return NULL;
}
