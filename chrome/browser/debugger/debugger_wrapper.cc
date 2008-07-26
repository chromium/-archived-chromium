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

void DebuggerWrapper::OnDebugDisconnect() {
  if (debugger_.get())
    debugger_->OnDebugDisconnect();
}