// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A part of browser-side server debugger exposed to DebuggerWrapper.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_HOST_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_HOST_H_

#include "base/basictypes.h"
#include "base/thread.h"

class ListValue;

class DebuggerHost : public base::RefCountedThreadSafe<DebuggerHost> {
 public:
  DebuggerHost() {};
  virtual ~DebuggerHost() {};

  // call before other methods
  virtual void Start() = 0;

  // A message from the V8 debugger in the renderer being debugged via
  // RenderViewHost
  virtual void DebugMessage(const std::wstring& msg) = 0;
  // We've been successfully attached to a renderer.
  virtual void OnDebugAttach() = 0;
  // The renderer we're attached to is gone.
  virtual void OnDebugDisconnect() = 0;

  virtual void DidDisconnect() = 0;
  virtual void DidConnect() {}
  virtual void ProcessCommand(const std::wstring& data) {}

  // Handles messages from debugger UI.
  virtual void OnDebuggerHostMsg(const ListValue* args) {}
 private:

  DISALLOW_COPY_AND_ASSIGN(DebuggerHost);
};

#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_HOST_H_
