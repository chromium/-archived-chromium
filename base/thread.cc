// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/waitable_event.h"

// This task is used to trigger the message loop to exit.
class ThreadQuitTask : public Task {
 public:
  virtual void Run() {
    MessageLoop::current()->Quit();
    Thread::SetThreadWasQuitProperly(true);
  }
};

Thread::Thread(const char *name)
    : message_loop_(NULL),
      startup_event_(NULL),
      name_(name),
      thread_created_(false) {
}

Thread::~Thread() {
  Stop();
}

// We use this thread-local variable to record whether or not a thread exited
// because its Stop method was called.  This allows us to catch cases where
// MessageLoop::Quit() is called directly, which is unexpected when using a
// Thread to setup and run a MessageLoop.
// Note that if we start doing complex stuff in other static initializers
// this could cause problems.
// TODO(evanm): this shouldn't rely on static initialization.
TLSSlot Thread::tls_index_;

void Thread::SetThreadWasQuitProperly(bool flag) {
#ifndef NDEBUG
  tls_index_.Set(reinterpret_cast<void*>(flag));
#endif
}

bool Thread::GetThreadWasQuitProperly() {
  bool quit_properly = true;
#ifndef NDEBUG
  quit_properly = (tls_index_.Get() != 0);
#endif
  return quit_properly;
}

bool Thread::Start() {
  return StartWithStackSize(0);
}

bool Thread::StartWithStackSize(size_t stack_size) {
  DCHECK(!message_loop_);

  SetThreadWasQuitProperly(false);

  base::WaitableEvent event(false, false);
  startup_event_ = &event;

  if (!PlatformThread::Create(stack_size, this, &thread_)) {
    DLOG(ERROR) << "failed to create thread";
    return false;
  }

  // Wait for the thread to start and initialize message_loop_
  startup_event_->Wait();
  startup_event_ = NULL;

  DCHECK(message_loop_);
  return true;
}

void Thread::Stop() {
  if (!thread_created_)
    return;

  DCHECK_NE(thread_id_, PlatformThread::CurrentId()) <<
      "Can't call Stop() on the currently executing thread";

  // If StopSoon was called, then we won't have a message loop anymore, but
  // more importantly, we won't need to tell the thread to stop.
  if (message_loop_)
    message_loop_->PostTask(FROM_HERE, new ThreadQuitTask());

  // Wait for the thread to exit. It should already have terminated but make
  // sure this assumption is valid.
  PlatformThread::Join(thread_);

  // The thread can't receive messages anymore.
  message_loop_ = NULL;

  // The thread no longer needs to be joined.
  thread_created_ = false;
}

void Thread::StopSoon() {
  if (!message_loop_)
    return;

  DCHECK_NE(thread_id_, PlatformThread::CurrentId()) <<
      "Can't call StopSoon() on the currently executing thread";

  // We had better have a message loop at this point!  If we do not, then it
  // most likely means that the thread terminated unexpectedly, probably due
  // to someone calling Quit() on our message loop directly.
  DCHECK(message_loop_);

  message_loop_->PostTask(FROM_HERE, new ThreadQuitTask());

  // The thread can't receive messages anymore.
  message_loop_ = NULL;
}

void Thread::ThreadMain() {
  // The message loop for this thread.
  MessageLoop message_loop;

  // Complete the initialization of our Thread object.
  thread_id_ = PlatformThread::CurrentId();
  PlatformThread::SetName(name_.c_str());
  message_loop.set_thread_name(name_);
  message_loop_ = &message_loop;
  thread_created_ = true;

  startup_event_->Signal();
  // startup_event_ can't be touched anymore since the starting thread is now
  // unlocked.

  // Let the thread do extra initialization.
  Init();

  message_loop.Run();

  // Let the thread do extra cleanup.
  CleanUp();

  // Assert that MessageLoop::Quit was called by ThreadQuitTask.
  DCHECK(GetThreadWasQuitProperly());

  // We can't receive messages anymore.
  message_loop_ = NULL;
}

