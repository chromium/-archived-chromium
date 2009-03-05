// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WORKER_THREAD_H_
#define CHROME_WORKER_WORKER_THREAD_H_

#include "base/thread.h"
#include "chrome/common/child_thread.h"

class GURL;
class WorkerWebKitClientImpl;

class WorkerThread : public ChildThread {
 public:
  WorkerThread();
  ~WorkerThread();

  // Returns the one worker thread.
  static WorkerThread* current() {
    return static_cast<WorkerThread*>(ChildThread::current());
  }

 private:
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // Called by the thread base class
  virtual void Init();
  virtual void CleanUp();

  void OnCreateWorker(const GURL& url, int route_id);

  scoped_ptr<WorkerWebKitClientImpl> webkit_client_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThread);
};

#endif  // CHROME_WORKER_WORKER_THREAD_H_
