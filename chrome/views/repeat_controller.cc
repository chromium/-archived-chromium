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
