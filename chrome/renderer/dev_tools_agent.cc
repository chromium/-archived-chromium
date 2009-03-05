// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/dev_tools_agent.h"

#include "base/message_loop.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/dev_tools_messages.h"
// TODO(yurys): remove this macros once plugins available on other platforms
#if defined(OS_WIN)
#include "chrome/renderer/plugin_channel_host.h"
#endif  // OS_WIN
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_view.h"

DevToolsAgent::DevToolsAgent(RenderView* view, MessageLoop* view_loop)
    : debugger_(NULL),
      routing_id_(view->routing_id()),
      view_(view),
      view_loop_(view_loop),
      channel_(NULL),
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
  // It's possible that this will get cleared out from under us.
  MessageLoop* io_loop = io_loop_;
  if (!io_loop)
    return;

  IPC::Message* m = new ViewHostMsg_ForwardToDevToolsClient(
      routing_id_,
      tools_client_message);
  io_loop->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DevToolsAgent::SendFromIOThread, m));
}

void DevToolsAgent::SendFromIOThread(IPC::Message* message) {
  if (channel_) {
    channel_->Send(message);
  } else {
    delete message;
  }
}

// Called on IO thread.
void DevToolsAgent::OnFilterAdded(IPC::Channel* channel) {
  io_loop_ = MessageLoop::current();
  channel_ = channel;
}

// Called on IO thread.
bool DevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  DCHECK(MessageLoop::current() == io_loop_);

  if (message.routing_id() != routing_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebugAttach, OnDebugAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebugDetach, OnDebugDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebugBreak, OnDebugBreak)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DebugCommand, OnDebugCommand)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Called on IO thread.
void DevToolsAgent::OnFilterRemoved() {
  io_loop_ = NULL;
  channel_ = NULL;
}

void DevToolsAgent::DebuggerOutput(const std::wstring& out) {
  Send(DevToolsClientMsg_DebuggerOutput(out));
}

void DevToolsAgent::EvaluateScript(const std::wstring& script) {
  DCHECK(MessageLoop::current() == view_loop_);
  // view_ may have been cleared after this method execution was scheduled.
  if (view_)
    view_->EvaluateScript(L"", script);
}

void DevToolsAgent::OnDebugAttach() {
  DCHECK(MessageLoop::current() == io_loop_);
  if (!debugger_) {
    debugger_ = new DebuggerBridge(this);
  }

  debugger_->Attach();

  Send(DevToolsClientMsg_DidDebugAttach());

  // TODO(yurys): remove this macros once plugins available on other platforms
#if defined(OS_WIN)
  // Tell the plugin host to stop accepting messages in order to avoid
  // hangs while the renderer is paused.
  // TODO(yurys): It might be an improvement to add more plumbing to do this
  // when the renderer is actually paused vs. just the debugger being attached.
  // http://code.google.com/p/chromium/issues/detail?id=7556
  PluginChannelHost::SetListening(false);
#endif  // OS_WIN
}

void DevToolsAgent::OnDebugDetach() {
  DCHECK(MessageLoop::current() == io_loop_);
  if (debugger_)
    debugger_->Detach();
  // TODO(yurys): remove this macros once plugins available on other platforms
#if defined(OS_WIN)
  PluginChannelHost::SetListening(true);
#endif  // OS_WIN
}

void DevToolsAgent::OnDebugBreak(bool force) {
  DCHECK(MessageLoop::current() == io_loop_);
  // Set the debug break flag in the V8 engine.
  debugger_->Break(force);

  // If a forced break has been requested make sure that it will occour by
  // running some JavaScript in the renderer.
  if (force && view_loop_) {
    view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &DevToolsAgent::EvaluateScript,
        std::wstring(L"javascript:void(0)")));
  }
}

void DevToolsAgent::OnDebugCommand(const std::wstring& cmd) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (!debugger_) {
    NOTREACHED();
    std::wstring msg =
        StringPrintf(L"before attach, ignored command (%S)", cmd.c_str());
    DebuggerOutput(msg);
  } else {
    debugger_->Command(cmd);
  }
}

