// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_H__
#define CHROME_COMMON_CHILD_PROCESS_H__

#include <string>
#include <vector>
#include "base/atomic_ref_count.h"
#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/waitable_event.h"

class ChildThread;


// Base class for child processes of the browser process (i.e. renderer and
// plugin host). This is a singleton object for each child process.
class ChildProcess {
 public:
  // Child processes should have an object that derives from this class.  The
  // constructor will return once ChildThread has started.
  ChildProcess(ChildThread* child_thread);
  virtual ~ChildProcess();

  // Getter for this process' main thread.
  ChildThread* child_thread() { return child_thread_.get(); }

  // A global event object that is signalled when the main thread's message
  // loop exits.  This gives background threads a way to observe the main
  // thread shutting down.  This can be useful when a background thread is
  // waiting for some information from the browser process.  If the browser
  // process goes away prematurely, the background thread can at least notice
  // the child processes's main thread exiting to determine that it should give
  // up waiting.
  // For example, see the renderer code used to implement
  // webkit_glue::GetCookies.
  base::WaitableEvent* GetShutDownEvent();

  // These are used for ref-counting the child process.  The process shuts
  // itself down when the ref count reaches 0.  These functions may be called
  // on any thread.
  // For example, in the renderer process, generally each tab managed by this
  // process will hold a reference to the process, and release when closed.
  void AddRefProcess();
  void ReleaseProcess();

 protected:
  friend class ChildThread;

  // Getter for the one ChildProcess object for this process.
  static ChildProcess* current() { return child_process_; }

 protected:
  bool ProcessRefCountIsZero();

  // Derived classes can override this to alter the behavior when the ref count
  // reaches 0. The default implementation calls Quit on the main message loop
  // which causes the process to shutdown. Note, this can be called on any
  // thread. (See ReleaseProcess)
  virtual void OnFinalRelease();

 private:
  // The singleton instance for this process.
  static ChildProcess* child_process_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  base::AtomicRefCount ref_count_;

  scoped_ptr<ChildThread> child_thread_;

  DISALLOW_EVIL_CONSTRUCTORS(ChildProcess);
};

#endif  // CHROME_COMMON_CHILD_PROCESS_H__

