// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_io_socket.h"

#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "v8/include/v8.h"

////////////////////////////////////////////////

DebuggerInputOutputSocket::DebuggerInputOutputSocket(int port)
    : server_(0), connection_(0), port_(port) {
  ui_loop_ = MessageLoop::current();
  io_loop_ = g_browser_process->io_thread()->message_loop();
}

void DebuggerInputOutputSocket::Start(DebuggerHost* debugger) {
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
        debugger_, &DebuggerHost::DidConnect));
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

void DebuggerInputOutputSocket::OutputToSocket(const std::string& out,
                                               bool lf) {
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
        debugger_, &DebuggerHost::ProcessCommand, wstr));
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


