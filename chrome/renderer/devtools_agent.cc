// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_agent.h"

#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webdevtoolsagent.h"

DevToolsAgent::DevToolsAgent(int routing_id, RenderView* view)
    : routing_id_(routing_id),
      view_(view) {
}

DevToolsAgent::~DevToolsAgent() {
}

// Called on the Renderer thread.
bool DevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_RpcMessage, OnRpcMessage)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_InspectElement, OnInspectElement)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsAgent::SendMessageToClient(const std::string& raw_msg) {
  IPC::Message* m = new ViewHostMsg_ForwardToDevToolsClient(
      routing_id_,
      DevToolsClientMsg_RpcMessage(raw_msg));
  view_->Send(m);
}

void DevToolsAgent::OnAttach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->Attach();
  }
}

void DevToolsAgent::OnDetach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->Detach();
  }
}

void DevToolsAgent::OnRpcMessage(const std::string& raw_msg) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->DispatchMessageFromClient(raw_msg);
  }
}

void DevToolsAgent::OnInspectElement(int x, int y) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->Attach();
    web_agent->InspectElement(x, y);
  }
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  WebView* web_view = view_->webview();
  if (!web_view)
    return NULL;
  return web_view->GetWebDevToolsAgent();
}
