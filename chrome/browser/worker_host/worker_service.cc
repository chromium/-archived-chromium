// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_service.h"

#include "base/command_line.h"
#include "base/singleton.h"
#include "base/sys_info.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/worker_host/worker_process_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "net/base/registry_controlled_domain.h"

namespace {
static const int kMaxWorkerProcesses = 10;
}

WorkerService* WorkerService::GetInstance() {
  return Singleton<WorkerService>::get();
}

WorkerService::WorkerService()
    : next_worker_route_id_(0),
      resource_dispatcher_host_(NULL),
      ui_loop_(NULL) {
  // Receive a notification if the message filter is deleted.
  registrar_.Add(this, NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
                 NotificationService::AllSources());
}

void WorkerService::Initialize(ResourceDispatcherHost* rdh,
                               MessageLoop* ui_loop) {
  resource_dispatcher_host_ = rdh;
  ui_loop_ = ui_loop;
}

WorkerService::~WorkerService() {
}

bool WorkerService::CreateDedicatedWorker(const GURL &url,
                                          int renderer_process_id,
                                          int render_view_route_id,
                                          IPC::Message::Sender* sender,
                                          int sender_pid,
                                          int sender_route_id) {
  WorkerProcessHost* worker = NULL;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebWorkerProcessPerCore)) {
    worker = GetProcessToFillUpCores();
  } else if (CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kWebWorkerShareProcesses)) {
    worker = GetProcessForDomain(url);
  }

  if (!worker) {
    worker = new WorkerProcessHost(resource_dispatcher_host_);
    if (!worker->Init()) {
      delete worker;
      return false;
    }
  }

  // Generate a unique route id for the browser-worker communication that's
  // unique among all worker processes.  That way when the worker process sends
  // a wrapped IPC message through us, we know which WorkerProcessHost to give
  // it to.
  worker->CreateWorker(url, renderer_process_id, render_view_route_id,
                       next_worker_route_id(), sender, sender_pid,
                       sender_route_id);

  return true;
}

void WorkerService::ForwardMessage(const IPC::Message& message,
                                   int sender_pid) {
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    if (worker->FilterMessage(message, sender_pid))
      return;
  }

  // TODO(jabdelmalek): tell sender that callee is gone
}

WorkerProcessHost* WorkerService::GetProcessForDomain(const GURL& url) {
  int num_processes = 0;
  std::string domain =
      net::RegistryControlledDomainService::GetDomainAndRegistry(url);
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    num_processes++;
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    for (WorkerProcessHost::Instances::const_iterator instance =
             worker->instances().begin();
         instance != worker->instances().end(); ++instance) {
      if (net::RegistryControlledDomainService::GetDomainAndRegistry(
          instance->url) == domain) {
        return worker;
      }
    }
  }

  if (num_processes >= kMaxWorkerProcesses)
    return GetLeastLoadedWorker();

  return NULL;
}

WorkerProcessHost* WorkerService::GetProcessToFillUpCores() {
  int num_processes = 0;
  ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
  for (; !iter.Done(); ++iter)
    num_processes++;

  if (num_processes >= base::SysInfo::NumberOfProcessors())
    return GetLeastLoadedWorker();

  return NULL;
}

WorkerProcessHost* WorkerService::GetLeastLoadedWorker() {
  WorkerProcessHost* smallest = NULL;
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    if (!smallest || worker->instances().size() < smallest->instances().size())
      smallest = worker;
  }

  return smallest;
}

void WorkerService::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  DCHECK(type.value == NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN);
  ResourceMessageFilter* filter = Source<ResourceMessageFilter>(source).ptr();
  NotifySenderShutdown(filter);
}

void WorkerService::NotifySenderShutdown(IPC::Message::Sender* sender) {
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    worker->SenderShutdown(sender);
  }
}
