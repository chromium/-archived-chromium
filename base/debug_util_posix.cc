// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug_util.h"

#include <unistd.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "base/notimplemented.h"
#include "base/string_piece.h"

bool DebugUtil::SpawnDebuggerOnProcess(unsigned /* process_id */) {
  NOTIMPLEMENTED();
  return false;
}

#if defined(OS_MACOSX)
// http://developer.apple.com/qa/qa2004/qa1361.html
bool DebugUtil::BeingDebugged() {
  NOTIMPLEMENTED();
  return false;
}

#elif defined(OS_LINUX)
// We can look in /proc/self/status for TracerPid.  We are likely used in crash
// handling, so we are careful not to use the heap or have side effects.
// Another option that is common is to try to ptrace yourself, but then we
// can't detach without forking(), and that's not so great.
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
#endif

// static
void DebugUtil::BreakDebugger() {
  asm ("int3");
}

