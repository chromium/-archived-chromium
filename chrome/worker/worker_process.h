// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WORKER_PROCESS_H_
#define CHROME_WORKER_WORKER_PROCESS_H_

#include "chrome/common/child_process.h"

// Represents the worker end of the renderer<->worker connection. The
// opposite end is the WorkerProcessHost. This is a singleton object for
// each worker process.
class WorkerProcess : public ChildProcess {
 public:
  WorkerProcess();
  virtual ~WorkerProcess();

  // Returns a pointer to the PluginProcess singleton instance.
  static WorkerProcess* current() {
    return static_cast<WorkerProcess*>(ChildProcess::current());
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(WorkerProcess);
};

#endif  // CHROME_WORKER_WORKER_PROCESS_H_
