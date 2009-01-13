// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/time.h"

const int kMicrosecondsPerSecond = 1000000;

namespace base {

int GetCurrentProcId() {
  return getpid();
}

ProcessHandle GetCurrentProcessHandle() {
  return GetCurrentProcId();
}

int GetProcId(ProcessHandle process) {
  return process;
}

// Attempts to kill the process identified by the given process
// entry structure.  Ignores specified exit_code; posix can't force that.
// Returns true if this is successful, false otherwise.
bool KillProcess(int process_id, int exit_code, bool wait) {
  bool result = false;

  int status = kill(process_id, SIGTERM);
  if (!status && wait) {
    int tries = 60;
    // The process may not end immediately due to pending I/O
    while (tries-- > 0) {
      int pid = waitpid(process_id, &status, WNOHANG);
      if (pid == process_id) {
        result = true;
        break;
      }
      sleep(1);
    }
  }
  if (!result)
    DLOG(ERROR) << "Unable to terminate process.";
  return result;
}

int GetMaxFilesOpenInProcess() {
  struct rlimit rlimit;
  if (getrlimit(RLIMIT_NOFILE, &rlimit) != 0) {
    return 0;
  }

  // rlim_t is a uint64 - clip to maxint.
  // We do this since we use the value of this function to close FD #s in a loop
  // if we didn't clamp the value, doing this would be too time consuming.
  rlim_t max_int = static_cast<rlim_t>(std::numeric_limits<int32>::max());
  if (rlimit.rlim_cur > max_int) {
    return max_int;
  }

  return rlimit.rlim_cur;
}

ProcessMetrics::ProcessMetrics(ProcessHandle process) : process_(process),
                                                        last_time_(0),
                                                        last_system_time_(0) {
  processor_count_ = base::SysInfo::NumberOfProcessors();
}

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

ProcessMetrics::~ProcessMetrics() { }

void EnableTerminationOnHeapCorruption() {
  // On POSIX, there nothing to do AFAIK.
}

void RaiseProcessToHighPriority() {
  // On POSIX, we don't actually do anything here.  We could try to nice() or
  // setpriority() or sched_getscheduler, but these all require extra rights.
}

bool WaitForExitCode(ProcessHandle handle, int* exit_code) {
  int status;
  while (waitpid(handle, &status, 0) == -1) {
    if (errno != EINTR) {
      NOTREACHED();
      return false;
    }
  }

  if (WIFEXITED(status)) {
    *exit_code = WEXITSTATUS(status);
    return true;
  }

  // If it didn't exit cleanly, it must have been signaled.
  DCHECK(WIFSIGNALED(status));
  return false;
}

bool WaitForSingleProcess(ProcessHandle handle, int wait_milliseconds) {
  // This POSIX version of this function only guarantees that we wait no less
  // than |wait_milliseconds| for the proces to exit.  The child process may
  // exit sometime before the timeout has ended but we may still block for
  // up to 0.25 seconds after the fact.
  //
  // waitpid() has no direct support on POSIX for specifying a timeout, you can
  // either ask it to block indefinitely or return immediately (WNOHANG).
  // When a child process terminates a SIGCHLD signal is sent to the parent.
  // Catching this signal would involve installing a signal handler which may
  // affect other parts of the application and would be difficult to debug.
  //
  // Our strategy is to call waitpid() once up front to check if the process
  // has already exited, otherwise to loop for wait_milliseconds, sleeping for
  // at most 0.25 secs each time using usleep() and then calling waitpid().
  //
  // usleep() is speced to exit if a signal is received for which a handler
  // has been installed.  This means that when a SIGCHLD is sent, it will exit
  // depending on behavior external to this function.
  //
  // This function is used primarilly for unit tests, if we want to use it in
  // the application itself it would probably be best to examine other routes.
  int status = -1;
  pid_t ret_pid = waitpid(handle, &status, WNOHANG);
  static const int64 kQuarterSecondInMicroseconds = kMicrosecondsPerSecond/4;

  // If the process hasn't exited yet, then sleep and try again.
  Time wakeup_time = Time::Now() + TimeDelta::FromMilliseconds(
      wait_milliseconds);
  while (ret_pid == 0) {
    Time now = Time::Now();
    if (now > wakeup_time)
      break;
    // Guaranteed to be non-negative!
    int64 sleep_time_usecs = (wakeup_time - now).InMicroseconds();
    // Don't sleep for more than 0.25 secs at a time.
    if (sleep_time_usecs > kQuarterSecondInMicroseconds) {
      sleep_time_usecs = kQuarterSecondInMicroseconds;
    }

    // usleep() will return 0 and set errno to EINTR on receipt of a signal
    // such as SIGCHLD.
    usleep(sleep_time_usecs);
    ret_pid = waitpid(handle, &status, WNOHANG);
  }

  if (status != -1) {
    return WIFEXITED(status);
  } else {
    return false;
  }
}

namespace {

int64 TimeValToMicroseconds(const struct timeval& tv) {
  return tv.tv_sec * kMicrosecondsPerSecond + tv.tv_usec;
}

}

int ProcessMetrics::GetCPUUsage() {
  struct timeval now;
  struct rusage usage;

  int retval = gettimeofday(&now, NULL);
  if (retval)
    return 0;
  retval = getrusage(RUSAGE_SELF, &usage);
  if (retval)
    return 0;

  int64 system_time = (TimeValToMicroseconds(usage.ru_stime) +
                       TimeValToMicroseconds(usage.ru_utime)) /
                        processor_count_;
  int64 time = TimeValToMicroseconds(now);

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

}  // namespace base
