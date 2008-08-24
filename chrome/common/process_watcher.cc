// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#include "base/message_loop.h"
#include "chrome/app/result_codes.h"
#include "chrome/common/env_util.h"
#include "chrome/common/env_vars.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

namespace {

class TimerExpiredTask : public Task, public MessageLoop::Watcher {
 public:
  explicit TimerExpiredTask(ProcessHandle process) : process_(process) {
    MessageLoop::current()->WatchObject(process_, this);
  }

  virtual ~TimerExpiredTask() {
    if (process_) {
      KillProcess();
      DCHECK(!process_) << "Make sure to close the handle.";
    }
  }

  // Task ---------------------------------------------------------------------

  virtual void Run() {
    if (process_)
      KillProcess();
  }

  // MessageLoop::Watcher -----------------------------------------------------

  virtual void OnObjectSignaled(HANDLE object) {
    if (MessageLoop::current()) {
      // When we're called from our destructor, the message loop is in the
      // process of being torn down.  Only touch the message loop if it is
      // still running.
      MessageLoop::current()->WatchObject(process_, NULL);  // Stop watching.
    }

    CloseHandle(process_);
    process_ = NULL;
  }

 private:
  void KillProcess() {
    if (env_util::HasEnvironmentVariable(env_vars::kHeadless)) {
     // If running the distributed tests, give the renderer a little time to figure out
     // that the channel is shutdown and unwind.
     if (WaitForSingleObject(process_, kWaitInterval) == WAIT_OBJECT_0) {
       OnObjectSignaled(process_);
       return;
     }
    }

    // OK, time to get frisky.  We don't actually care when the process
    // terminates.  We just care that it eventually terminates, and that's what
    // TerminateProcess should do for us. Don't check for the result code since
    // it fails quite often. This should be investigated eventually.
    TerminateProcess(process_, ResultCodes::HUNG);

    // Now, just cleanup as if the process exited normally.
    OnObjectSignaled(process_);
  }

  // The process that we are watching.
  ProcessHandle process_;

  DISALLOW_EVIL_CONSTRUCTORS(TimerExpiredTask);
};

}  // namespace

// static
void ProcessWatcher::EnsureProcessTerminated(ProcessHandle process) {
  DCHECK(process != GetCurrentProcess());

  // If already signaled, then we are done!
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    CloseHandle(process);
    return;
  }

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new TimerExpiredTask(process),
                                          kWaitInterval);
}

