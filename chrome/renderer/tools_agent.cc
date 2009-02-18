// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/tools_agent.h"

#include "chrome/common/render_messages.h"
// TODO(yurys): remove this macros once plugins available on other platforms
#if defined(OS_WIN)
#include "chrome/renderer/plugin_channel_host.h"
#endif  // OS_WIN
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_view.h"

ToolsAgent::ToolsAgent(RenderView* view)
    : debugger_(NULL),
      view_(view),
      view_loop_(g_render_thread->message_loop()) {
}

ToolsAgent::~ToolsAgent() {
}

void ToolsAgent::Send(ToolsClientMessageType message_type,
                      const std::wstring& body) {
  view_->Send(new ViewHostMsg_ToolsClientMsg(
      view_->routing_id(), message_type, body));
}

// Called on IO thread.
bool ToolsAgent::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() != view_->routing_id())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ToolsAgent, message)
    IPC_MESSAGE_HANDLER(ViewMsg_ToolsAgentMsg, OnToolsAgentMsg)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Called on IO thread.
void ToolsAgent::OnToolsAgentMsg(int tools_message_type,
                                 const std::wstring& body) {
  view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this,
      &ToolsAgent::HandleMessageInRenderThread,
      tools_message_type,
      body));
}

void ToolsAgent::HandleMessageInRenderThread(
    int tools_message_type, const std::wstring& body) {
  DCHECK(MessageLoop::current() == view_loop_);

  switch (tools_message_type) {
    case TOOLS_AGENT_MSG_DEBUG_ATTACH:
      OnDebugAttach();
      break;
    case TOOLS_AGENT_MSG_DEBUG_DETACH:
      OnDebugDetach();
      break;
    case TOOLS_AGENT_MSG_DEBUG_BREAK:
      OnDebugBreak(body == L"true");
      break;
    case TOOLS_AGENT_MSG_DEBUG_COMMAND:
      OnCommand(body);
      break;
    default:
      NOTREACHED() << "Unknown ToolsAgentMessageType: " << tools_message_type;
  }
}

void ToolsAgent::DebuggerOutput(const std::wstring& out) {
  Send(TOOLS_CLIENT_MSG_DEBUGGER_OUTPUT, out);
}

void ToolsAgent::EvaluateScript(const std::wstring& script) {
  DCHECK(MessageLoop::current() == view_loop_);
  // It's possible that this will get cleared out from under us.
  view_->EvaluateScript(L"", script);
}

void ToolsAgent::OnDebugAttach() {
  DCHECK(MessageLoop::current() == view_loop_);
  if (!debugger_) {
    debugger_ = new DebuggerBridge(this);
  }

  debugger_->Attach();

  Send(TOOLS_CLIENT_MSG_DID_DEBUG_ATTACH, L"");

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

void ToolsAgent::OnDebugDetach() {
  DCHECK(MessageLoop::current() == view_loop_);
  if (debugger_)
    debugger_->Detach();
// TODO(yurys): remove this macros once plugins available on other platforms
#if defined(OS_WIN)
  PluginChannelHost::SetListening(true);
#endif  // OS_WIN
}

void ToolsAgent::OnDebugBreak(bool force) {
  DCHECK(MessageLoop::current() == view_loop_);
  // Set the debug break flag in the V8 engine.
  debugger_->Break(force);

  // If a forced break has been requested make sure that it will occour by
  // running some JavaScript in the renderer.
  if (force)
    EvaluateScript(std::wstring(L"javascript:void(0)"));
}

void ToolsAgent::OnCommand(const std::wstring& cmd) {
  DCHECK(MessageLoop::current() == view_loop_);
  if (!debugger_) {
    NOTREACHED();
    std::wstring msg =
        StringPrintf(L"before attach, ignored command (%S)", cmd.c_str());
    DebuggerOutput(msg);
  } else {
    debugger_->Command(cmd);
  }
}

