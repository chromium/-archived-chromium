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

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"

class DebuggerShell;

class DebuggerInputOutput
    : public base::RefCountedThreadSafe<DebuggerInputOutput> {
public:
  DebuggerInputOutput() {}
  virtual ~DebuggerInputOutput() {}

  // Called when Debugger is ready to begin.
  virtual void Start(DebuggerShell* debugger) { debugger_ = debugger; }

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

protected:
  DebuggerShell* debugger_;

private:
  DISALLOW_COPY_AND_ASSIGN(DebuggerInputOutput);
};

#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_IO_H_