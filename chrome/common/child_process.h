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

#ifndef CHROME_COMMON_CHILD_PROCESS_H__
#define CHROME_COMMON_CHILD_PROCESS_H__

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/message_loop.h"


class ChildProcess;

class ChildProcessFactoryInterface {
 public:
  virtual ChildProcess* Create(const std::wstring& channel_name) = 0;
};

template<class T>
class ChildProcessFactory : public ChildProcessFactoryInterface {
  virtual ChildProcess* Create(const std::wstring& channel_name) {
    return new T(channel_name);
  }
};

// Base class for child processes of the browser process (i.e. renderer and
// plugin host). This is a singleton object for each child process.
class ChildProcess {
 public:

  // initializes/cleansup the global variables, services, and libraries
  // Derived classes need to implement a static GlobalInit, that calls
  // into ChildProcess::GlobalInit with a class factory
//static bool GlobalInit(const std::wstring& channel_name);
  static void GlobalCleanup();

  // These are used for ref-counting the child process.  The process shuts
  // itself down when the ref count reaches 0.  These functions may be called
  // on any thread.
  // For example, in the renderer process, generally each tab managed by this
  // process will hold a reference to the process, and release when closed.
  static void AddRefProcess();
  static void ReleaseProcess();

  // A global event object that is signalled when the main thread's message
  // loop exits.  This gives background threads a way to observe the main
  // thread shutting down.  This can be useful when a background thread is
  // waiting for some information from the browser process.  If the browser
  // process goes away prematurely, the background thread can at least notice
  // the child processes's main thread exiting to determine that it should give
  // up waiting.
  // For example, see the renderer code used to implement
  // webkit_glue::GetCookies.
  static HANDLE GetShutDownEvent();

  // You must call Init after creating this object before it will be valid
  ChildProcess();
  virtual ~ChildProcess();

 protected:
  static bool GlobalInit(const std::wstring& channel_name,
                         ChildProcessFactoryInterface* factory);

  static int GetProcessRefcount() { return static_cast<int>(ref_count_);}

  // The singleton instance for this process.
  static ChildProcess* child_process_;

  static MessageLoop* main_thread_loop_;

  // Derived classes can override this to alter the behavior when the ref count
  // reaches 0. The default implementation calls Quit on the main message loop
  // which causes the process to shutdown. Note, this can be called on any
  // thread. (See ReleaseProcess)
  virtual void OnFinalRelease();

 private:
  // Derived classes can override this to handle any cleanup, called by
  // GlobalCleanup.
  virtual void Cleanup() {}

  static LONG ref_count_;
  static HANDLE shutdown_event_;

  DISALLOW_EVIL_CONSTRUCTORS(ChildProcess);
};

#endif  // CHROME_COMMON_CHILD_PROCESS_H__
