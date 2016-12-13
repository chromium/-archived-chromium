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
#include "chrome/browser/chrome_thread.h"

class BrowserWebKitClientImpl;

// This is an object that represents WebKit's "main" thread within the browser
// process.  It should be instantiated and destroyed on the UI thread
// before/after the IO thread is created/destroyed.  All other usage should be
// on the IO thread.  If the browser is being run in --single-process mode, a
// thread will never be spun up, and GetMessageLoop() will always return NULL.
class WebKitThread {
 public:
  // Called from the UI thread.
  WebKitThread();
  ~WebKitThread();

  // Returns the message loop for the WebKit thread unless we're in
  // --single-processuntil mode, in which case it'll return NULL.  Only call
  // from the IO thread.  Only do fast-path work here.
  MessageLoop* GetMessageLoop() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    if (!webkit_thread_.get())
      return InitializeThread();
    return webkit_thread_->message_loop();
  }

 private:
  // Must be private so that we can carefully control its lifetime.
  class InternalWebKitThread : public ChromeThread {
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

  // Returns the WebKit thread's message loop or NULL if we're in
  // --single-process mode.  Do slow-path initialization work here.
  MessageLoop* InitializeThread();

  // Pointer to the actual WebKitThread.  NULL if not yet started.  Only modify
  // from the IO thread while the WebKit thread is not running.
  scoped_ptr<InternalWebKitThread> webkit_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebKitThread);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_THREAD_H_
