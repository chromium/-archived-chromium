// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_agent_filter.h"

#include "chrome/common/devtools_messages.h"
#include "webkit/glue/webdevtoolsagent.h"

DevToolsAgentFilter::DevToolsAgentFilter() {
}

DevToolsAgentFilter::~DevToolsAgentFilter() {
}

bool DevToolsAgentFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgentFilter, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebuggerCommand,
                        OnDebuggerCommand)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsAgentFilter::OnDebuggerCommand(const std::string& command) {
  WebDevToolsAgent::ExecuteDebuggerCommand(command);
}
