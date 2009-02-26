// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/dev_tools_client.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/dev_tools_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"

DevToolsClient::DevToolsClient(RenderView* view)
    : render_view_(view) {
}

DevToolsClient::~DevToolsClient() {
}

void DevToolsClient::Send(const IPC::Message& tools_agent_message) {
  render_view_->Send(new ViewHostMsg_ForwardToDevToolsAgent(
      render_view_->routing_id(),
      tools_agent_message));
}

bool DevToolsClient::OnMessageReceived(const IPC::Message& message) {
  DCHECK(RenderThread::current()->message_loop() == MessageLoop::current());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsClient, message)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DidDebugAttach, DidDebugAttach)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DevToolsClient::DidDebugAttach() {
  DCHECK(RenderThread::current()->message_loop() == MessageLoop::current());
  // TODO(yurys): delegate to JS frontend.
}

