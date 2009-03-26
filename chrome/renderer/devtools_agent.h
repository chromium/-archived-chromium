// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVTOOLS_AGENT_H_
#define CHROME_RENDERER_DEVTOOLS_AGENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/renderer/devtools_messages.h"
#include "webkit/glue/debugger_bridge.h"
#include "webkit/glue/webdevtoolsagent_delegate.h"

class MessageLoop;
class RenderView;

// Inspected page end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the renderer side of the tools UI there's
// a corresponding ToolsClient object.
class DevToolsAgent : public IPC::ChannelProxy::MessageFilter,
                      public DebuggerBridge::Delegate,
                      public WebDevToolsAgentDelegate {
 public:
  // DevToolsAgent is a field of the RenderView. The view is supposed to remove
  // this agent from message filter list on IO thread before dying.
  DevToolsAgent(int routing_id, RenderView* view, MessageLoop* view_loop);
  virtual ~DevToolsAgent();

  // WebDevToolsAgentDelegate implementation
  virtual void SendMessageToClient(const std::string& raw_msg);

  // DevToolsAgent is created by RenderView which is supposed to call this
  // method from its destructor.
  void RenderViewDestroyed();

 private:
  // Sends message to DevToolsClient. May be called on any thread.
  void Send(const IPC::Message& tools_client_message);

  // Sends message to DevToolsClient. Must be called on IO thread. Takes
  // ownership of the message.
  void SendFromIOThread(IPC::Message* message);

  // IPC::ChannelProxy::MessageFilter overrides. Called on IO thread.
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnFilterRemoved();

  // Debugger::Delegate callback method to handle debugger output.
  void DebuggerOutput(const std::wstring& out);

  void DispatchRpcMessage(const std::string& raw_msg);

  void InspectElement(int x, int y);

  // Evaluate javascript URL in the renderer
  void EvaluateScript(const std::wstring& script);

  // All these OnXXX methods will be executed in IO thread so that we can
  // handle debug messages even when v8 is stopped.
  void OnDebugAttach();
  void OnDebugDetach();
  void OnDebugBreak(bool force);
  void OnDebugCommand(const std::wstring& cmd);
  void OnRpcMessage(const std::string& raw_msg);
  void OnInspectElement(int x, int y);

  scoped_refptr<DebuggerBridge> debugger_;

  int routing_id_; //  View routing id that we can access from IO thread.
  RenderView* view_;
  MessageLoop* view_loop_;

  IPC::Channel* channel_;
  MessageLoop* io_loop_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

#endif  // CHROME_RENDERER_DEVTOOLS_AGENT_H_
