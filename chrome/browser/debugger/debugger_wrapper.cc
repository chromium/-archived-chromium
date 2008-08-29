// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugger_wrapper.h"
#include "debugger_shell.h"
#include "debugger_io_socket.h"

DebuggerWrapper::DebuggerWrapper(int port) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (port > 0) {
    DebuggerInputOutputSocket *io = new DebuggerInputOutputSocket(port);
    debugger_ = new DebuggerShell(io);
    debugger_->Start();
  }
#endif
}

DebuggerWrapper::~DebuggerWrapper() {
}

void DebuggerWrapper::SetDebugger(DebuggerShell* debugger) {
  debugger_ = debugger;
}

DebuggerShell* DebuggerWrapper::GetDebugger() {
  return debugger_.get();
}

void DebuggerWrapper::DebugMessage(const std::wstring& msg) {
  if (debugger_.get())
    debugger_->DebugMessage(msg);
}

void DebuggerWrapper::OnDebugAttach() {
  if (debugger_.get())
    debugger_->OnDebugAttach();
}

void DebuggerWrapper::OnDebugDisconnect() {
  if (debugger_.get())
    debugger_->OnDebugDisconnect();
}
