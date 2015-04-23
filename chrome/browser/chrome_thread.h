// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_THREAD_H__
#define CHROME_BROWSER_CHROME_THREAD_H__

#include "base/lock.h"
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
class ChromeThread : public base::Thread {
 public:
  // An enumeration of the well-known threads.
  enum ID {
    // This is the thread that processes IPC and network messages.
    IO,

    // This is the thread that interacts with the file system.
    FILE,

    // This is the thread that interacts with the database.
    DB,

    // This is the "main" thread for WebKit within the browser process when
    // NOT in --single-process mode.
    WEBKIT,

    // This is the thread that interacts with the history database.
    HISTORY,

#if defined(OS_LINUX)
    // This thread has a second connection to the X server and is used to
    // process UI requests when routing the request to the UI thread would risk
    // deadlock.
    BACKGROUND_X11,
#endif

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

  // Callable on any thread.  Returns whether you're currently on a particular
  // thread.
  //
  // WARNING:
  //   When running under unit-tests, this will return true if you're on the
  //   main thread and the thread ID you pass in isn't running.  This is
  //   normally the correct behavior because you want to ignore these asserts
  //   unless you've specifically spun up the threads, but be mindful of it.
  static bool CurrentlyOn(ID identifier);

 private:
  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;

  // This lock protects |chrome_threads_|.  Do not read or modify that array
  // without holding this lock.  Do not block while holding this lock.
  static Lock lock_;

  // An array of the ChromeThread objects.  This array is protected by |lock_|.
  // The threads are not owned by this array.  Typically, the threads are owned
  // on the UI thread by the g_browser_process object.  ChromeThreads remove
  // themselves from this array upon destruction.
  static ChromeThread* chrome_threads_[ID_COUNT];
};

#endif  // #ifndef CHROME_BROWSER_CHROME_THREAD_H__
