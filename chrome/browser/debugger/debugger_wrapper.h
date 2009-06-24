// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include this file if you need to access the Debugger outside of the debugger
// project.  Don't include debugger.h directly.  If there's functionality from
// Debugger needed, add new wrapper methods to this file.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_WRAPPER_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_WRAPPER_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"

class DebuggerHost;
class DevToolsProtocolHandler;
class DevToolsRemoteListenSocket;

class DebuggerWrapper : public base::RefCountedThreadSafe<DebuggerWrapper> {
 public:
  explicit DebuggerWrapper(int port);

  virtual ~DebuggerWrapper();

 private:
  scoped_refptr<DevToolsProtocolHandler> proto_handler_;
};

#endif  // CHROME_BROWSER_DEBUGGER_DEBUGGER_WRAPPER_H_
