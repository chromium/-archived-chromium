// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

void ThrobAnimation::Step() {
  Animation::Step();
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
