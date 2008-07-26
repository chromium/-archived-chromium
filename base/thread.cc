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

#include <process.h>
#include <windows.h>

#include "base/thread.h"

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/win_util.h"

// This class is used when starting a thread.  It passes information to the
// thread function.  It is referenced counted so we can cleanup the event
// object used to synchronize thread startup properly.
class ThreadStartInfo : public base::RefCountedThreadSafe<ThreadStartInfo> {
 public:
  Thread* self;
  HANDLE start_event;

  explicit ThreadStartInfo(Thread* t)
      : self(t),
        start_event(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  }

  ~ThreadStartInfo() {
    CloseHandle(start_event);
  }
};

// This task is used to trigger the message loop to exit.
class ThreadQuitTask : public Task {
 public:
  virtual void Run() {
    MessageLoop::current()->Quit();
    Thread::SetThreadWasQuitProperly(true);
  }
};

// Once an object is signaled, quits the current inner message loop.
class QuitOnSignal : public MessageLoop::Watcher {
 public:
  explicit QuitOnSignal(HANDLE signal) : signal_(signal) {
  }
  virtual void OnObjectSignaled(HANDLE object) {
    DCHECK_EQ(object, signal_);
    MessageLoop::current()->WatchObject(signal_, NULL);
    MessageLoop::current()->Quit();
  }
 private:
  HANDLE signal_;
  DISALLOW_EVIL_CONSTRUCTORS(QuitOnSignal);
};

Thread::Thread(const char *name)
    : thread_(NULL),
      thread_id_(0),
      message_loop_(NULL),
      name_(name) {
  DCHECK(tls_index_) << "static initializer failed";
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
TLSSlot Thread::tls_index_ = ThreadLocalStorage::Alloc();

void Thread::SetThreadWasQuitProperly(bool flag) {
#ifndef NDEBUG
  ThreadLocalStorage::Set(tls_index_, reinterpret_cast<void*>(flag));
#endif
}

bool Thread::GetThreadWasQuitProperly() {
  bool quit_properly = true;
#ifndef NDEBUG
  quit_properly = (ThreadLocalStorage::Get(tls_index_) != 0);
#endif
  return quit_properly;
}

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
#define MS_VC_EXCEPTION 0x406D1388

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;  // Must be 0x1000.
  LPCSTR szName;  // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;  // Reserved for future use, must be zero.
} THREADNAME_INFO;


// On XP, you can only get the ThreadId of the current
// thread.  So it is expected that you'll call this after the
// thread starts up; hence, it is static.
void Thread::SetThreadName(const char* name, DWORD tid) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = tid;
  info.dwFlags = 0;

  __try {
    RaiseException(MS_VC_EXCEPTION, 0,
                   sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD*>(&info));
  } __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
}


bool Thread::Start() {
  return StartWithStackSize(0);
}

bool Thread::StartWithStackSize(size_t stack_size) {
  DCHECK(!thread_id_ && !thread_);
  SetThreadWasQuitProperly(false);
  scoped_refptr<ThreadStartInfo> info = new ThreadStartInfo(this);

  unsigned int flags = 0;
  if (win_util::GetWinVersion() >= win_util::WINVERSION_XP && stack_size) {
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
  } else {
    stack_size = 0;
  }
  thread_ = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL,
                     static_cast<unsigned int>(stack_size),
                     ThreadFunc,
                     info,
                     flags,
                     &thread_id_));
  if (!thread_) {
    DLOG(ERROR) << "failed to create thread";
    return false;
  }

  // Wait for the thread to start and initialize message_loop_
  WaitForSingleObject(info->start_event, INFINITE);
  return true;
}

void Thread::Stop() {
  InternalStop(false);
}

void Thread::NonBlockingStop() {
  InternalStop(true);
}

void Thread::InternalStop(bool run_message_loop) {
  if (!thread_)
    return;

  DCHECK_NE(thread_id_, GetCurrentThreadId()) << "Can't call Stop() on itself";

  // We had better have a message loop at this point!  If we do not, then it
  // most likely means that the thread terminated unexpectedly, probably due
  // to someone calling Quit() on our message loop directly.
  DCHECK(message_loop_);

  message_loop_->PostTask(FROM_HERE, new ThreadQuitTask());

  if (run_message_loop) {
    QuitOnSignal signal_watcher(thread_);
    MessageLoop::current()->WatchObject(thread_, &signal_watcher);
    bool old_state = MessageLoop::current()->NestableTasksAllowed();
    MessageLoop::current()->SetNestableTasksAllowed(true);
    MessageLoop::current()->Run();
    // Restore task state.
    MessageLoop::current()->SetNestableTasksAllowed(old_state);
  } else {
    // Wait for the thread to exit.
    WaitForSingleObject(thread_, INFINITE);
  }
  CloseHandle(thread_);

  // Reset state.
  thread_ = NULL;
  message_loop_ = NULL;
}

/*static*/
unsigned __stdcall Thread::ThreadFunc(void* param) {
  // The message loop for this thread.
  MessageLoop message_loop;

  Thread* self;

  // Complete the initialization of our Thread object.  We grab an owning
  // reference to the ThreadStartInfo object to ensure that the start_event
  // object is not prematurely closed.
  {
    scoped_refptr<ThreadStartInfo> info = static_cast<ThreadStartInfo*>(param);
    self = info->self;
    self->message_loop_ = &message_loop;
    SetThreadName(self->thread_name().c_str(), GetCurrentThreadId());
    message_loop.SetThreadName(self->thread_name());
    SetEvent(info->start_event);
  }

  // Let the thread do extra initialization.
  self->Init();

  message_loop.Run();

  // Let the thread do extra cleanup.
  self->CleanUp();

  // Assert that MessageLoop::Quit was called by ThreadQuitTask.
  DCHECK(Thread::GetThreadWasQuitProperly());

  // Clearing this here should be unnecessary since we should only be here if
  // Thread::Stop was called (the above DCHECK asserts that).  However, to help
  // make it easier to track down problems in release builds, we null out the
  // message_loop_ pointer.
  self->message_loop_ = NULL;

  // We can't receive messages anymore.
  self->thread_id_ = 0;
  return 0;
}
