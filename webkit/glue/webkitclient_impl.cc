// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webkitclient_impl.h"

#include "base/message_loop.h"

namespace webkit_glue {

WebKitClientImpl::WebKitClientImpl()
    : main_loop_(MessageLoop::current()),
      shared_timer_func_(NULL) {
}

WebKit::WebClipboard* WebKitClientImpl::clipboard() {
  return &clipboard_;
}

double WebKitClientImpl::currentTime() {
  return base::Time::Now().ToDoubleT();
}

// HACK to see if this code impacts the intl1 page cycler
#if defined(OS_WIN)
class SharedTimerTask;

// We maintain a single active timer and a single active task for
// setting timers directly on the platform.
static SharedTimerTask* shared_timer_task;
static void (*shared_timer_function)();

// Timer task to run in the chrome message loop.
class SharedTimerTask : public Task {
 public:
  SharedTimerTask(void (*callback)()) : callback_(callback) {}

  virtual void Run() {
    if (!callback_)
      return;
    // Since we only have one task running at a time, verify 'this' is it
    DCHECK(shared_timer_task == this);
    shared_timer_task = NULL;
    callback_();
  }

  void Cancel() {
    callback_ = NULL;
  }

 private:
  void (*callback_)();
  DISALLOW_COPY_AND_ASSIGN(SharedTimerTask);
};

void WebKitClientImpl::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_function = func;
}

void WebKitClientImpl::setSharedTimerFireTime(double fire_time) {
  DCHECK(shared_timer_function);
  int interval = static_cast<int>((fire_time - currentTime()) * 1000);
  if (interval < 0)
    interval = 0;

  stopSharedTimer();

  // Verify that we didn't leak the task or timer objects.
  DCHECK(shared_timer_task == NULL);
  shared_timer_task = new SharedTimerTask(shared_timer_function);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, shared_timer_task,
                                          interval);
}

void WebKitClientImpl::stopSharedTimer() {
  if (!shared_timer_task)
    return;
  shared_timer_task->Cancel();
  shared_timer_task = NULL;
}

#else

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

#endif

void WebKitClientImpl::callOnMainThread(void (*func)()) {
  main_loop_->PostTask(FROM_HERE, NewRunnableFunction(func));
}

}  // namespace webkit_glue
