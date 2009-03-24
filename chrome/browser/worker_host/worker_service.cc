// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_service.h"

#include "base/singleton.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/worker_host/worker_process_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"

WorkerService* WorkerService::GetInstance() {
  return Singleton<WorkerService>::get();
}

WorkerService::WorkerService() : next_worker_route_id_(0) {
}

WorkerService::~WorkerService() {
}

bool WorkerService::CreateDedicatedWorker(const GURL &url,
                                          int render_view_route_id,
                                          ResourceMessageFilter* filter,
                                          int renderer_route_id) {
  WorkerProcessHost* worker = NULL;
  // One worker process for quick bringup!
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    worker = static_cast<WorkerProcessHost*>(*iter);
    break;
  }

  if (!worker) {
    worker = new WorkerProcessHost(filter->resource_dispatcher_host());
    if (!worker->Init()) {
      delete worker;
      return false;
    }
  }

  // Generate a unique route id for the browser-worker communication that's
  // unique among all worker processes.  That way when the worker process sends
  // a wrapped IPC message through us, we know which WorkerProcessHost to give
  // it to.
  worker->CreateWorker(url, render_view_route_id, ++next_worker_route_id_,
                       renderer_route_id, filter);

  return true;
}

void WorkerService::ForwardMessage(const IPC::Message& message) {
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    if (worker->FilterMessage(message))
      return;
  }

  // TODO(jabdelmalek): tell sender that callee is gone
}

void WorkerService::RendererShutdown(ResourceMessageFilter* filter) {
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    worker->RendererShutdown(filter);
  }
}
