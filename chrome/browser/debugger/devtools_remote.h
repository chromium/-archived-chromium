// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"

class DevToolsRemoteMessage;

// This interface should be implemented by a class that wants to handle
// DevToolsRemoteMessages dispatched by some entity. It must extend
class DevToolsRemoteListener
    : public base::RefCountedThreadSafe<DevToolsRemoteListener> {
 public:
  DevToolsRemoteListener() {}
  virtual ~DevToolsRemoteListener() {}
  virtual void HandleMessage(const DevToolsRemoteMessage& message) = 0;
  // This method is invoked on the UI thread whenever the debugger connection
  // has been lost.
  virtual void OnConnectionLost() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsRemoteListener);
};

// Interface exposed by DevToolsProtocolHandler to receive reply messages
// from registered tools.
class OutboundSocketDelegate {
 public:
  virtual ~OutboundSocketDelegate() {}
  virtual void Send(const DevToolsRemoteMessage& message) = 0;
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_REMOTE_H_
