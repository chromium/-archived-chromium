// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include this file if you need to access the Debugger outside of the debugger
// project.  Don't include debugger.h directly.  If there's functionality from
// Debugger needed, add new wrapper methods to this file.
//
// This is a workaround to enable the Debugger without breaking the KJS build.
// It wraps all methods in Debugger which are called from outside of the
// debugger project.  Each solution has its own project with debugger files.
// KJS has only debugger_wrapper* and debugger.h, and defines
// CHROME_DEBUGGER_DISABLED, which makes it compile only a stub version of
// Debugger that doesn't reference V8.  Meanwhile the V8 solution includes all
// of the debugger files without CHROME_DEBUGGER_DISABLED so the full
// functionality is enabled.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_INTERFACE_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_INTERFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"

class DebuggerHost;

class DebuggerWrapper : public base::RefCountedThreadSafe<DebuggerWrapper> {
 public:
  DebuggerWrapper(int port);

  virtual ~DebuggerWrapper();

  void SetDebugger(DebuggerHost* debugger);
  DebuggerHost* GetDebugger();

  void DebugMessage(const std::wstring& msg);

  void OnDebugAttach();
  void OnDebugDisconnect();

 private:
  scoped_refptr<DebuggerHost> debugger_;
};

#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_INTERFACE_H_
