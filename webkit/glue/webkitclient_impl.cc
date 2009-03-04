// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webkitclient_impl.h"

#include "base/message_loop.h"

// HACK for testing purposes only.  Just trying a change on the buildbots.
#if defined(OS_WIN)
namespace WTF { double currentTime(); }
#endif

namespace webkit_glue {

WebKitClientImpl::WebKitClientImpl()
    : main_loop_(MessageLoop::current()),
      shared_timer_func_(NULL) {
}

WebKit::WebClipboard* WebKitClientImpl::clipboard() {
  return &clipboard_;
}

double WebKitClientImpl::currentTime() {
#if defined(OS_WIN)
  return WTF::currentTime();
#else
  return base::Time::Now().ToDoubleT();
#endif
}

void WebKitClientImpl::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_func_ = func;
}

void WebKitClientImpl::setSharedTimerFireTime(double fire_time) {
  int interval = static_cast<int>((fire_time - currentTime()) * 1000);
  if (interval < 0)
    interval = 0;

  shared_timer_.Stop();
  shared_timer_.Start(base::TimeDelta::FromMilliseconds(interval), this,
                      &WebKitClientImpl::DoTimeout);
}

void WebKitClientImpl::stopSharedTimer() {
  shared_timer_.Stop();
}

void WebKitClientImpl::callOnMainThread(void (*func)()) {
  main_loop_->PostTask(FROM_HERE, NewRunnableFunction(func));
}

}  // namespace webkit_glue
