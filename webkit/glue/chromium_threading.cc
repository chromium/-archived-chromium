// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/chromium/ChromiumThreading.h>

#undef LOG
#include "base/message_loop.h"

namespace WTF {

static MessageLoop* main_thread = NULL;

void ChromiumThreading::initializeMainThread() {
  main_thread = MessageLoop::current();
}

void ChromiumThreading::scheduleDispatchFunctionsOnMainThread() {
  ASSERT(main_thread);
  main_thread->PostTask(FROM_HERE,
      NewRunnableFunction(&WTF::dispatchFunctionsFromMainThread));
}

}  // namespace WTF
