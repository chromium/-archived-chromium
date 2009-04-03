// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DevToolsAgent belongs to the inspectable renderer and provides Glue's
// agents with the communication capabilities. All messages from/to Glue's
// agents infrastructure are flowing through this comminucation agent.
//
// DevToolsAgent is registered as an IPC filter in order to be able to
// dispatch messages while on the IO thread. The reason for that is that while
// debugging, Render thread is being held by the v8 and hence no messages
// are being dispatched there. While holding the thread in a tight loop,
// v8 provides thread-safe Api for controlling debugger. In our case v8's Api
// is being used from this communication agent on the IO thread.

#include "chrome/renderer/devtools_agent.h"

#include "base/message_loop.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
// TODO(yurys): remove this macros once plugins available on other platforms
#if defined(OS_WIN)
#include "chrome/renderer/plugin_channel_host.h"
#endif  // OS_WIN
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webdevtoolsagent.h"

DevToolsAgent::DevToolsAgent(int routing_id,
                             RenderView* view,
                             MessageLoop* view_loop)
    : routing_id_(routing_id),
      view_(view),
      view_loop_(view_loop),
      io_loop_(NULL) {
}

DevToolsAgent::~DevToolsAgent() {
}

// Called on render thread.
void DevToolsAgent::RenderViewDestroyed() {
  DCHECK(MessageLoop::current() == view_loop_);
  view_ = NULL;
}

void DevToolsAgent::Send(const IPC::Message& tools_client_message) {
  DCHECK(MessageLoop::current() == view_loop_);
  if (!view_) {
    return;
  }

  IPC::Message* m = new ViewHostMsg_ForwardToDevToolsClient(
      routing_id_,
      tools_client_message);
  view_->Send(m);
}

// Called on the IO thread.
void DevToolsAgent::OnFilterAdded(IPC::Channel* channel) {
  io_loop_ = MessageLoop::current();
}

// Called on the IO thread.
bool DevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  DCHECK(MessageLoop::current() == io_loop_);

  if (message.routing_id() != routing_id_)
    return false;

  // TODO(yurys): only DebuggerCommand message is handled on the IO thread
  // all other messages could be handled in RenderView::OnMessageReceived. With
  // that approach we wouldn't have to call view_loop_->PostTask for each of the
  // messages.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_RpcMessage, OnRpcMessage)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebuggerCommand,
                        OnDebuggerCommand)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_InspectElement, OnInspectElement)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Called on the IO thread.
void DevToolsAgent::OnFilterRemoved() {
  io_loop_ = NULL;
}

void DevToolsAgent::EvaluateScript(const std::wstring& script) {
  DCHECK(MessageLoop::current() == view_loop_);
  // view_ may have been cleared after this method execution was scheduled.
  if (view_)
    view_->EvaluateScript(L"", script);
}

void DevToolsAgent::OnAttach() {
  view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DevToolsAgent::Attach));
}

void DevToolsAgent::OnDetach() {
  view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DevToolsAgent::Detach));
}

void DevToolsAgent::SendMessageToClient(const std::string& raw_msg) {
  Send(DevToolsClientMsg_RpcMessage(raw_msg));
}

void DevToolsAgent::OnRpcMessage(const std::string& raw_msg) {
  view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DevToolsAgent::DispatchRpcMessage, raw_msg));
}

void DevToolsAgent::OnInspectElement(int x, int y) {
  view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DevToolsAgent::InspectElement, x, y));
}

void DevToolsAgent::Attach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->Attach();
  }
}

void DevToolsAgent::Detach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->Detach();
  }
}

void DevToolsAgent::DispatchRpcMessage(const std::string& raw_msg) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->DispatchMessageFromClient(raw_msg);
  }
}

void DevToolsAgent::OnDebuggerCommand(const std::string& command) {
  // Debugger commands are handled on the IO thread.
  WebDevToolsAgent::ExecuteDebuggerCommand(command);
}

void DevToolsAgent::InspectElement(int x, int y) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->InspectElement(x, y);
  }
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  if (!view_)
    return NULL;
  WebView* web_view = view_->webview();
  if (!web_view)
    return NULL;
  return web_view->GetWebDevToolsAgent();
}
