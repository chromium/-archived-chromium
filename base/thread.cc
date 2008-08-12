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

#include <process.h>
#include <windows.h>

#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "base/win_util.h"

namespace {

// This class is used when starting a thread.  It passes information to the
// thread function.  It is referenced counted so we can cleanup the event
// object used to synchronize thread startup properly.
class ThreadStartInfo {
 public:
  Thread* self;
  base::WaitableEvent start_event;

  explicit ThreadStartInfo(Thread* t) : self(t), start_event(false, false) {
  }
};

}  // namespace

// This task is used to trigger the message loop to exit.
class ThreadQuitTask : public Task {
 public:
  virtual void Run() {
    MessageLoop::current()->Quit();
    Thread::SetThreadWasQuitProperly(true);
  }
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
void Thread::SetThreadName(const char* name, unsigned int tid) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = tid;
  info.dwFlags = 0;

  __try {
    RaiseException(MS_VC_EXCEPTION, 0,
                   sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR *>(&info));
  } __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
}


bool Thread::Start() {
  return StartWithStackSize(0);
}

bool Thread::StartWithStackSize(size_t stack_size) {
  DCHECK(!thread_id_ && !thread_);
  SetThreadWasQuitProperly(false);
  ThreadStartInfo info(this);

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
                     &info,
                     flags,
                     &thread_id_));
  if (!thread_) {
    DLOG(ERROR) << "failed to create thread";
    return false;
  }

  // Wait for the thread to start and initialize message_loop_
  info.start_event.Wait();
  return true;
}

void Thread::Stop() {
  if (!thread_)
    return;

  DCHECK_NE(thread_id_, GetCurrentThreadId()) << "Can't call Stop() on itself";

  if (message_loop())
    message_loop_->PostTask(FROM_HERE, new ThreadQuitTask());

  // Wait for the thread to exit. It should already have terminated but make
  // sure this assumption is valid.
  DWORD result = WaitForSingleObject(thread_, INFINITE);
  DCHECK_EQ(result, WAIT_OBJECT_0);

  // Reset state.
  CloseHandle(thread_);
  thread_ = NULL;
  thread_id_ = 0;
}

void Thread::StopSoon() {
  if (!thread_id_)
    return;

  DCHECK_NE(thread_id_, GetCurrentThreadId()) <<
      "Can't call StopSoon() on itself";

  // We had better have a message loop at this point!  If we do not, then it
  // most likely means that the thread terminated unexpectedly, probably due
  // to someone calling Quit() on our message loop directly.
  DCHECK(message_loop_);

  message_loop_->PostTask(FROM_HERE, new ThreadQuitTask());

  // The thread can't receive messages anymore.
  thread_id_ = 0;
}

/*static*/
unsigned __stdcall Thread::ThreadFunc(void* param) {
  // The message loop for this thread.
  MessageLoop message_loop;

  Thread* self;

  // Complete the initialization of our Thread object.
  {
    ThreadStartInfo* info = static_cast<ThreadStartInfo*>(param);
    self = info->self;
    self->message_loop_ = &message_loop;
    SetThreadName(self->thread_name().c_str(), GetCurrentThreadId());
    message_loop.SetThreadName(self->thread_name());
    info->start_event.Signal();
    // info can't be touched anymore since the starting thread is now unlocked.
  }

  // Let the thread do extra initialization.
  self->Init();

  message_loop.Run();

  // Let the thread do extra cleanup.
  self->CleanUp();

  // Assert that MessageLoop::Quit was called by ThreadQuitTask.
  DCHECK(Thread::GetThreadWasQuitProperly());

  // We can't receive messages anymore.
  self->message_loop_ = NULL;

  return 0;
}
