// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <spawn.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"

namespace base {

bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               bool wait, ProcessHandle* process_handle) {
  bool retval = true;

  char* argv_copy[argv.size() + 1];
  for (size_t i = 0; i < argv.size(); i++) {
    argv_copy[i] = const_cast<char*>(argv[i].c_str());
  }
  argv_copy[argv.size()] = NULL;

  // Make sure we don't leak any FDs to the child process by marking all FDs
  // as close-on-exec.
  SetAllFDsToCloseOnExec();

  posix_spawn_file_actions_t file_actions;
  if (posix_spawn_file_actions_init(&file_actions) != 0) {
    return false;
  }

  // Turn fds_to_remap array into a set of dup2 calls.
  for (file_handle_mapping_vector::const_iterator it = fds_to_remap.begin();
       it != fds_to_remap.end();
       ++it) {
    int src_fd = it->first;
    int dest_fd = it->second;

    if (src_fd == dest_fd) {
      int flags = fcntl(src_fd, F_GETFD);
      if (flags != -1) {
        fcntl(src_fd, F_SETFD, flags & ~FD_CLOEXEC);
      }
    } else {
      if (posix_spawn_file_actions_adddup2(&file_actions, src_fd, dest_fd) != 0)
          {
        posix_spawn_file_actions_destroy(&file_actions);
        return false;
      }
    }
  }

  int pid = 0;
  int spawn_succeeded = (posix_spawnp(&pid,
                                      argv_copy[0],
                                      &file_actions,
                                      NULL,
                                      argv_copy,
                                      *_NSGetEnviron()) == 0);

  posix_spawn_file_actions_destroy(&file_actions);

  bool process_handle_valid = pid > 0;
  if (!spawn_succeeded || !process_handle_valid) {
    retval = false;
  } else {
    if (wait)
      waitpid(pid, 0, 0);

    if (process_handle)
      *process_handle = pid;
  }

  return retval;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  // TODO(playmobil): Do we need to respect the start_hidden flag?
  file_handle_mapping_vector no_files;
  return LaunchApp(cl.argv(), no_files, wait, process_handle);
}

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter) 
  : executable_name_(executable_name),
    index_of_kinfo_proc_(0),
    filter_(filter) {
}

NamedProcessIterator::~NamedProcessIterator() {
}

const ProcessEntry* NamedProcessIterator::NextProcessEntry() {
  // Every call, you have to get new kinfo_procs_.
  // Because the process status might be changed.
  int num_of_kinfo_proc = 0;
  index_of_kinfo_proc_ = 0;
  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
  size_t len = 0;

  if (sysctl(mib, arraysize(mib), NULL, &len, NULL, 0) < 0)
    return NULL;

  num_of_kinfo_proc = len / sizeof(struct kinfo_proc);
  // Leave some spare room for process table growth.
  num_of_kinfo_proc += 16; 
  kinfo_procs_.resize(num_of_kinfo_proc);
  len = num_of_kinfo_proc * sizeof(struct kinfo_proc);
  if (sysctl(mib, arraysize(mib), &kinfo_procs_[0], &len, NULL, 0) < 0)
    return NULL;

  num_of_kinfo_proc = len / sizeof(struct kinfo_proc);
  kinfo_procs_.resize(num_of_kinfo_proc);

  bool result = false;
  do {
    result = CheckForNextProcess();
  } while (result && !IncludeEntry());

  if (result)
    return &entry_;

  return NULL;
}

bool NamedProcessIterator::CheckForNextProcess() {
  std::string exec_name;
  kinfo_proc* kinfo = NULL;
  for (; index_of_kinfo_proc_ < kinfo_procs_.size(); ++index_of_kinfo_proc_) {
    if (kinfo_procs_[index_of_kinfo_proc_].kp_proc.p_stat != SZOMB) {
      kinfo = &kinfo_procs_[index_of_kinfo_proc_];

      int mib[] = { KERN_PROCARGS, KERN_PROCARGS, kinfo->kp_proc.p_pid };
      
      size_t data_len = 0;
      if (sysctl(mib, arraysize(mib), NULL, &data_len, NULL, 0) < 0)
	continue;

      std::string data;
      data.resize(data_len);
      if (sysctl(mib, arraysize(mib), &data[0], &data_len, NULL, 0) < 0)
	continue;
	
      // "data" has absolute process path with '/',
      // so we get the last part as execution process name.
      
      int exec_name_end = data.find('\0');
      int last_slash = data.rfind('/', exec_name_end);

      // If the index is not -1, it means valid exec name is found.
      // Get the exec name and store the name into exec_name and break.
      // "last_slash" point is '/', so get substr from the next.
      if (last_slash != -1) {  
	exec_name = data.substr(exec_name_end - last_slash - 1);
      } else {
	exec_name = data.substr(0, exec_name_end);
      }
      break;
    }    
  }

  if (index_of_kinfo_proc_ >= kinfo_procs_.size())
    return false;
  
  entry_.pid = kinfo->kp_proc.p_pid;
  entry_.ppid = kinfo->kp_proc.p_oppid;

  base::strlcpy(entry_.szExeFile, exec_name.c_str(), sizeof(entry_.szExeFile));

  return true;
}

bool NamedProcessIterator::IncludeEntry() {
  if (WideToUTF8(executable_name_) != entry_.szExeFile)
    return false;
  if (!filter_)
    return true;
  return filter_->Includes(entry_.pid, entry_.ppid);
}

bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) {
  // TODO(pinkerton): can we implement this? On linux it relies on /proc.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
