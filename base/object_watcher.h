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

#include <map>

#include "base/linked_ptr.h"
#include "base/tracked.h"

class Task;

namespace base {

// A class that enables support for asynchronously waiting for Windows objects
// to become signaled.  Supports waiting on more than 64 objects.
class ObjectWatcher {
 public:
  ObjectWatcher();
  ~ObjectWatcher();

  // When the object is signaled, the given task is run on the thread where
  // Watch is called.  The ObjectWatcher assumes ownership of the task and
  // will ensure that it gets deleted eventually.
  //
  // NOTE: It is an error to call this method on an object that is already
  // being watched by this ObjectWatcher.
  //
  // Returns true if the watch was added.  Otherwise, false is returned.
  //
  bool AddWatch(const tracked_objects::Location& from_here, HANDLE object,
                Task* task);

  // Stops watching the given object.  Does nothing if the watch has already
  // completed.  If the watch is still active, then it is canceled, and the
  // associated task is deleted.
  //
  // Returns true if the watch was canceled.  Otherwise, false is returned.
  //
  bool CancelWatch(HANDLE object);

 private:
  // Called on a background thread when done waiting.
  static void CALLBACK DoneWaiting(void* param, BOOLEAN timed_out);

  // Passed as the param argument to the above methods.
  struct Watch;

  typedef std::map<HANDLE, linked_ptr<Watch>> WatchMap;
  WatchMap watches_;
};

}  // namespace base

#endif  // BASE_OBJECT_WATCHER_H_
