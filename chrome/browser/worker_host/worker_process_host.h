// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
#define CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_

#include <list>

#include "base/basictypes.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/ipc_channel.h"
#include "googleurl/src/gurl.h"

class WorkerProcessHost : public ChildProcessHost {
 public:
  // Contains information about each worker instance, needed to forward messages
  // between the renderer and worker processes.
  struct WorkerInstance {
    GURL url;
    int renderer_process_id;
    int render_view_route_id;
    int worker_route_id;
    IPC::Message::Sender* sender;
    int sender_pid;
    int sender_route_id;
  };

  WorkerProcessHost(ResourceDispatcherHost* resource_dispatcher_host_);
  ~WorkerProcessHost();

  // Starts the process.  Returns true iff it succeeded.
  bool Init();

  // Creates a worker object in the process.
  void CreateWorker(const WorkerInstance& instance);

  // Returns true iff the given message from a renderer process was forwarded to
  // the worker.
  bool FilterMessage(const IPC::Message& message, int sender_pid);

  void SenderShutdown(IPC::Message::Sender* sender);

 protected:
  friend class WorkerService;

  typedef std::list<WorkerInstance> Instances;
  const Instances& instances() const { return instances_; }

 private:
  // ResourceDispatcherHost::Receiver implementation:
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  // Called when a message arrives from the worker process.
  void OnMessageReceived(const IPC::Message& message);

  virtual bool CanShutdown() { return instances_.empty(); }

  // Updates the title shown in the task manager.
  void UpdateTitle();

  void OnCreateDedicatedWorker(const GURL& url,
                               int render_view_route_id,
                               int* route_id);
  void OnCancelCreateDedicatedWorker(int route_id);
  void OnForwardToWorker(const IPC::Message& message);

  Instances instances_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHost);
};

#endif  // CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
