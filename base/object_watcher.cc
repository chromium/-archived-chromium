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

#include "base/object_watcher.h"

#include "base/message_loop.h"
#include "base/logging.h"

namespace base {

//-----------------------------------------------------------------------------

struct ObjectWatcher::Watch : public Task {
  ObjectWatcher* watcher;    // The associated ObjectWatcher instance
  HANDLE object;             // The object being watched
  HANDLE wait_object;        // Returned by RegisterWaitForSingleObject
  MessageLoop* origin_loop;  // Used to get back to the origin thread
  scoped_ptr<Task> task;     // Task to notify when signaled
  bool did_signal;           // DoneWaiting was called

  virtual void Run() {
    // The watcher may have already been torn down, in which case we need to
    // just get out of dodge.
    if (!watcher)
      return;
    
    // Put this on the stack since CancelWatch deletes task.  It is a good
    // to call CancelWatch before running the task because we want to allow
    // the consumer to call AddWatch again inside Run.
    Task* task_to_run = task.release();
    
    watcher->CancelWatch(object);

    task_to_run->Run();
    delete task_to_run;
  }
};

//-----------------------------------------------------------------------------

ObjectWatcher::ObjectWatcher() {
}

ObjectWatcher::~ObjectWatcher() {
  // Cancel any watches that may still exist.
  while (!watches_.empty())
    CancelWatch(watches_.begin()->first);
}

bool ObjectWatcher::AddWatch(const tracked_objects::Location& from_here,
                             HANDLE object, Task* task) {
  task->SetBirthPlace(from_here);

  linked_ptr<Watch>& watch = watches_[object];
  CHECK(!watch.get()) << "Already watched!";

  watch.reset(new Watch());
  watch->watcher = this;
  watch->object = object;
  watch->origin_loop = MessageLoop::current();
  watch->task.reset(task);
  watch->did_signal = false;

  // Since our job is to just notice when an object is signaled and report the
  // result back to this thread, we can just run on a Windows wait thread.
  DWORD wait_flags = WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE;

  if (!RegisterWaitForSingleObject(&watch->wait_object, object, DoneWaiting,
                                   watch.get(), INFINITE, wait_flags)) {
    NOTREACHED() << "RegisterWaitForSingleObject failed: " << GetLastError();
    watches_.erase(object);
    return false;
  }

  return true;
}

bool ObjectWatcher::CancelWatch(HANDLE object) {
  WatchMap::iterator i = watches_.find(object);
  if (i == watches_.end())
    return false;

  Watch* watch = i->second.get();
  
  // If DoneWaiting is in progress, we wait for it to finish.  We know whether
  // DoneWaiting happened or not by inspecting the did_signal flag.
  if (!UnregisterWaitEx(watch->wait_object, INVALID_HANDLE_VALUE)) {
    NOTREACHED() << "UnregisterWaitEx failed: " << GetLastError();
    return false;
  }

  // If DoneWaiting was called, then the watch would have been posted as a
  // task, and will therefore be deleted by the MessageLoop.  Otherwise, we
  // need to take care to delete it here.
  if (watch->did_signal)
    i->second.release();

  // If the watch has been posted, then we need to make sure it knows not to do
  // anything once it is run.
  watch->watcher = NULL;

  // Delete the task object now so that everything, from the perspective of the
  // consumer, is cleaned up once we return from CancelWatch.
  watch->task.reset();

  watches_.erase(i);
  return true;
}

// static
void CALLBACK ObjectWatcher::DoneWaiting(void* param, BOOLEAN timed_out) {
  DCHECK(!timed_out);

  Watch* watch = static_cast<Watch*>(param);

  // Record that we ran this function.
  watch->did_signal = true;

  watch->origin_loop->PostTask(FROM_HERE, watch);
}

}  // namespace base
