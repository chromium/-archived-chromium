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

#ifndef CHROME_BROWSER_CHROME_THREAD_H__
#define CHROME_BROWSER_CHROME_THREAD_H__

#include "base/lock.h"
#include "base/logging.h"
#include "base/thread.h"

///////////////////////////////////////////////////////////////////////////////
// ChromeThread
//
// This class represents a thread that is known by a browser-wide name.  For
// example, there is one IO thread for the entire browser process, and various
// pieces of code find it useful to retrieve a pointer to the IO thread's
// MessageLoop by name:
//
//   MessageLoop* io_loop = ChromeThread::GetMessageLoop(ChromeThread::IO);
//
// On the UI thread, it is often preferable to obtain a pointer to a well-known
// thread via the g_browser_process object, e.g. g_browser_process->io_thread();
//
// Code that runs on a thread other than the UI thread must take extra care in
// handling pointers to threads because many of the well-known threads are owned
// by the UI thread and can be deallocated without notice.
//
class ChromeThread : public Thread {
 public:
  // An enumeration of the well-known threads.
  enum ID {
    // This is the thread that processes IPC and network messages.
    IO,

    // This is the thread that interacts with the file system.
    FILE,

    // This is the thread that interacts with the database.
    DB,

    // This is the thread that interacts with the history database.
    HISTORY,

    // This identifier does not represent a thread.  Instead it counts the
    // number of well-known threads.  Insert new well-known threads before this
    // identifier.
    ID_COUNT
  };

  // Construct a ChromeThread with the supplied identifier.  It is an error
  // to construct a ChromeThread that already exists.
  explicit ChromeThread(ID identifier);
  virtual ~ChromeThread();

  // Callable on any thread, this helper function returns a pointer to the
  // thread's MessageLoop.
  //
  // WARNING:
  //   Nothing in this class prevents the MessageLoop object returned from this
  //   function from being destroyed on another thread.  Use with care.
  //
  static MessageLoop* GetMessageLoop(ID identifier);

 private:
  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;

  // This lock protects |chrome_threads_|.  Do not read or modify that array
  // without holding this lock.  Do not block while holding this lock.
  static Lock lock_;

  // An array of the ChromeThread objects.  This array is protected by |lock_|.
  // The threads are not owned by this array.  Typically, the threads are owned
  // on the IU thread by the g_browser_process object.  ChromeThreads remove
  // themselves from this array upon destruction.
  static ChromeThread* chrome_threads_[ID_COUNT];
};

#endif  // #ifndef CHROME_BROWSER_CHROME_THREAD_H__
