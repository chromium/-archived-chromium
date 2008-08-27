// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"

namespace process_util {

int GetCurrentProcId() {
  return getpid();
}

int GetProcId(ProcessHandle process) {
  return ProcessHandle;
}
void RaiseProcessToHighPriority() {
  // On POSIX, we don't actually do anything here.  We could try to nice() or
  // setpriority() or sched_getscheduler, but these all require extra rights.
}

}  // namespace process_util
