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

#include "base/thread.h"

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/string_util.h"

// This task is used to trigger the message loop to exit.
class ThreadQuitTask : public Task {
 public:
  virtual void Run() {
    MessageLoop::current()->Quit();
#ifndef NDEBUG
    Thread::SetThreadWasQuitProperly(true);
#endif
  }
};

Thread::Thread(const char *name)
    : thread_id_(0),
      message_loop_(NULL),
      name_(name) {
  DCHECK(tls_index_) << "static initializer failed";
}

Thread::~Thread() {
  Stop();
}

void* ThreadFunc(void* closure) {
  // TODO(pinkerton): I have no clue what to do here
  
  pthread_exit(NULL);
}

// We use this thread-local variable to record whether or not a thread exited
// because its Stop method was called.  This allows us to catch cases where
// MessageLoop::Quit() is called directly, which is unexpected when using a
// Thread to setup and run a MessageLoop.
// Note that if we start doing complex stuff in other static initializers
// this could cause problems.
TLSSlot Thread::tls_index_ = ThreadLocalStorage::Alloc();

void Thread::SetThreadWasQuitProperly(bool flag) {
  ThreadLocalStorage::Set(tls_index_, reinterpret_cast<void*>(flag));
}

bool Thread::GetThreadWasQuitProperly() {
  return (ThreadLocalStorage::Get(tls_index_) != 0);
}


bool Thread::Start() {
  return StartWithStackSize(0);
}

bool Thread::StartWithStackSize(size_t stack_size) {
  bool success = false;
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  
  // A stack size smaller than PTHREAD_STACK_MIN won't change the default value.
  pthread_attr_setstacksize(&attributes, stack_size);
  int result = pthread_create(&thread_id_,
                              &attributes,
                              ThreadFunc,
                              &message_loop_);
  if (!result)
    success = true;
  
  pthread_attr_destroy(&attributes);
  return success;
}

void Thread::Stop() {
//  DCHECK_NE(thread_id_, GetCurrentThreadId())
//             << "Can't call Stop() on itself";

  // We had better have a message loop at this point!  If we do not, then it
  // most likely means that the thread terminated unexpectedly, probably due
  // to someone calling Quit() on our message loop directly.
  DCHECK(message_loop_);

  message_loop_->PostTask(FROM_HERE, new ThreadQuitTask());

  message_loop_ = NULL;
}

void Thread::StopSoon() {
  // TODO(paulg): Make Thread::Stop block on thread join, and Thread::StopSoon
  // return immediately after calling (like the Windows versions).
  Stop();
}
