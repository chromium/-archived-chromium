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

#ifndef BASE_THREAD_H__
#define BASE_THREAD_H__

#include <wtypes.h>
#include <string>

#include "base/basictypes.h"
#include "base/thread_local_storage.h"

class MessageLoop;

// A simple thread abstraction that establishes a MessageLoop on a new thread.
// The consumer uses the MessageLoop of the thread to cause code to execute on
// the thread.  When this object is destroyed the thread is terminated.  All
// pending tasks queued on the thread's message loop will run to completion
// before the thread is terminated.
//
class Thread {
 public:
  // Constructor.
  // name is a display string to identify the thread.
  explicit Thread(const char *name);

  // Destroys the thread, stopping it if necessary.
  //
  virtual ~Thread();

  // Starts the thread.  Returns true if the thread was successfully started;
  // otherwise, returns false.  Upon successful return, the message_loop()
  // getter will return non-null.
  //
  bool Start();

  // Starts the thread. Behaves exactly like Start in addition to allow to
  // override the default process stack size. This is not the initial stack size
  // but the maximum stack size that thread is allowed to use.
  bool StartWithStackSize(size_t stack_size);

  // Signals the thread to exit and returns once the thread has exited.  After
  // this method returns, the Thread object is completely reset and may be used
  // as if it were newly constructed (i.e., Start may be called again).
  //
  // Stop may be called multiple times and is simply ignored if the thread is
  // already stopped.
  //
  // NOTE: This method is optional.  It is not strictly necessary to call this
  // method as the Thread's destructor will take care of stopping the thread if
  // necessary.
  //
  void Stop();

  // Signals the thread to exit and returns once the thread has exited.
  // Meanwhile, a message loop is run. After this method returns, the Thread
  // object is completely reset and may be used as if it were newly constructed
  // (i.e., Start may be called again).
  //
  // NonBlockingStop may be called multiple times and is simply ignored if the
  // thread is already stopped.
  //
  // NOTE: The behavior is the same as Stop() except that pending tasks are run.
  void NonBlockingStop();

  // Returns the message loop for this thread.  Use the MessageLoop's
  // Posttask methods to execute code on the thread.  This only returns
  // non-null after a successful call to Start.  After Stop has been called,
  // this will return NULL.
  //
  // NOTE: You must not call this MessageLoop's Quit method directly.  Use
  // the Thread's Stop method instead.
  //
  MessageLoop* message_loop() const { return message_loop_; }

  // Set the name of this thread (for display in debugger too).
  const std::string &thread_name() { return name_; }

  static void SetThreadWasQuitProperly(bool flag);
  static bool GetThreadWasQuitProperly();

  // Sets the thread name if a debugger is currently attached. Has no effect
  // otherwise. To set the name of the current thread, pass GetCurrentThreadId()
  // as the tid parameter.
  static void SetThreadName(const char* name, DWORD tid);

 protected:
  // Called just prior to starting the message loop
  virtual void Init() { }

  // Called just after the message loop ends
  virtual void CleanUp() { }

  // Stops the thread. Called by either Stop() or NonBlockingStop().
  void InternalStop(bool run_message_loop);

 private:
  static unsigned __stdcall ThreadFunc(void* param);

  HANDLE thread_;
  unsigned thread_id_;
  MessageLoop* message_loop_;
  std::string name_;
  static TLSSlot tls_index_;

  DISALLOW_EVIL_CONSTRUCTORS(Thread);
};

#endif  // BASE_THREAD_H__
