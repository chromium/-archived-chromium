// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_

#include "base/lazy_instance.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/thread.h"

class BrowserWebKitClientImpl;

// This is an object that represents WebKit's "main" thread within the browser
// process.  You can create as many instances of this class as you'd like;
// they'll all point to the same thread and you're guaranteed they'll
// initialize in a thread-safe way, though WebKitThread instances should
// probably be shared when it's easy to do so.  The first time you call
// GetMessageLoop() or EnsureWebKitInitialized() the thread will be created
// and WebKit initialized.  When the last instance of WebKitThread is
// destroyed, WebKit is shut down and the thread is stopped.
// THIS CLASS MUST NOT BE DEREFED TO 0 ON THE WEBKIT THREAD (for now).
class WebKitThread : public base::RefCountedThreadSafe<WebKitThread> {
 public:
  WebKitThread();

  MessageLoop* GetMessageLoop() {
    if (!cached_webkit_thread_)
      InitializeThread();
    return cached_webkit_thread_->message_loop();
  }

  void EnsureWebKitInitialized() {
    if (!cached_webkit_thread_)
      InitializeThread();
  }

 private:
  // Must be private so that we can carefully control its lifetime.
  class InternalWebKitThread : public base::Thread {
   public:
    InternalWebKitThread();
    virtual ~InternalWebKitThread() { }
    // Does the actual initialization and shutdown of WebKit.  Called at the
    // beginning and end of the thread's lifetime.
    virtual void Init();
    virtual void CleanUp();

   private:
    BrowserWebKitClientImpl* webkit_client_;
  };

  friend class base::RefCountedThreadSafe<WebKitThread>;
  ~WebKitThread();

  void InitializeThread();

  // If this is set, then this object has incremented the global WebKit ref
  // count and will shutdown the thread if it sees the ref count go to 0.
  // It's assumed that once this is non-NULL, the pointer will be valid until
  // destruction.
  InternalWebKitThread* cached_webkit_thread_;

  // If there are multiple WebKitThread object (should only be possible in
  // unittests at the moment), make sure they all share one real thread.
  static base::LazyInstance<Lock> global_webkit_lock_;
  static int global_webkit_ref_count_;
  static InternalWebKitThread* global_webkit_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebKitThread);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
