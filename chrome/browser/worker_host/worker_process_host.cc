// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_process_host.h"

#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/worker_messages.h"


WorkerProcessHost::WorkerProcessHost(
    ResourceDispatcherHost* resource_dispatcher_host_)
    : ChildProcessHost(WORKER_PROCESS, resource_dispatcher_host_) {
}

WorkerProcessHost::~WorkerProcessHost() {
}

bool WorkerProcessHost::Init() {
  // TODO(jabdelmalek): figure out what to set as the title.
  set_name(L"TBD");

  if (!CreateChannel())
    return false;

  std::wstring exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path))
    return false;

  CommandLine cmd_line(exe_path);

  // TODO(jabdelmalek): factor out common code from renderer/plugin that does
  // sandboxing and command line copying and reuse here.
  cmd_line.AppendSwitchWithValue(switches::kProcessType,
                                 switches::kWorkerProcess);
  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID, channel_id());
  base::ProcessHandle handle;
  if (!base::LaunchApp(cmd_line, false, false, &handle))
    return false;
  SetHandle(handle);

  return true;
}

void WorkerProcessHost::CreateWorker(const GURL& url,
                                     int worker_route_id,
                                     int renderer_route_id,
                                     ResourceMessageFilter* filter) {
  WorkerInstance instance;
  instance.worker_route_id = worker_route_id;
  instance.renderer_route_id = renderer_route_id;
  instance.filter = filter;
  instances_.push_back(instance);
  Send(new WorkerProcessMsg_CreateWorker(url, worker_route_id));
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
