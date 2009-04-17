// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_agent_filter.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/renderer/devtools_agent.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webdevtoolsagent.h"

namespace {

class DebuggerMessage : public WebDevToolsAgent::Message {
 public:
  DebuggerMessage(int routing_id, const IPC::Message& message)
      : routing_id_(routing_id),
        message_(message) {
  }

  virtual void Dispatch() {
    DevToolsAgent* agent = DevToolsAgent::FromHostId(routing_id_);
    if (agent) {
      agent->OnMessageReceived(message_);
    }
  }

 private:
  int routing_id_;
  IPC::Message message_;
  DISALLOW_COPY_AND_ASSIGN(DebuggerMessage);
};

}  // namespace

std::set<int> DevToolsAgentFilter::attached_routing_ids_;

DevToolsAgentFilter::DevToolsAgentFilter()
    : current_routing_id_(0) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  devtools_enabled_ = command_line.HasSwitch(
      switches::kEnableOutOfProcessDevTools);
}

DevToolsAgentFilter::~DevToolsAgentFilter() {
}

bool DevToolsAgentFilter::OnMessageReceived(const IPC::Message& message) {
  if (!devtools_enabled_) {
    return false;
  }
  if (message.type() < DevToolsAgentStart ||
      message.type() >= DevToolsAgentEnd) {
    return false;
  }

  int routing_id = message.routing_id();

  MessageLoop* view_loop = RenderThread::current()->message_loop();
  if (attached_routing_ids_.find(routing_id) == attached_routing_ids_.end()) {
    // Agent has not been scheduled for attachment yet.
    // Schedule attach command, no matter which one is actually being
    // dispatched.
    view_loop->PostTask(FROM_HERE, NewRunnableMethod(
        this,
        &DevToolsAgentFilter::Attach,
        message.routing_id()));
    attached_routing_ids_.insert(routing_id);

#if defined(OS_WIN)
    // Disable plugin message routing.
    PluginChannelHost::SetListening(false);
#endif
  }

  // If this is attach request - we are done.
  if (message.type() == DevToolsAgentMsg_Attach::ID) {
    return true;
  }

  if (message.type() == DevToolsAgentMsg_Detach::ID) {
    // We are about to schedule detach.
    attached_routing_ids_.erase(routing_id);
    if (attached_routing_ids_.size() == 0) {
#if defined(OS_WIN)
      // All the agents are scheduled for detach -> resume dispatching
      // of plugin messages.
      PluginChannelHost::SetListening(true);
#endif
    }
  }

  if (message.type() != DevToolsAgentMsg_DebuggerCommand::ID) {
    // Dispatch everything except for command through the debugger interrupt.
    DebuggerMessage* m = new DebuggerMessage(routing_id, message);
    WebDevToolsAgent::ScheduleMessageDispatch(m);
    view_loop->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &DevToolsAgentFilter::EvalNoop,
            routing_id));
    return true;
  } else {
    // Dispatch command directly from IO.
    bool handled = true;
    current_routing_id_ = routing_id;
    IPC_BEGIN_MESSAGE_MAP(DevToolsAgentFilter, message)
      IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebuggerCommand, OnDebuggerCommand)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }
}

void DevToolsAgentFilter::EvalNoop(int routing_id) {
  DevToolsAgent* agent = DevToolsAgent::FromHostId(routing_id);
  if (agent) {
    agent->render_view()->EvaluateScript(L"", L"javascript:void(0)");
  }
}

void DevToolsAgentFilter::Attach(int routing_id) {
  DevToolsAgent* agent = DevToolsAgent::FromHostId(routing_id);
  if (agent) {
    WebDevToolsAgent* web_agent = agent->GetWebAgent();
    if (web_agent) {
      web_agent->Attach();
    }
  }
}

void DevToolsAgentFilter::OnDebuggerCommand(const std::string& command) {
  WebDevToolsAgent::ExecuteDebuggerCommand(command, current_routing_id_);
}
