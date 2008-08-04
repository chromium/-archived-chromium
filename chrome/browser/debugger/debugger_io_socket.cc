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

#include "chrome/browser/debugger/debugger_io_socket.h"

#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/common/resource_bundle.h"
#include "v8/public/v8.h"

////////////////////////////////////////////////

DebuggerInputOutputSocket::DebuggerInputOutputSocket(int port)
    : server_(0), connection_(0), port_(port) {
  ui_loop_ = MessageLoop::current();
  io_loop_ = g_browser_process->io_thread()->message_loop();
}

void DebuggerInputOutputSocket::Start(DebuggerShell* debugger) {
  DebuggerInputOutput::Start(debugger);
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DebuggerInputOutputSocket::StartListening));
}

void DebuggerInputOutputSocket::StartListening() {
  DCHECK(MessageLoop::current() == io_loop_);
  server_ = TelnetServer::Listen("127.0.0.1", port_, this);
}

DebuggerInputOutputSocket::~DebuggerInputOutputSocket() {
  // Stop() must be called prior to this being called
  DCHECK(connection_.get() == NULL);
  DCHECK(server_.get() == NULL);
}

void DebuggerInputOutputSocket::Stop() {
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DebuggerInputOutputSocket::StopListening));
}

void DebuggerInputOutputSocket::StopListening() {
  connection_ = NULL;
  server_ = NULL;
}

void DebuggerInputOutputSocket::DidAccept(ListenSocket *server,
                                 ListenSocket *connection) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (connection_ == NULL) {
    connection_ = connection;
    connection_->AddRef();
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        debugger_, &DebuggerShell::DidConnect));
  } else {
    delete connection;
  }
}

void DebuggerInputOutputSocket::Output(const std::wstring& out) {
  OutputLater(out, false);
}

void DebuggerInputOutputSocket::OutputLine(const std::wstring& out) {
  OutputLater(out, true);
}

void DebuggerInputOutputSocket::OutputPrompt(const std::wstring& prompt) {
  Output(prompt);
}

void DebuggerInputOutputSocket::Output(const std::string& out) {
  OutputLater(out, false);
}

void DebuggerInputOutputSocket::OutputLine(const std::string& out) {
  OutputLater(out, true);
}

void DebuggerInputOutputSocket::OutputPrompt(const std::string& prompt) {
  Output(prompt);
}

void DebuggerInputOutputSocket::OutputLater(const std::wstring& out, bool lf) {
  std::string utf8 = WideToUTF8(out);
  OutputLater(utf8, lf);
}

void DebuggerInputOutputSocket::OutputLater(const std::string& out, bool lf) {
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DebuggerInputOutputSocket::OutputToSocket, out, lf));
}

void DebuggerInputOutputSocket::OutputToSocket(const std::string& out, bool lf) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (connection_) {
    if (out.length()) {
      connection_->Send(out, lf);
    }
  } else {
    logging::LogMessage("CONSOLE", 0).stream() << "V8 debugger: " << out;
  }
}

void DebuggerInputOutputSocket::DidRead(ListenSocket *connection,
                                     const std::string& data) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (connection == connection_) {
    const std::wstring wstr = UTF8ToWide(data);
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        debugger_, &DebuggerShell::ProcessCommand, wstr));
  } else {
    // TODO(erikkay): assert?
  }
}

void DebuggerInputOutputSocket::DidClose(ListenSocket *sock) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (connection_ == sock) {
    connection_ = NULL;
    sock->Release();
  } else {
    // TODO(erikkay): assert?
  }
}

