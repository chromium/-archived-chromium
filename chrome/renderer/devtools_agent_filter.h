// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVTOOLS_AGENT_FILTER_H_
#define CHROME_RENDERER_DEVTOOLS_AGENT_FILTER_H_

#include <set>
#include <string>

#include "chrome/common/ipc_channel_proxy.h"

class WebDevToolsAgent;

// DevToolsAgentFilter is registered as an IPC filter in order to be able to
// dispatch messages while on the IO thread. The reason for that is that while
// debugging, Render thread is being held by the v8 and hence no messages
// are being dispatched there. While holding the thread in a tight loop,
// v8 provides thread-safe Api for controlling debugger. In our case v8's Api
// is being used from this communication agent on the IO thread.
class DevToolsAgentFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  // There is a single instance of this class instantiated by the RenderThread.
  DevToolsAgentFilter();
  virtual ~DevToolsAgentFilter();

 private:
  // IPC::ChannelProxy::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message);

  static void DispatchMessageLoop();

  // OnDebuggerCommand will be executed in the IO thread so that we can
  // handle debug messages even when v8 is stopped.
  void OnDebuggerCommand(const std::string& command);

  int current_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgentFilter);
};

#endif  // CHROME_RENDERER_DEVTOOLS_AGENT_FILTER_H_
