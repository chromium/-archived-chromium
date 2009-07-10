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
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/renderer_host/render_view_host.h"
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
    if (host) {
      RenderViewHostDelegate::BrowserIntegration* integration_delegate =
          host->delegate()->GetBrowserIntegrationDelegate();
      if (integration_delegate)
        integration_delegate->OnCrashedWorker();
    }
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
  WorkerService::GetInstance()->OnSenderShutdown(this);
  WorkerService::GetInstance()->OnWorkerProcessDestroyed(this);

  // If we crashed, tell the RenderViewHost.
  MessageLoop* ui_loop = WorkerService::GetInstance()->ui_loop();
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    ui_loop->PostTask(FROM_HERE, new WorkerCrashTask(
        i->renderer_process_id, i->render_view_route_id));
  }

  ChildProcessSecurityPolicy::GetInstance()->Remove(GetProcessId());
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
  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID,
                                 ASCIIToWide(channel_id()));

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNativeWebWorkers)) {
    cmd_line.AppendSwitch(switches::kEnableNativeWebWorkers);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebWorkerShareProcesses)) {
    cmd_line.AppendSwitch(switches::kWebWorkerShareProcesses);
  }

  base::ProcessHandle process;
#if defined(OS_WIN)
  process = sandbox::StartProcess(&cmd_line);
#else
  base::LaunchApp(cmd_line, false, false, &process);
#endif
  if (!process)
    return false;
  SetHandle(process);

  ChildProcessSecurityPolicy::GetInstance()->Add(GetProcessId());

  return true;
}

void WorkerProcessHost::CreateWorker(const WorkerInstance& instance) {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestURL(
      GetProcessId(), instance.url);

  instances_.push_back(instance);
  Send(new WorkerProcessMsg_CreateWorker(
      instance.url, instance.worker_route_id));

  UpdateTitle();
  instances_.back().sender->Send(
      new ViewMsg_DedicatedWorkerCreated(instance.sender_route_id));
}

bool WorkerProcessHost::FilterMessage(const IPC::Message& message,
                                      int sender_pid) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->sender_pid == sender_pid &&
        i->sender_route_id == message.routing_id()) {
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
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerProcessHost, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateDedicatedWorker,
                        OnCreateDedicatedWorker)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelCreateDedicatedWorker,
                        OnCancelCreateDedicatedWorker)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker,
                        OnForwardToWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  if (handled)
    return;

  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id == message.routing_id()) {
      IPC::Message* new_message = new IPC::Message(message);
      new_message->set_routing_id(i->sender_route_id);
      i->sender->Send(new_message);

      if (message.type() == WorkerHostMsg_WorkerContextDestroyed::ID) {
        instances_.erase(i);
        UpdateTitle();
      }
      break;
    }
  }
}

void WorkerProcessHost::SenderShutdown(IPC::Message::Sender* sender) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    if (i->sender == sender) {
      Send(new WorkerMsg_TerminateWorkerContext(i->worker_route_id));
      i = instances_.erase(i);
    } else {
      ++i;
    }
  }
}

void WorkerProcessHost::UpdateTitle() {
  std::set<std::string> titles;
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    std::string title =
        net::RegistryControlledDomainService::GetDomainAndRegistry(i->url);
    // Use the host name if the domain is empty, i.e. localhost or IP address.
    if (title.empty())
      title = i->url.host();
    // If the host name is empty, i.e. file url, use the path.
    if (title.empty())
      title = i->url.path();
    titles.insert(title);
  }

  std::string display_title;
  for (std::set<std::string>::iterator i = titles.begin();
       i != titles.end(); ++i) {
    if (!display_title.empty())
      display_title += ", ";
    display_title += *i;
  }

  set_name(ASCIIToWide(display_title));
}

void WorkerProcessHost::OnCreateDedicatedWorker(const GURL& url,
                                                int render_view_route_id,
                                                int* route_id) {
  DCHECK(instances_.size() == 1);  // Only called when one process per worker.
  *route_id = WorkerService::GetInstance()->next_worker_route_id();
  WorkerService::GetInstance()->CreateDedicatedWorker(
      url, instances_.front().renderer_process_id,
      instances_.front().render_view_route_id, this, GetProcessId(), *route_id);
}

void WorkerProcessHost::OnCancelCreateDedicatedWorker(int route_id) {
  WorkerService::GetInstance()->CancelCreateDedicatedWorker(
      GetProcessId(), route_id);
}

void WorkerProcessHost::OnForwardToWorker(const IPC::Message& message) {
  WorkerService::GetInstance()->ForwardMessage(message, GetProcessId());
}
