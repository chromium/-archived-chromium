// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/worker_pool.h"

#include "base/task.h"

// TODO(dsh): Split this file into worker_pool_win.cc and worker_pool_posix.cc.
#if defined(OS_WIN)

namespace {

DWORD CALLBACK WorkItemCallback(void* param) {
  Task* task = static_cast<Task*>(param);
  task->Run();
  delete task;
  return 0;
}

}  // namespace

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          Task* task, bool task_is_slow) {
  task->SetBirthPlace(from_here);

  ULONG flags = 0;
  if (task_is_slow)
    flags |= WT_EXECUTELONGFUNCTION;

  if (!QueueUserWorkItem(WorkItemCallback, task, flags)) {
    DLOG(ERROR) << "QueueUserWorkItem failed: " << GetLastError();
    delete task;
    return false;
  }

  return true;
}

#elif defined(OS_LINUX)

namespace {

void* PThreadCallback(void* param) {
  Task* task = static_cast<Task*>(param);
  task->Run();
  delete task;
  return 0;
}

}  // namespace

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          Task* task, bool task_is_slow) {
  task->SetBirthPlace(from_here);

  pthread_t thread;
  pthread_attr_t attr;

  // POSIX does not have a worker thread pool implementation.  For now we just
  // create a thread for each task, and ignore |task_is_slow|.
  // TODO(dsh): Implement thread reuse.
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  int err = pthread_create(&thread, &attr, PThreadCallback, task);
  pthread_attr_destroy(&attr);
  if (err) {
    DLOG(ERROR) << "pthread_create failed: " << err;
    delete task;
    return false;
  }

  return true;
}

#endif  // OS_LINUX
