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

#include "chrome/common/throb_animation.h"

static const int kDefaultThrobDurationMS = 400;

ThrobAnimation::ThrobAnimation(AnimationDelegate* target)
    : SlideAnimation(target),
      slide_duration_(GetSlideDuration()),
      throb_duration_(kDefaultThrobDurationMS),
      cycles_remaining_(0),
      throbbing_(false) {
}

void ThrobAnimation::StartThrobbing(int cycles_til_stop) {
  cycles_remaining_ = cycles_til_stop;
  throbbing_ = true;
  SlideAnimation::SetSlideDuration(throb_duration_);
  if (IsAnimating())
    return;  // We're already running, we'll cycle when current loop finishes.

  if (IsShowing())
    SlideAnimation::Hide();
  else
    SlideAnimation::Show();
  cycles_remaining_ = cycles_til_stop;
}

void ThrobAnimation::Reset() {
  ResetForSlide();
  SlideAnimation::Reset();
}

void ThrobAnimation::Show() {
  ResetForSlide();
  SlideAnimation::Show();
}

void ThrobAnimation::Hide() {
  ResetForSlide();
  SlideAnimation::Hide();
}

void ThrobAnimation::Run() {
  SlideAnimation::Run();
  if (!IsAnimating() && throbbing_) {
    // Were throbbing a finished a cycle. Start the next cycle unless we're at
    // the end of the cycles, in which case we stop.
    cycles_remaining_--;
    if (IsShowing()) {
      // We want to stop hidden, hence this doesn't check cycles_remaining_.
      SlideAnimation::Hide();
    } else if (cycles_remaining_ > 0) {
      SlideAnimation::Show();
    } else {
      // We're done throbbing.
      throbbing_ = false;
    }
  }
}

void ThrobAnimation::ResetForSlide() {
  SlideAnimation::SetSlideDuration(slide_duration_);
  cycles_remaining_ = 0;
  throbbing_ = false;
}
