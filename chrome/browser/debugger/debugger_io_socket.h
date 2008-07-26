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

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_SOCKET_H__
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_SOCKET_H__

#include "chrome/browser/debugger/debugger_io.h"
#include "net/base/telnet_server.h"

class DebuggerShell;

// Interaction with the underlying Socket object MUST happen in the IO thread.
// However, Debugger will call into this object from the main thread.  As a result
// we wind up having helper methods that we call with InvokeLater into the IO
// thread.

class DebuggerInputOutputSocket: public DebuggerInputOutput,
                              public ListenSocket::ListenSocketDelegate {
public:
  DebuggerInputOutputSocket(int port);
  virtual ~DebuggerInputOutputSocket();

  // SocketDelegate - called in IO thread by Socket
  virtual void DidAccept(ListenSocket* server, ListenSocket* connection);
  virtual void DidRead(ListenSocket* connection, const std::string& data);
  virtual void DidClose(ListenSocket* sock);

  // Overrides - called from the main thread by Debugger
  // these in turn call helper methods in the IO thread.
  virtual void Output(const std::wstring& out);
  virtual void OutputLine(const std::wstring& out);
  virtual void OutputPrompt(const std::wstring& prompt);
  virtual void Output(const std::string& out);
  virtual void OutputLine(const std::string& out);
  virtual void OutputPrompt(const std::string& prompt);
  virtual void Start(DebuggerShell* debugger);
  // Stop must be called prior to this object being released, so that cleanup
  // can happen in the IO thread.
  virtual void Stop();

private:

  // The following methods are called from the IO thread.

  // Creates a TelnetServer listing on 127:0.0.1:port_
  void StartListening();
  void StopListening();
  void OutputLater(const std::wstring& out, bool lf);
  void OutputLater(const std::string& out, bool lf);
  void OutputToSocket(const std::string& out, bool lf);

  scoped_refptr<ListenSocket> server_;
  scoped_refptr<ListenSocket> connection_;
  MessageLoop* ui_loop_;
  MessageLoop* io_loop_;
  int port_;

  DISALLOW_EVIL_CONSTRUCTORS(DebuggerInputOutputSocket);
};

#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_SOCKET_H__