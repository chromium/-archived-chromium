// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/tools_client.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/tools_proxy.h"

ToolsClient::ToolsClient(RenderView* view)
    : tools_ui_(NULL),
      render_view_(view),
      view_loop_(MessageLoop::current()) {
}

ToolsClient::~ToolsClient() {
  tools_ui_ = NULL;
}

void ToolsClient::Send(ToolsAgentMessageType message_type, 
                       const std::wstring& json_arg) {
  render_view_->Send(new ViewHostMsg_ToolsAgentMsg(
      render_view_->routing_id(), message_type, json_arg));
}

// IPC::ChannelProxy::MessageFilter overrides:
bool ToolsClient::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() != render_view_->routing_id()) {
    NOTREACHED();
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ToolsClient, message)
    IPC_MESSAGE_HANDLER(ViewMsg_ToolsClientMsg, OnToolsClientMessage)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ToolsClient::OnToolsClientMessage(int tools_message_type, 
                                       const std::wstring& body) {
  view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, 
      &ToolsClient::HandleMessageInRenderThread,
      tools_message_type,
      body));
}

void ToolsClient::HandleMessageInRenderThread(int tools_message_type, 
                                              const std::wstring& body) {
  DCHECK(view_loop_ == MessageLoop::current());

  switch(tools_message_type) {
    case TOOLS_CLIENT_MSG_DID_DEBUG_ATTACH:
      OnDidDebugAttach();
      break;
    default:
      NOTREACHED() << "Unknown message type: " << tools_message_type;
  }
}

void ToolsClient::OnDidDebugAttach() {
  if (tools_ui_)
    tools_ui_->OnDidDebugAttach();
}

// DebuggerProxy methods:
void ToolsClient::SetToolsUI(ToolsUI* tools_ui) {
  tools_ui_ = tools_ui;
}

void ToolsClient::DebugAttach() {
  Send(TOOLS_AGENT_MSG_DEBUG_ATTACH, L"");
}

void ToolsClient::DebugDetach() {
  Send(TOOLS_AGENT_MSG_DEBUG_DETACH, L"");
}
