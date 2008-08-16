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

// A browser-side server debugger built with V8 providing a scriptable
// interface to a JavaScript debugger as well as browser automation.
// Supports multiple user interfaces including a command-line debugger
// accessible from a Chrome window or telnet.

// NOTE: DON'T include this file outside of the debugger project.  In order to
// build in the KJS solution, you should instead use debugger_wrapper.h.
// If DebuggerWraper doesn't expose the interface you need, extend it to do so.
// See comments in debugger_wrapper.h for more details.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_SHELL_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_SHELL_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"

#ifdef CHROME_DEBUGGER_DISABLED

#include "base/logging.h"

class DebuggerShell : public base::RefCountedThreadSafe<DebuggerShell> {
 public:
  DebuggerShell() {
    LOG(ERROR) << "Debugger not enabled for KJS";
  }
  virtual ~DebuggerShell() {}
  void Start() {}
  void DebugMessage(const std::wstring& msg) {}
  void OnDebugDisconnect() {}
};

#else

#include "debugger_io.h"
#include "debugger_node.h"
#include "v8/public/v8.h"

class DebuggerInputOutput;
class MessageLoop;
class TabContents;

class DebuggerShell : public base::RefCountedThreadSafe<DebuggerShell> {
 public:
  DebuggerShell(DebuggerInputOutput *io);
  virtual ~DebuggerShell();

  // call before other methods
  void Start();

  // Start debugging the specified tab
  void Debug(TabContents* tab);

  // A message from the V8 debugger in the renderer being debugged via
  // RenderViewHost
  void DebugMessage(const std::wstring& msg);

  // The renderer we're attached to is gone.
  void OnDebugDisconnect();

  // SocketInputOutput callback methods
  void DidConnect();
  void DidDisconnect();
  void ProcessCommand(const std::wstring& data);

  static v8::Handle<v8::Value> SetDebuggerReady(const v8::Arguments& args,
                                                DebuggerShell* debugger);
  static v8::Handle<v8::Value> SetDebuggerBreak(const v8::Arguments& args,
                                                DebuggerShell* debugger);

  // For C++ objects which are tied to JS objects (e.g. DebuggerNode),
  // we need to know when the underlying JS objects have been collected
  // so that we can clean up the C++ object as well.
  static void HandleWeakReference(v8::Persistent<v8::Object> obj, void* data);

  // populates str with the ascii string value of result
  static void ObjectToString(v8::Handle<v8::Value> result, std::string* str);
  static void ObjectToString(v8::Handle<v8::Value> result, std::wstring* str);

  DebuggerInputOutput* GetIo();

 private:
  void PrintObject(v8::Handle<v8::Value> result, bool crlf = true);
  void PrintLine(const std::wstring& out);
  void PrintString(const std::wstring& out);
  void PrintLine(const std::string& out);
  void PrintString(const std::string& out);
  void PrintPrompt();
  v8::Handle<v8::Value> CompileAndRun(const std::wstring& wstr,
                                      const std::string& filename = "");
  v8::Handle<v8::Value> CompileAndRun(const std::string& str,
                                      const std::string& filename = "");

  bool LoadFile(const std::wstring& file);
  void LoadUserConfig();

  // Log/error messages from V8
  static void DelegateMessageListener(v8::Handle<v8::Message> message,
                                      v8::Handle<v8::Value> data);

  void MessageListener(v8::Handle<v8::Message> message);

  // global shell() function designed to allow command-line processing by
  // javascript code rather than by this object.
  static v8::Handle<v8::Value> DelegateSubshell(const v8::Arguments& args);
  v8::Handle<v8::Value> Subshell(const v8::Arguments& args);
  v8::Handle<v8::Value> SubshellFunction(const char* func,
                                         int argc,
                                         v8::Handle<v8::Value>* argv);

  // print message to the debugger
  static v8::Handle<v8::Value> DelegatePrint(const v8::Arguments& args);
  v8::Handle<v8::Value> Print(const v8::Arguments& args);

  // load and execute javascript file
  static v8::Handle<v8::Value> DelegateSource(const v8::Arguments& args);

  v8::Persistent<v8::Context> v8_context_;
  v8::Persistent<v8::External> v8_this_;
  v8::Persistent<v8::Object> shell_;
  scoped_refptr<DebuggerInputOutput> io_;

  // If the debugger is ready to process another command or is busy.
  bool debugger_ready_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerShell);
};

#endif // else CHROME_DEBUGGER_DISABLED
#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_SHELL_H_
