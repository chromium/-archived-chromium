// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/worker_pool.h"

#include "base/task.h"

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
