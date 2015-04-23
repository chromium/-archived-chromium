// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/repeat_controller.h"

using base::TimeDelta;

namespace views {

// The delay before the first and then subsequent repeats. Values taken from
// XUL code: http://mxr.mozilla.org/seamonkey/source/layout/xul/base/src/nsRepeatService.cpp#52
static const int kInitialRepeatDelay = 250;
static const int kRepeatDelay = 50;

///////////////////////////////////////////////////////////////////////////////
// RepeatController, public:

RepeatController::RepeatController(RepeatCallback* callback)
    : callback_(callback) {
}

RepeatController::~RepeatController() {
}

void RepeatController::Start() {
  // The first timer is slightly longer than subsequent repeats.
  timer_.Start(TimeDelta::FromMilliseconds(kInitialRepeatDelay), this,
               &RepeatController::Run);
}

void RepeatController::Stop() {
  timer_.Stop();
}

///////////////////////////////////////////////////////////////////////////////
// RepeatController, private:

void RepeatController::Run() {
  timer_.Start(TimeDelta::FromMilliseconds(kRepeatDelay), this,
               &RepeatController::Run);
  callback_->Run();
}

}  // namespace views
