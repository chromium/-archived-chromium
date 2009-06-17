// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ZYGOTE_HOST_LINUX_H_
#define CHROME_BROWSER_ZYGOTE_HOST_LINUX_H_

#include <string>
#include <vector>

#include "base/global_descriptors_posix.h"
#include "base/process.h"

template<typename Type>
struct DefaultSingletonTraits;

// http://code.google.com/p/chromium/wiki/LinuxZygote

// The zygote host is the interface, in the browser process, to the zygote
// process.
class ZygoteHost {
 public:
  ~ZygoteHost();

  pid_t ForkRenderer(const std::vector<std::string>& command_line,
                     const base::GlobalDescriptors::Mapping& mapping);
  void EnsureProcessTerminated(pid_t process);

  // Get the termination status (exit code) of the process and return true if
  // the status indicates the process crashed. |child_exited| is set to true
  // iff the child process has terminated. (|child_exited| may be NULL.)
  bool DidProcessCrash(base::ProcessHandle handle, bool* child_exited);

  // These are the command codes used on the wire between the browser and the
  // zygote.
  enum {
    kCmdFork = 0,             // Fork off a new renderer.
    kCmdReap = 1,             // Reap a renderer child.
    kCmdDidProcessCrash = 2,  // Check if child process crashed.
  };

 private:
  friend struct DefaultSingletonTraits<ZygoteHost>;
  ZygoteHost();
  void LaunchZygoteProcess();

  int control_fd_;  // the socket to the zygote
};

#endif  // CHROME_BROWSER_ZYGOTE_HOST_LINUX_H_
