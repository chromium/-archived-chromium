// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// MessageFilter object to handle messages aimed at the debugger, and to
// dispatch them from the main thread rather than the render thread.
// Also owns the reference to the Debugger object and handles callbacks from it.

#ifndef CHROME_RENDERER_DEBUG_MESSAGE_HANDLER_H_
#define CHROME_RENDERER_DEBUG_MESSAGE_HANDLER_H_

#include "chrome/common/ipc_channel_proxy.h"
#include "webkit/glue/debugger.h"

class RenderView;

class DebugMessageHandler : public IPC::ChannelProxy::MessageFilter,
                            public Debugger::Delegate {
 public:
  DebugMessageHandler(RenderView* view);
  virtual ~DebugMessageHandler();

 private:
  // evaluate javascript URL in the renderer
  void EvaluateScriptUrl(const std::wstring& url);

  // Debugger::Delegate callback method to handle debugger output.
  void DebuggerOutput(const std::wstring& out);

  // Sends a command to the debugger.
  void OnSendToDebugger(const std::wstring& cmd);

  // Sends an attach event to the debugger front-end.
  void OnAttach();

  // Unregister with V8 and notify the RenderView.
  void OnDetach();

  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnFilterRemoved();

  // Returns true to indicate that the message was handled, or false to let
  // the message be handled in the default way.
  virtual bool OnMessageReceived(const IPC::Message& message);

  scoped_refptr<Debugger> debugger_;

  // Don't ever dereference view_ directly from another thread as it's not
  // threadsafe, instead proxy locally via its MessageLoop.
  RenderView* view_;
  MessageLoop* view_loop_;

  int32 view_routing_id_;
  IPC::Channel* channel_;
};

#endif // CHROME_RENDERER_DEBUG_MESSAGE_HANDLER_H_