// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An interface to the V8 debugger.  This is in WebKit in order to isolate
// the renderer from a direct V8 dependency.

#ifndef WEBKIT_GLUE_DEBUGGER_BRIDGE_H_
#define WEBKIT_GLUE_DEBUGGER_BRIDGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "v8/include/v8-debug.h"

void V8DebugMessageHandler(const uint16_t* message, int length, void* data);

class DebuggerBridge : public base::RefCountedThreadSafe<DebuggerBridge> {
 public:
  class Delegate {
   public:
    virtual void DebuggerOutput(const std::wstring& data) = 0;
  };
  // When constructed, the underlying V8 debugger is initialized and connected
  // to the internals of this object.  The provided delegate received output
  // from the debugger.  This output may be spontaneous (error messages,
  // exceptions, etc.) or the output from a command.
  // NOTE: the delegate will be called from another thread
  DebuggerBridge(Delegate* del);
  virtual ~DebuggerBridge();

  // Break V8 execution.
  void Break(bool force);

  // Sends a command to the debugger (same as v8 command-line debugger).
  // Results from the command come asynchronously.
  // TODO(erikkay): link command output to a particular command
  void Command(const std::wstring& cmd);

  // Attach and detach from the V8 debug message handler.
  void Attach();
  void Detach();

 private:
  friend void V8DebugMessageHandler(const uint16_t* message,
                                    int length, void* data);

  // Called by the LocalDebugSession so that the delegate can called in the
  // appropriate thread.
  void OutputLater(const std::wstring& cmd);
  // Calls the delegate.  This method is called in the delegate's thread.
  void Output(const std::wstring& out);

  Delegate* delegate_;
  MessageLoop* delegate_loop_;
  bool attached_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerBridge);
};

#endif  // WEBKIT_GLUE_DEBUGGER_BRIDGE_H_
