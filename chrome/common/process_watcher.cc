// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/sys_info.h"
#include "base/timer.h"
#include "base/worker_pool.h"
#include "chrome/app/result_codes.h"
#include "chrome/common/env_vars.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

namespace {

class TerminatorTask : public Task {
 public:
  explicit TerminatorTask(base::ProcessHandle process) : process_(process) {
    timer_.Start(base::TimeDelta::FromMilliseconds(kWaitInterval),
                 this, &TerminatorTask::KillProcess);
  }

  virtual ~TerminatorTask() {
    if (process_) {
      KillProcess();
      DCHECK(!process_);
    }
  }

  virtual void Run() {
    base::WaitForSingleProcess(process_, kWaitInterval);
    timer_.Stop();
    if (process_)
      KillProcess();
  }

 private:
  void KillProcess() {
    if (base::SysInfo::HasEnvVar(env_vars::kHeadless)) {
      // If running the distributed tests, give the renderer a little time
      // to figure out that the channel is shutdown and unwind.
      if (base::WaitForSingleProcess(process_, kWaitInterval)) {
        Cleanup();
        return;
      }
    }

    // OK, time to get frisky.  We don't actually care when the process
    // terminates.  We just care that it eventually terminates.
    base::KillProcess(base::Process(process_).pid(),
                      ResultCodes::HUNG,
                      false /* don't wait */);

    Cleanup();
  }

  void Cleanup() {
    timer_.Stop();
    base::CloseProcessHandle(process_);
    process_ = NULL;
  }

  // The process that we are watching.
  base::ProcessHandle process_;

  base::OneShotTimer<TerminatorTask> timer_;

  DISALLOW_COPY_AND_ASSIGN(TerminatorTask);
};

}  // namespace

// static
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process) {
  DCHECK(base::GetProcId(process) != base::GetCurrentProcId());

  // Check if the process has already exited.
  if (base::WaitForSingleProcess(process, 0)) {
    base::CloseProcessHandle(process);
    return;
  }

  WorkerPool::PostTask(FROM_HERE, new TerminatorTask(process), true);
}
