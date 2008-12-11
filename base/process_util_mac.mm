// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/logging.h"
#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <spawn.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>

extern "C" {
extern char** environ;
}

namespace base {

bool LaunchApp(const std::vector<std::string>& argv,
               bool wait, ProcessHandle* process_handle) {
  bool retval = true;
  
  char* argv_copy[argv.size() + 1];
  for (size_t i = 0; i < argv.size(); i++) {
    argv_copy[i] = const_cast<char*>(argv[i].c_str());
  }
  argv_copy[argv.size()] = NULL;
  
  int pid = 0;
  int spawn_succeeded = (posix_spawnp(&pid, 
                                      argv_copy[0], 
                                      NULL, 
                                      NULL, 
                                      argv_copy, 
                                      environ) == 0);
  
  bool process_handle_valid = pid > 0;  
  if (!spawn_succeeded || !process_handle_valid) {
    retval = false;
  } else {
    if (wait)
      waitpid(pid, 0, 0);
    
    if(process_handle)
      *process_handle = pid;
  }
  
  return retval;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  // TODO(playmobil): Do we need to respect the start_hidden flag?
  return LaunchApp(cl.argv(), wait, process_handle);
}

bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) {
  // TODO(pinkerton): can we implement this? On linux it relies on /proc.
  return false;
}

int GetProcessCount(const std::wstring& executable_name,
                    const ProcessFilter* filter) {
  NOTIMPLEMENTED();
  return 0;
}

bool CleanupProcesses(const std::wstring& executable_name,
                      int wait_milliseconds,
                      int exit_code,
                      const ProcessFilter* filter) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
