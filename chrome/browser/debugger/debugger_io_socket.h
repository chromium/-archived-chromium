// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_SOCKET_H__
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_SOCKET_H__

#include "chrome/browser/debugger/debugger_io.h"
#include "net/base/telnet_server.h"

class DebuggerHost;
class MessageLoop;

// Interaction with the underlying Socket object MUST happen in the IO thread.
// However, Debugger will call into this object from the main thread.  As a
// result we wind up having helper methods that we call with InvokeLater into
// the IO thread.

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
  virtual void Start(DebuggerHost* debugger);
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
