// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/browser/debugger/debugger_io_socket.h"
#include "chrome/browser/debugger/debugger_host.h"

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

void DebuggerWrapper::SetDebugger(DebuggerHost* debugger) {
  debugger_ = debugger;
}

DebuggerHost* DebuggerWrapper::GetDebugger() {
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
