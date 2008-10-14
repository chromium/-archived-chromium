// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/sys_info.h"

namespace process_util {

int GetCurrentProcId() {
  return getpid();
}

ProcessHandle GetCurrentProcessHandle() {
  return GetCurrentProcId();
}

int GetProcId(ProcessHandle process) {
  return process;
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

}  // namespace process_util
