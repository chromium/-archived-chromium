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

// WARNING: You should probably be using Thread (thread.h) instead.  Thread is
//          Chrome's message-loop based Thread abstraction, and if you are a
//          thread running in the browser, there will likely be assumptions
//          that your thread will have an associated message loop.
//
// This is a simple thread interface that backs to a native operating system
// thread.  You should use this only when you want a thread that does not have
// an associated MessageLoop.  Unittesting is the best example of this.
//
// The simplest interface to use is CallWrapperSimpleThread, which will create
// a new thread, and execute the CallWrapper in this new thread until it has
// completed, exiting the thread.  See call_wrapper.h for that interface.
//
// NOTE: You *MUST* call Join on the thread to clean up the underlying thread
// resources.  You are also responsible for destructing the SimpleThread object.
// It is invalid to destroy a SimpleThread while it is running, or without
// Start() having been called (and a thread never created).
//
// Thread Safety: A SimpleThread is not completely thread safe.  It is safe to
// access it from the creating thread or from the newly created thread.  This
// implies that the creator thread should be the thread that calls Join.
//
// Example:
//   CallWrapper* wrapper = NewMethodCallWrapper(obj, &Foo::Main);
//   scoped_ptr<SimpleThread> thread(new CallWrapperSimpleThread(wrapper));
//   thread->Start();
//   // Start will return after the Thread has been successfully started and
//   // initialized.  The newly created thread will invoke obj->Main, and run
//   // until it returns.  The CallWrapper will then delete itself.
//   thread->Join();  // Wait until the thread has exited.  You MUST Join!
//   // The SimpleThread object is still valid, however you may not call Join
//   // or Start again.  In this example the scoper will destroy the object.

#ifndef BASE_SIMPLE_THREAD_H_
#define BASE_SIMPLE_THREAD_H_

#include <string>

#include "base/basictypes.h"
#include "base/waitable_event.h"
#include "base/platform_thread.h"

class CallWrapper;

namespace base {

// This is the base SimpleThread.  You can derive from it and implement the
// virtual Run method, or you can use the CallWrapperSimpleThread interface.
class SimpleThread : public PlatformThread::Delegate {
 public:
  class Options {
   public:
    Options() : stack_size_(0) { }
    ~Options() { }

    // We use the standard compiler-supplied copy constructor.

    // A custom stack size, or 0 for the system default.
    void set_stack_size(size_t size) { stack_size_ = size; }
    size_t stack_size() const { return stack_size_; }
   private:
    size_t stack_size_;
  };

  // Create a SimpleThread.  |options| should be used to manage any specific
  // configuration involving the thread creation and management.
  // Every thread has a name, in the form of |name_prefix|/TID, for example
  // "my_thread/321".  The thread will not be created until Start() is called.
  SimpleThread(const Options& options, const std::string& name_prefix)
      : name_prefix_(name_prefix), name_(name_prefix_), options_(options),
        thread_(0), event_(true, false), tid_(0), joined_(false) { }

  SimpleThread()
      : name_prefix_("unnamed"), name_(name_prefix_),
        thread_(0), event_(true, false), tid_(0), joined_(false) { }

  virtual ~SimpleThread();

  virtual void Start();
  virtual void Join();

  // We follow the PlatformThread Delegate interface.
  virtual void ThreadMain();

  // Subclasses should override the Run method.
  virtual void Run() = 0;

  // Return the thread name prefix, or "unnamed" if none was supplied.
  std::string name_prefix() { return name_prefix_; }

  // Return the completed name including TID, only valid after Start().
  std::string name() { return name_; }

  // Return the thread id, only valid after Start().
  int tid() { return tid_; }

  // Return True if Start() has ever been called.
  bool HasBeenStarted() { return event_.IsSignaled(); }

  // Return True if Join() has evern been called.
  bool HasBeenJoined() { return joined_; }

 private:
  const std::string name_prefix_;
  std::string name_;
  const Options options_;
  PlatformThreadHandle thread_;  // PlatformThread handle, invalid after Join!
  WaitableEvent event_;          // Signaled if Start() was ever called.
  int tid_;                      // The backing thread's id.
  bool joined_;                  // True if Join has been called.
};

class CallWrapperSimpleThread : public SimpleThread {
 public:
  typedef SimpleThread::Options Options;

  explicit CallWrapperSimpleThread(CallWrapper* wrapper)
      : SimpleThread(), wrapper_(wrapper) { }

  CallWrapperSimpleThread(CallWrapper* wrapper,
                          const Options& options,
                          const std::string& name_prefix)
      : SimpleThread(options, name_prefix), wrapper_(wrapper) { }

  virtual ~CallWrapperSimpleThread();
  virtual void Run();
 private:
  CallWrapper* wrapper_;
};

}  // namespace base

#endif  // BASE_SIMPLE_THREAD_H_
