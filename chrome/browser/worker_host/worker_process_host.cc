// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_process_host.h"

#include <set>

#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/worker_messages.h"
#include "net/base/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "chrome/browser/sandbox_policy.h"
#endif

// Notifies RenderViewHost that one or more worker objects crashed.
class WorkerCrashTask : public Task {
 public:
  WorkerCrashTask(int render_process_id, int render_view_id)
      : render_process_id_(render_process_id),
        render_view_id_(render_view_id) { }

  void Run() {
    RenderViewHost* host =
        RenderViewHost::FromID(render_process_id_, render_view_id_);
    if (host)
      host->delegate()->OnCrashedWorker();
  }

 private:
  int render_process_id_;
  int render_view_id_;
};


WorkerProcessHost::WorkerProcessHost(
    ResourceDispatcherHost* resource_dispatcher_host_)
    : ChildProcessHost(WORKER_PROCESS, resource_dispatcher_host_) {
}

WorkerProcessHost::~WorkerProcessHost() {
  // If we crashed, tell the RenderViewHost.
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    i->filter->ui_loop()->PostTask(FROM_HERE, new WorkerCrashTask(
        i->filter->GetProcessId(), i->render_view_route_id));
  }
}

bool WorkerProcessHost::Init() {
  if (!CreateChannel())
    return false;

  std::wstring exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path))
    return false;

  CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchWithValue(switches::kProcessType,
                                 switches::kWorkerProcess);
  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID, channel_id());
  base::ProcessHandle process;
#if defined(OS_WIN)
  process = sandbox::StartProcess(&cmd_line);
#else
  base::LaunchApp(cmd_line, false, false, &process);
#endif
  if (!process)
    return false;
  SetHandle(process);

  return true;
}

void WorkerProcessHost::CreateWorker(const GURL& url,
                                     int render_view_route_id,
                                     int worker_route_id,
                                     int renderer_route_id,
                                     ResourceMessageFilter* filter) {
  WorkerInstance instance;
  instance.url = url;
  instance.render_view_route_id = render_view_route_id;
  instance.worker_route_id = worker_route_id;
  instance.renderer_route_id = renderer_route_id;
  instance.filter = filter;
  instances_.push_back(instance);
  Send(new WorkerProcessMsg_CreateWorker(url, worker_route_id));

  UpdateTitle();
}

bool WorkerProcessHost::FilterMessage(const IPC::Message& message) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->renderer_route_id == message.routing_id()) {
      IPC::Message* new_message = new IPC::Message(message);
      new_message->set_routing_id(i->worker_route_id);
      Send(new_message);
      return true;
    }
  }

  return false;
}

URLRequestContext* WorkerProcessHost::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  return NULL;
}

void WorkerProcessHost::OnMessageReceived(const IPC::Message& message) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id == message.routing_id()) {
      IPC::Message* new_message = new IPC::Message(message);
      new_message->set_routing_id(i->renderer_route_id);
      i->filter->Send(new_message);

      if (message.type() == WorkerHostMsg_WorkerContextDestroyed::ID) {
        instances_.erase(i);
        UpdateTitle();
      }
      break;
    }
  }
}

void WorkerProcessHost::RendererShutdown(ResourceMessageFilter* filter) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    if (i->filter == filter) {
      i = instances_.erase(i);
    } else {
      ++i;
    }
  }
}

void WorkerProcessHost::UpdateTitle() {
  std::set<std::string> worker_domains;
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    worker_domains.insert(
      net::RegistryControlledDomainService::GetDomainAndRegistry(i->url));
  }

  std::string title;
  for (std::set<std::string>::iterator i = worker_domains.begin();
       i != worker_domains.end(); ++i) {
    if (!title.empty())
      title += ", ";
    title += *i;
  }

  set_name(ASCIIToWide(title));
}
