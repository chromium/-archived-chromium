// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_thread.h"

#include "base/command_line.h"
#include "chrome/browser/in_process_webkit/browser_webkitclient_impl.h"
#include "chrome/common/chrome_switches.h"
#include "webkit/api/public/WebKit.h"

// This happens on the UI thread before the IO thread has been shut down.
WebKitThread::WebKitThread() {
  // The thread is started lazily by InitializeThread() on the IO thread.
}

// This happens on the UI thread after the IO thread has been shut down.
WebKitThread::~WebKitThread() {
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
}

WebKitThread::InternalWebKitThread::InternalWebKitThread()
    : ChromeThread(ChromeThread::WEBKIT),
      webkit_client_(NULL) {
}

void WebKitThread::InternalWebKitThread::Init() {
  DCHECK(!webkit_client_);
  webkit_client_ = new BrowserWebKitClientImpl;
  DCHECK(webkit_client_);
  WebKit::initialize(webkit_client_);
  // Don't do anything heavyweight here since this can block the IO thread from
  // executing (since InitializeThread() is called on the IO thread).
}

void WebKitThread::InternalWebKitThread::CleanUp() {
  // Don't do anything heavyweight here since this can block the IO thread from
  // executing (since the thread is shutdown from the IO thread).
  DCHECK(webkit_client_);
  WebKit::shutdown();
  delete webkit_client_;
}

MessageLoop* WebKitThread::InitializeThread() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return NULL;

  DCHECK(!webkit_thread_.get());
  webkit_thread_.reset(new InternalWebKitThread);
  bool started = webkit_thread_->Start();
  DCHECK(started);
  return webkit_thread_->message_loop();
}
