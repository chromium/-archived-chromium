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

// Include this file if you need to access the Debugger outside of the debugger
// project.  Don't include debugger.h directly.  If there's functionality from
// Debugger needed, add new wrapper methods to this file.
//
// This is a workaround to enable the Debugger without breaking the KJS build.
// It wraps all methods in Debugger which are called from outside of the debugger
// project.  Each solution has its own project with debugger files.  KJS has only
// debugger_wrapper* and debugger.h, and defines CHROME_DEBUGGER_DISABLED, which makes
// it compile only a stub version of Debugger that doesn't reference V8.  Meanwhile
// the V8 solution includes all of the debugger files without CHROME_DEBUGGER_DISABLED
// so the full functionality is enabled.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_INTERFACE_H__
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_INTERFACE_H__

#include "base/basictypes.h"
#include "base/ref_counted.h"

class DebuggerShell;

class DebuggerWrapper : public base::RefCountedThreadSafe<DebuggerWrapper> {
 public:
  DebuggerWrapper(int port);

  virtual ~DebuggerWrapper();

  void SetDebugger(DebuggerShell* debugger);
  DebuggerShell* GetDebugger();

  void DebugMessage(const std::wstring& msg);

  void OnDebugDisconnect();

 private:
  scoped_refptr<DebuggerShell> debugger_;
};


#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_INTERFACE_H__