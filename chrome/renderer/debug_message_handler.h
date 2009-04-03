// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MessageFilter object to handle messages aimed at the debugger, and to
// dispatch them from the main thread rather than the render thread.
// Also owns the reference to the Debugger object and handles callbacks from it.

#ifndef CHROME_RENDERER_DEBUG_MESSAGE_HANDLER_H_
#define CHROME_RENDERER_DEBUG_MESSAGE_HANDLER_H_

#include "ipc/ipc_channel_proxy.h"
#include "webkit/glue/debugger_bridge.h"

class RenderView;

class DebugMessageHandler : public IPC::ChannelProxy::MessageFilter,
                            public DebuggerBridge::Delegate {
 public:
  DebugMessageHandler(RenderView* view);
  virtual ~DebugMessageHandler();

 private:
  // Evaluate javascript URL in the renderer
  void EvaluateScript(const std::wstring& script);

  // Attach in the renderer
  void Attach();

  // Debugger::Delegate callback method to handle debugger output.
  void DebuggerOutput(const std::wstring& out);

  // Schedule a debugger break.
  void OnBreak(bool force);

  // Sends a command to the debugger.
  void OnCommand(const std::wstring& cmd);

  // Sends an attach event to the debugger front-end.
  void OnAttach();

  // Unregister with V8 and notify the RenderView.
  void OnDetach();

  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnFilterRemoved();

  // Returns true to indicate that the message was handled, or false to let
  // the message be handled in the default way.
  virtual bool OnMessageReceived(const IPC::Message& message);

  scoped_refptr<DebuggerBridge> debugger_;

  // Don't ever dereference view_ directly from another thread as it's not
  // threadsafe, instead proxy locally via its MessageLoop.
  RenderView* view_;
  MessageLoop* view_loop_;

  int32 view_routing_id_;
  IPC::Channel* channel_;
};

#endif // CHROME_RENDERER_DEBUG_MESSAGE_HANDLER_H_
