// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "base/debug_util.h"

#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
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
  // If the process is sandboxed then we can't use the sysctl, so cache the
  // value.
  static bool is_set = false;
  static bool being_debugged = false;

  if (is_set) {
    return being_debugged;
  }

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
  if (sysctl_result != 0) {
    is_set = true;
    being_debugged = false;
    return being_debugged;
  }

  // This process is being debugged if the P_TRACED flag is set.
  is_set = true;
  being_debugged = (info.kp_proc.p_flag & P_TRACED) != 0;
  return being_debugged;
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

  ssize_t num_read = HANDLE_EINTR(read(status_fd, buf, sizeof(buf)));
  HANDLE_EINTR(close(status_fd));

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
#if defined(ARCH_CPU_ARM_FAMILY)
  asm("bkpt 0");
#else
  asm("int3");
#endif
}

StackTrace::StackTrace() {
  const int kMaxCallers = 256;

  void* callers[kMaxCallers];
  int count = backtrace(callers, kMaxCallers);

  // Though the backtrace API man page does not list any possible negative
  // return values, we still still exclude them because they would break the
  // memcpy code below.
  if (count > 0) {
    trace_.resize(count);
    memcpy(&trace_[0], callers, sizeof(callers[0]) * count);
  } else {
    trace_.resize(0);
  }
}

void StackTrace::PrintBacktrace() {
  fflush(stderr);
  backtrace_symbols_fd(&trace_[0], trace_.size(), STDERR_FILENO);
}

void StackTrace::OutputToStream(std::ostream* os) {
  scoped_ptr_malloc<char*> trace_symbols(
      backtrace_symbols(&trace_[0], trace_.size()));

  // If we can't retrieve the symbols, print an error and just dump the raw
  // addresses.
  if (trace_symbols.get() == NULL) {
    (*os) << "Unable get symbols for backtrace (" << strerror(errno)
          << "). Dumping raw addresses in trace:\n";
    for (size_t i = 0; i < trace_.size(); ++i) {
      (*os) << "\t" << trace_[i] << "\n";
    }
  } else {
    (*os) << "Backtrace:\n";
    for (size_t i = 0; i < trace_.size(); ++i) {
      (*os) << "\t" << trace_symbols.get()[i] << "\n";
    }
  }
}
