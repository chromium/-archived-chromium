// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_PROTOCOL_HANDLER_H_

#include <string>

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/debugger/devtools_remote.h"
#include "net/base/listen_socket.h"

class InspectableTabProxy;
class DevToolsRemoteListenSocket;
class DevToolsRemoteMessage;

// Dispatches DevToolsRemoteMessages to their appropriate handlers (Tools)
// based on the "Tool" message header value.
class DevToolsProtocolHandler
    : public DevToolsRemoteListener,
      public OutboundSocketDelegate,
      public ListenSocket::ListenSocketDelegate {
 public:
  typedef base::hash_map< std::string, scoped_refptr<DevToolsRemoteListener> >
      ToolToListenerMap;

  explicit DevToolsProtocolHandler(int port);
  virtual ~DevToolsProtocolHandler();

  // This method should be called after the object construction.
  void Start();

  // This method should be called before the object destruction.
  void Stop();

  // Registers a |listener| to handle messages for a certain |tool_name| Tool.
  // |listener| is the new message handler to register.
  //     As DevToolsRemoteListener inherits base::RefCountedThreadSafe,
  //     you should have no problems with ownership and destruction.
  // |tool_name| is the name of the Tool to associate the listener with.
  void RegisterDestination(DevToolsRemoteListener* listener,
                           const std::string& tool_name);

  // Unregisters a |listener| so that it will no longer handle messages
  // directed to the specified |tool_name| tool.
  void UnregisterDestination(DevToolsRemoteListener* listener,
                             const std::string& tool_name);

  InspectableTabProxy* inspectable_tab_proxy() {
    return inspectable_tab_proxy_.get();
  }

  // DevToolsRemoteListener interface
  virtual void HandleMessage(const DevToolsRemoteMessage& message);
  virtual void OnConnectionLost() {}

  // OutboundSocketDelegate interface
  virtual void Send(const DevToolsRemoteMessage& message);

  // ListenSocket::ListenSocketDelegate interface
  virtual void DidAccept(ListenSocket *server, ListenSocket *connection);
  virtual void DidRead(ListenSocket *connection, const std::string& data);
  virtual void DidClose(ListenSocket *sock);

 private:
  void Init();
  void Teardown();
  int port_;
  MessageLoop* ui_loop_;
  MessageLoop* io_loop_;
  ToolToListenerMap tool_to_listener_map_;
  scoped_refptr<ListenSocket> connection_;
  scoped_refptr<DevToolsRemoteListenSocket> server_;
  scoped_ptr<InspectableTabProxy> inspectable_tab_proxy_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsProtocolHandler);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_PROTOCOL_HANDLER_H_
