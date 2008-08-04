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

#ifndef BASE_OBJECT_WATCHER_H_
#define BASE_OBJECT_WATCHER_H_

#include <windows.h>

#include "base/basictypes.h"

namespace base {

// A class that provides a means to asynchronously wait for a Windows object to
// become signaled.  It is an abstraction around RegisterWaitForSingleObject
// that provides a notification callback, OnObjectSignaled, that runs back on
// the origin thread (i.e., the thread that called StartWatching).
//
// This class acts like a smart pointer such that when it goes out-of-scope,
// UnregisterWaitEx is automatically called, and any in-flight notification is
// suppressed.
//
// Typical usage:
//
//   class MyClass : public base::ObjectWatcher::Delegate {
//    public:
//     void DoStuffWhenSignaled(HANDLE object) {
//       watcher_.StartWatching(object, this);
//     }
//     virtual void OnObjectSignaled(HANDLE object) {
//       // OK, time to do stuff!
//     }
//    private:
//     base::ObjectWatcher watcher_;
//   };
//
// In the above example, MyClass wants to "do stuff" when object becomes
// signaled.  ObjectWatcher makes this task easy.  When MyClass goes out of
// scope, the watcher_ will be destroyed, and there is no need to worry about
// OnObjectSignaled being called on a deleted MyClass pointer.  Easy!
//
class ObjectWatcher {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Called from the MessageLoop when a signaled object is detected.  To
    // continue watching the object, AddWatch must be called again.
    virtual void OnObjectSignaled(HANDLE object) = 0;
  };

  ObjectWatcher();
  ~ObjectWatcher();

  // When the object is signaled, the given delegate is notified on the thread
  // where StartWatching is called.  The ObjectWatcher is not responsible for
  // deleting the delegate.
  //
  // Returns true if the watch was started.  Otherwise, false is returned.
  //
  bool StartWatching(HANDLE object, Delegate* delegate);

  // Stops watching.  Does nothing if the watch has already completed.  If the
  // watch is still active, then it is canceled, and the associated delegate is
  // not notified.
  //
  // Returns true if the watch was canceled.  Otherwise, false is returned.
  //
  bool StopWatching();

 private:
  // Called on a background thread when done waiting.
  static void CALLBACK DoneWaiting(void* param, BOOLEAN timed_out);

  // Internal state.
  struct Watch;
  Watch* watch_;

  DISALLOW_COPY_AND_ASSIGN(ObjectWatcher);
}; 
}  // namespace base

#endif  // BASE_OBJECT_WATCHER_H_
