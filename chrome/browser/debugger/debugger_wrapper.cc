// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_wrapper.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/browser/debugger/debugger_io_socket.h"
#include "chrome/browser/debugger/debugger_host.h"
#include "chrome/browser/debugger/debugger_remote_service.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote_service.h"
#include "chrome/common/chrome_switches.h"

DebuggerWrapper::DebuggerWrapper(int port) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (port > 0) {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableOutOfProcessDevTools)) {
      DebuggerInputOutputSocket *io = new DebuggerInputOutputSocket(port);
      debugger_ = new DebuggerShell(io);
      debugger_->Start();
    } else {
      proto_handler_ = new DevToolsProtocolHandler(port);
      proto_handler_->RegisterDestination(
          new DevToolsRemoteService(proto_handler_),
          DevToolsRemoteService::kToolName);
      proto_handler_->RegisterDestination(
          new DebuggerRemoteService(proto_handler_),
          DebuggerRemoteService::kToolName);
      proto_handler_->Start();
    }
  }
#endif
}

DebuggerWrapper::~DebuggerWrapper() {
  if (proto_handler_.get() != NULL) {
    proto_handler_->Stop();
  }
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
