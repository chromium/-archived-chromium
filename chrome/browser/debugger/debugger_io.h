// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"

class DebuggerHost;
class ListValue;

class DebuggerInputOutput
    : public base::RefCountedThreadSafe<DebuggerInputOutput> {
public:
  DebuggerInputOutput() {}
  virtual ~DebuggerInputOutput() {}

  // Called when Debugger is ready to begin.
  virtual void Start(DebuggerHost* debugger) { debugger_ = debugger; }

  // Called when Debugger is shutting down
  virtual void Stop() {}

  // Outputs a string to the connection.
  virtual void Output(const std::string& out) = 0;
  virtual void Output(const std::wstring& out) = 0;
  virtual void OutputLine(const std::string& out) = 0;
  virtual void OutputLine(const std::wstring& out) = 0;
  virtual void OutputPrompt(const std::string& prompt) = 0;
  virtual void OutputPrompt(const std::wstring& prompt) = 0;

  // called by debugger debugger - ready is false when a command has just been
  // entered and true when a response to that command has been received
  virtual void SetDebuggerReady(bool ready) {}
  // called by debugger debugger - brk is false when the web page being debugged
  // is running, and true when the page is stopped at a breakpoint
  virtual void SetDebuggerBreak(bool brk) {}

  // sends message to debugger UI page in order to invoke JS function in it
  virtual void CallFunctionInPage(const std::wstring& name,
                                  ListValue* argv) {}

protected:
  DebuggerHost* debugger_;

private:
  DISALLOW_COPY_AND_ASSIGN(DebuggerInputOutput);
};

#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_H_
