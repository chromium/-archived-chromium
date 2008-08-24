// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/repeat_controller.h"

namespace ChromeViews {

// The delay before the first and then subsequent repeats. Values taken from
// XUL code: http://mxr.mozilla.org/seamonkey/source/layout/xul/base/src/nsRepeatService.cpp#52
static const int kInitialRepeatDelay = 250;
static const int kRepeatDelay = 50;

///////////////////////////////////////////////////////////////////////////////
// RepeatController, public:

RepeatController::RepeatController(RepeatCallback* callback)
    : timer_(NULL),
      callback_(callback) {
}

RepeatController::~RepeatController() {
  DestroyTimer();
}

void RepeatController::Start() {
  DCHECK(!timer_);
  // The first timer is slightly longer than subsequent repeats.
  timer_ = MessageLoop::current()->timer_manager()->StartTimer(
      kInitialRepeatDelay, this, false);
}

void RepeatController::Stop() {
  DestroyTimer();
}

void RepeatController::Run() {
  DestroyTimer();

  // TODO(beng): (Cleanup) change this to just Run() when base rolls forward.
  callback_->RunWithParams(Tuple0());
  timer_ = MessageLoop::current()->timer_manager()->StartTimer(
      kRepeatDelay, this, true);
}

///////////////////////////////////////////////////////////////////////////////
// RepeatController, private:

void RepeatController::DestroyTimer() {
  if (!timer_)
    return;

  MessageLoop::current()->timer_manager()->StopTimer(timer_);
  delete timer_;
  timer_ = NULL;
}

}

