// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include <vector>
#include <set>

#include "base/process_util.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/result_codes.h"

using base::Time;
using base::TimeDelta;

void TerminateAllChromeProcesses(const FilePath& data_dir) {
  // Total time the function will wait for chrome processes
  // to terminate after it told them to do so.
  const TimeDelta kExitTimeout = TimeDelta::FromMilliseconds(5000);

  ChromeProcessList process_pids(GetRunningChromeProcesses(data_dir));

  std::vector<base::ProcessHandle> handles;
  {
    ChromeProcessList::const_iterator it;
    for (it = process_pids.begin(); it != process_pids.end(); ++it) {
      base::ProcessHandle handle;
      // Ignore processes for which we can't open the handle. We don't guarantee
      // that all processes will terminate, only try to do so.
      if (base::OpenPrivilegedProcessHandle(*it, &handle))
        handles.push_back(handle);
    }
  }

  std::vector<base::ProcessHandle>::const_iterator it;
  for (it = handles.begin(); it != handles.end(); ++it)
    base::KillProcess(*it, ResultCodes::TASKMAN_KILL, false);

  const Time start = Time::Now();
  for (it = handles.begin();
       it != handles.end() && Time::Now() - start < kExitTimeout;
       ++it) {
    int64 wait_time_ms = (Time::Now() - start).InMilliseconds();
    base::WaitForSingleProcess(*it, wait_time_ms);
  }

  for (it = handles.begin(); it != handles.end(); ++it)
    base::CloseProcessHandle(*it);
}

class ChildProcessFilter : public base::ProcessFilter {
 public:
  explicit ChildProcessFilter(base::ProcessId parent_pid)
      : parent_pids_(&parent_pid, (&parent_pid) + 1) {}

  explicit ChildProcessFilter(std::vector<base::ProcessId> parent_pids)
      : parent_pids_(parent_pids.begin(), parent_pids.end()) {}

  virtual bool Includes(base::ProcessId pid, base::ProcessId parent_pid) const {
    return parent_pids_.find(parent_pid) != parent_pids_.end();
  }

 private:
  const std::set<base::ProcessId> parent_pids_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessFilter);
};

ChromeProcessList GetRunningChromeProcesses(const FilePath& data_dir) {
  ChromeProcessList result;

  base::ProcessId browser_pid = ChromeBrowserProcessId(data_dir);
  if (browser_pid == (base::ProcessId) -1)
    return result;

  ChildProcessFilter filter(browser_pid);
  base::NamedProcessIterator it(chrome::kBrowserProcessExecutableName, &filter);

  const ProcessEntry* process_entry;
  while ((process_entry = it.NextProcessEntry())) {
#if defined(OS_WIN)
    result.push_back(process_entry->th32ProcessID);
#elif defined(OS_POSIX)
    result.push_back(process_entry->pid);
#endif
  }

#if defined(OS_LINUX)
  // On Linux we might be running with a zygote process for the renderers.
  // Because of that we sweep the list of processes again and pick those which
  // are children of one of the processes that we've already seen.
  {
    ChildProcessFilter filter(result);
    base::NamedProcessIterator it(chrome::kBrowserProcessExecutableName,
                                  &filter);
    while ((process_entry = it.NextProcessEntry()))
      result.push_back(process_entry->pid);
  }
#endif

  result.push_back(browser_pid);

  return result;
}
