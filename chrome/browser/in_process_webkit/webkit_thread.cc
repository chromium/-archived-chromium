// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_thread.h"

#include "chrome/browser/in_process_webkit/browser_webkitclient_impl.h"
#include "webkit/api/public/WebKit.h"

base::LazyInstance<Lock> WebKitThread::global_webkit_lock_(
    base::LINKER_INITIALIZED);
int WebKitThread::global_webkit_ref_count_ = 0;
WebKitThread::InternalWebKitThread* WebKitThread::global_webkit_thread_ = NULL;

WebKitThread::WebKitThread()
    : cached_webkit_thread_(NULL) {
  // The thread is started lazily by InitializeThread().
}

WebKitThread::InternalWebKitThread::InternalWebKitThread()
    : base::Thread("WebKit"),
      webkit_client_(NULL) {
}

void WebKitThread::InternalWebKitThread::Init() {
  DCHECK(!webkit_client_);
  webkit_client_ = new BrowserWebKitClientImpl;
  DCHECK(webkit_client_);
  WebKit::initialize(webkit_client_);
  // Don't do anything heavyweight here since this can block the IO thread from
  // executing (since InitializeThread() is often called on the IO thread).
}

void WebKitThread::InternalWebKitThread::CleanUp() {
  DCHECK(webkit_client_);
  WebKit::shutdown();
  delete webkit_client_;
  webkit_client_ = NULL;
}

WebKitThread::~WebKitThread() {
  AutoLock lock(global_webkit_lock_.Get());
  if (cached_webkit_thread_) {
    DCHECK(global_webkit_ref_count_ > 0);
    if (--global_webkit_ref_count_ == 0) {
      // TODO(jorlow): Make this safe.
      DCHECK(MessageLoop::current() != global_webkit_thread_->message_loop());
      global_webkit_thread_->Stop();
      delete global_webkit_thread_;
      global_webkit_thread_ = NULL;
    }
  }
}

void WebKitThread::InitializeThread() {
  AutoLock lock(global_webkit_lock_.Get());
  if (!cached_webkit_thread_) {
    if (!global_webkit_thread_) {
      global_webkit_thread_ = new InternalWebKitThread;
      DCHECK(global_webkit_thread_);
      bool started = global_webkit_thread_->Start();
      DCHECK(started);
    }
    ++global_webkit_ref_count_;
    // The cached version can be accessed outside of global_webkit_lock_.
    cached_webkit_thread_ = global_webkit_thread_;
  }
  DCHECK(cached_webkit_thread_->IsRunning());
}
