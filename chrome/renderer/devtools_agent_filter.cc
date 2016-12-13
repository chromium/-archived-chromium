// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_agent_filter.h"

#include "base/message_loop.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/renderer/devtools_agent.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webdevtoolsagent.h"

// static
void DevToolsAgentFilter::DispatchMessageLoop() {
  MessageLoop* current = MessageLoop::current();
  bool old_state = current->NestableTasksAllowed();
  current->SetNestableTasksAllowed(true);
  current->RunAllPending();
  current->SetNestableTasksAllowed(old_state);
}

DevToolsAgentFilter::DevToolsAgentFilter()
    : current_routing_id_(0) {
  WebDevToolsAgent::SetMessageLoopDispatchHandler(
      &DevToolsAgentFilter::DispatchMessageLoop);
}

DevToolsAgentFilter::~DevToolsAgentFilter() {
}

bool DevToolsAgentFilter::OnMessageReceived(const IPC::Message& message) {
  if (message.type() == DevToolsAgentMsg_DebuggerCommand::ID) {
    // Dispatch command directly from IO.
    bool handled = true;
    current_routing_id_ = message.routing_id();
    IPC_BEGIN_MESSAGE_MAP(DevToolsAgentFilter, message)
      IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebuggerCommand, OnDebuggerCommand)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  } else {
    return false;
  }
}

void DevToolsAgentFilter::OnDebuggerCommand(const std::string& command) {
  WebDevToolsAgent::ExecuteDebuggerCommand(command, current_routing_id_);
}
