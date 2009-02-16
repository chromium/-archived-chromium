// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TOOLS_CLIENT_H_
#define CHROME_RENDERER_TOOLS_CLIENT_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/renderer/tools_messages.h"
#include "webkit/glue/tools_proxy.h"

class MessageLoop;
class RenderView;

// Developer tools UI end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the side of the inspected page there's 
// corresponding ToolsAgent object.
class ToolsClient : public ToolsProxy,
                    public IPC::ChannelProxy::MessageFilter {
 public:
  explicit ToolsClient(RenderView* view);
  virtual ~ToolsClient();

 private:
  // DebuggerProxy overrides
  virtual void SetToolsUI(ToolsUI*);
  virtual void DebugAttach();
  virtual void DebugDetach();

  void OnDidDebugAttach();

  // Sends message to ToolsAgent.
  void Send(ToolsAgentMessageType message_type, const std::wstring& json_arg);

  // IPC::ChannelProxy::MessageFilter overrides:
  virtual bool OnMessageReceived(const IPC::Message& message);

  void OnToolsClientMessage(int tools_message_type, const std::wstring& body);
  void HandleMessageInRenderThread(int tools_message_type, 
                                   const std::wstring& body);

  ToolsUI* tools_ui_;
  RenderView* render_view_;
  MessageLoop* view_loop_;

  DISALLOW_COPY_AND_ASSIGN(ToolsClient);
};

#endif // CHROME_RENDERER_TOOLS_CLIENT_H_
