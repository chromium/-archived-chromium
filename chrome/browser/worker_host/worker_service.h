// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_
#define CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_

#include <list>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"


class MessageLoop;
class WorkerProcessHost;
class ResourceDispatcherHost;

class WorkerService : public NotificationObserver {
 public:
  // Returns the WorkerService singleton.
  static WorkerService* GetInstance();

  // Initialize the WorkerService.  OK to be called multiple times.
  void Initialize(ResourceDispatcherHost* rdh, MessageLoop* ui_loop);

  // Creates a dedicated worker.  Returns true on success.
  bool CreateDedicatedWorker(const GURL &url,
                             int renderer_process_id,
                             int render_view_route_id,
                             IPC::Message::Sender* sender,
                             int sender_pid,
                             int sender_route_id);

  // Called by the worker creator when a message arrives that should be
  // forwarded to the worker process.
  void ForwardMessage(const IPC::Message& message, int sender_pid);

  // NotificationObserver interface.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  void NotifySenderShutdown(IPC::Message::Sender* sender);

  MessageLoop* ui_loop() { return ui_loop_; }

  int next_worker_route_id() { return ++next_worker_route_id_; }

 private:
  friend struct DefaultSingletonTraits<WorkerService>;

  WorkerService();
  ~WorkerService();

  // Returns a WorkerProcessHost object if one exists for the given domain, or
  // NULL if there are no such workers yet.
  WorkerProcessHost* GetProcessForDomain(const GURL& url);

  // Returns a WorkerProcessHost based on a strategy of creating one worker per
  // core.
  WorkerProcessHost* GetProcessToFillUpCores();

  // Returns the WorkerProcessHost from the existing set that has the least
  // number of worker instance running.
  WorkerProcessHost* GetLeastLoadedWorker();

  NotificationRegistrar registrar_;
  int next_worker_route_id_;
  ResourceDispatcherHost* resource_dispatcher_host_;
  MessageLoop* ui_loop_;

  DISALLOW_COPY_AND_ASSIGN(WorkerService);
};

#endif  // CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_
