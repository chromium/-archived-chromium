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

#include "base/logging.h"

namespace base {

//-----------------------------------------------------------------------------

struct ObjectWatcher::Watch : public Task {
  ObjectWatcher* watcher;    // The associated ObjectWatcher instance
  HANDLE object;             // The object being watched
  HANDLE wait_object;        // Returned by RegisterWaitForSingleObject
  MessageLoop* origin_loop;  // Used to get back to the origin thread
  Delegate* delegate;        // Delegate to notify when signaled
  bool did_signal;           // DoneWaiting was called

  virtual void Run() {
    // The watcher may have already been torn down, in which case we need to
    // just get out of dodge.
    if (!watcher)
      return;
    
    DCHECK(did_signal);
    watcher->StopWatching();

    delegate->OnObjectSignaled(object);
  }
};

//-----------------------------------------------------------------------------

ObjectWatcher::ObjectWatcher() : watch_(NULL) {
}

ObjectWatcher::~ObjectWatcher() {
  StopWatching();
}

bool ObjectWatcher::StartWatching(HANDLE object, Delegate* delegate) {
  if (watch_) {
    NOTREACHED() << "Already watching an object";
    return false;
  }

  Watch* watch = new Watch;
  watch->watcher = this;
  watch->object = object;
  watch->origin_loop = MessageLoop::current();
  watch->delegate = delegate;
  watch->did_signal = false;

  // Since our job is to just notice when an object is signaled and report the
  // result back to this thread, we can just run on a Windows wait thread.
  DWORD wait_flags = WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE;

  if (!RegisterWaitForSingleObject(&watch->wait_object, object, DoneWaiting,
                                   watch, INFINITE, wait_flags)) {
    NOTREACHED() << "RegisterWaitForSingleObject failed: " << GetLastError();
    delete watch;
    return false;
  }

  watch_ = watch;

  // We need to know if the current message loop is going away so we can
  // prevent the wait thread from trying to access a dead message loop.
  MessageLoop::current()->AddDestructionObserver(this);
  return true;
}

bool ObjectWatcher::StopWatching() {
  if (!watch_)
    return false;

  // Make sure ObjectWatcher is used in a single-threaded fashion.
  DCHECK(watch_->origin_loop == MessageLoop::current());
  
  // If DoneWaiting is in progress, we wait for it to finish.  We know whether
  // DoneWaiting happened or not by inspecting the did_signal flag.
  if (!UnregisterWaitEx(watch_->wait_object, INVALID_HANDLE_VALUE)) {
    NOTREACHED() << "UnregisterWaitEx failed: " << GetLastError();
    return false;
  }

  // Make sure that we see any mutation to did_signal.  This should be a no-op
  // since we expect that UnregisterWaitEx resulted in a memory barrier, but
  // just to be sure, we're going to be explicit.
  MemoryBarrier();

  // If the watch has been posted, then we need to make sure it knows not to do
  // anything once it is run.
  watch_->watcher = NULL;

  // If DoneWaiting was called, then the watch would have been posted as a
  // task, and will therefore be deleted by the MessageLoop.  Otherwise, we
  // need to take care to delete it here.
  if (!watch_->did_signal)
    delete watch_;

  watch_ = NULL;

  MessageLoop::current()->RemoveDestructionObserver(this);
  return true;
}

// static
void CALLBACK ObjectWatcher::DoneWaiting(void* param, BOOLEAN timed_out) {
  DCHECK(!timed_out);

  Watch* watch = static_cast<Watch*>(param);

  // Record that we ran this function.
  watch->did_signal = true;

  // Make this run very soon on the target thread.  TODO(darin): This could
  // lead to starving other tasks on the origin thread.
  watch->set_priority(1000);

  // We rely on the locking in PostTask() to ensure that a memory barrier is
  // provided, which in turn ensures our change to did_signal can be observed
  // on the target thread.
  watch->origin_loop->PostTask(FROM_HERE, watch);
}

void ObjectWatcher::WillDestroyCurrentMessageLoop() {
  // Need to shutdown the watch so that we don't try to access the MessageLoop
  // after this point.
  StopWatching();
}

}  // namespace base
