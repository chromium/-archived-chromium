// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_H__
#define CHROME_COMMON_CHILD_PROCESS_H__

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/message_loop.h"

namespace base {
  class WaitableEvent;
};

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
  static base::WaitableEvent* GetShutDownEvent();

  // You must call Init after creating this object before it will be valid
  ChildProcess();
  virtual ~ChildProcess();

 protected:
  static bool GlobalInit(const std::wstring& channel_name,
                         ChildProcessFactoryInterface* factory);

  static bool ProcessRefCountIsZero();

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
  static base::WaitableEvent* shutdown_event_;

  DISALLOW_EVIL_CONSTRUCTORS(ChildProcess);
};

#endif  // CHROME_COMMON_CHILD_PROCESS_H__

