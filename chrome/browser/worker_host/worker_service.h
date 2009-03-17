// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_
#define CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_

#include <list>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "googleurl/src/gurl.h"

namespace IPC {
class Message;
}

class MessageLoop;
class WorkerProcessHost;
class ResourceMessageFilter;

class WorkerService {
 public:
  // Returns the WorkerService singleton.
  static WorkerService* GetInstance();

  // Creates a dedicated worker.  Returns true on success.
  bool CreateDedicatedWorker(const GURL &url,
                             ResourceMessageFilter* filter,
                             int renderer_route_id);

  // Called by ResourceMessageFilter when a message from the renderer comes that
  // should be forwarded to the worker process.
  void ForwardMessage(const IPC::Message& message);

  // Called by ResourceMessageFilter when it's destructed so that all the
  // WorkerProcessHost objects can remove their pointers to it.
  void RendererShutdown(ResourceMessageFilter* filter);

 private:
  friend struct DefaultSingletonTraits<WorkerService>;

  WorkerService();
  ~WorkerService();

  int next_worker_route_id_;

  DISALLOW_COPY_AND_ASSIGN(WorkerService);
};

#endif  // CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_
