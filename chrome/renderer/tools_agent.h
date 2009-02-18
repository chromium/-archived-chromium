// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TOOLS_AGENT_H_
#define CHROME_RENDERER_TOOLS_AGENT_H_

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/renderer/tools_messages.h"
#include "webkit/glue/debugger_bridge.h"

class Message;
class RenderView;

// Inspected page end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the renderer side of the tools UI there's
// a corresponding ToolsClient object.
class ToolsAgent : public IPC::ChannelProxy::MessageFilter,
                   public DebuggerBridge::Delegate {
 public:
  // ToolsAgent is a field of the RenderView. The view is supposed to be alive
  // at least until OnFilterRemoved method is called.
  explicit ToolsAgent(RenderView* view);
  virtual ~ToolsAgent();

 private:
  // Sends message to ToolsClient.
  void Send(ToolsClientMessageType message_type, const std::wstring& body);

  // IPC::ChannelProxy::MessageFilter overrides:
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Called on IO thread.
  void OnToolsAgentMsg(int tools_message_type, const std::wstring& body);

  // Message filer's OnMessageReceived method is called on IO thread while
  // ToolsAgent messages have to be handled in render thread. This method is
  // called in render thread to handle tools agent message.
  void HandleMessageInRenderThread(int tools_message_type,
                                   const std::wstring& body);

  // Debugger::Delegate callback method to handle debugger output.
  void DebuggerOutput(const std::wstring& out);

  // Evaluate javascript URL in the renderer
  void EvaluateScript(const std::wstring& script);

  // All these OnXXX methods must be executed in render thread.
  void OnDebugAttach();
  void OnDebugDetach();
  void OnDebugBreak(bool force);
  void OnCommand(const std::wstring& cmd);

  scoped_refptr<DebuggerBridge> debugger_;
  RenderView* view_;
  MessageLoop* view_loop_;

  DISALLOW_COPY_AND_ASSIGN(ToolsAgent);
};

#endif  // CHROME_RENDERER_TOOLS_AGENT_H_

