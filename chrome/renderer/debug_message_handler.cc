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

#include "chrome/renderer/debug_message_handler.h"
#include "chrome/renderer/render_view.h"

////////////////////////////////////////
// methods called from the RenderThread

DebugMessageHandler::DebugMessageHandler(RenderView* view) :
    debugger_(NULL), view_(view), channel_(NULL) {
  view_loop_ = MessageLoop::current();
  view_routing_id_ = view_->routing_id();
}

DebugMessageHandler::~DebugMessageHandler() {
}

void DebugMessageHandler::EvaluateScriptUrl(const std::wstring& url) {
  DCHECK(MessageLoop::current() == view_loop_);
  // It's possible that this will get cleared out from under us.
  RenderView* view = view_;
  if (view) {
    view->EvaluateScriptUrl(L"", url);
  }
}

///////////////////////////////////////////////
// all methods below called from the IO thread

void DebugMessageHandler::DebuggerOutput(const std::wstring& out) {
  DCHECK(MessageLoop::current() != view_loop_);
  // When certain commands are sent to the debugger, but we're not paused,
  // it's possible that no JS is running, so these commands can't be processed.
  // In these cases, we force some trivial, no side-effect, JS to be executed
  // so that V8 can process the commands.
  if (((out == L"break set") || (out == L"request queued")) && view_loop_) {
    if (view_loop_) {
      view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &DebugMessageHandler::EvaluateScriptUrl,
          std::wstring(L"javascript:void(0)")));
    }
  } else if (channel_) {
    channel_->Send(new ViewHostMsg_DebuggerOutput(view_routing_id_, out));
  }
}

void DebugMessageHandler::OnAttach() {
  DebuggerOutput(L"{'type':'event', 'event':'attach'}");
  debugger_->Attach();
}

void DebugMessageHandler::OnSendToDebugger(const std::wstring& cmd) {
  if (!debugger_) {
    debugger_ = new Debugger(this);
    if (cmd == L"" || cmd == L"attach") {
      OnAttach();
    } else {
      NOTREACHED();
      std::wstring msg =
          StringPrintf(L"before attach, ignored command (%S)", cmd.c_str());
      DebuggerOutput(msg);
    }
  } else if (cmd == L"attach") {
    OnAttach();
  } else if (cmd == L"quit" || cmd == L"detach") {
    OnDetach();
  } else {
    debugger_->Command(cmd);
  }
}

void DebugMessageHandler::OnDetach() {
  if (debugger_)
    debugger_->Detach();
  if (view_loop_ && view_)
    view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        view_, &RenderView::OnDebugDetach));
}

void DebugMessageHandler::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void DebugMessageHandler::OnFilterRemoved() {
  channel_ = NULL;
  view_loop_ = NULL;
  view_ = NULL;
  // By the time this is called, the view is not in a state where it can
  // receive messages from the MessageLoop, so we need to clear those first.
  OnDetach();
}

bool DebugMessageHandler::OnMessageReceived(const IPC::Message& message) {
  DCHECK(channel_ != NULL);

  // In theory, there could be multiple debuggers running (in practice this
  // hasn't been implemented yet), so make sure we only handle messages meant
  // for the view we were initialized for.
  if (message.routing_id() != view_routing_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DebugMessageHandler, message)
    IPC_MESSAGE_HANDLER(ViewMsg_SendToDebugger, OnSendToDebugger);
    // If the debugger is active, then it's possible that the renderer thread
    // is suspended handling a breakpoint.  In that case, the renderer will
    // hang forever and never exit.  To avoid this, we look for close messages
    // and tell the debugger to shutdown.
    IPC_MESSAGE_HANDLER_GENERIC(ViewMsg_Close,
      if (debugger_) OnDetach();
      handled = false;)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()
  return handled;
}
