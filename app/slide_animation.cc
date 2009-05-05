// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/slide_animation.h"

#include <math.h>

// How many frames per second to target.
static const int kDefaultFramerateHz = 50;

// How long animations should take by default.
static const int kDefaultDurationMs = 120;

SlideAnimation::SlideAnimation(AnimationDelegate* target)
    : Animation(kDefaultFramerateHz, target),
      target_(target),
      tween_type_(EASE_OUT),
      showing_(false),
      value_start_(0),
      value_end_(0),
      value_current_(0),
      slide_duration_(kDefaultDurationMs) {
}

SlideAnimation::~SlideAnimation() {
}

void SlideAnimation::Reset() {
  Reset(0);
}

void SlideAnimation::Reset(double value) {
  Stop();
  showing_ = static_cast<bool>(value == 1);
  value_current_ = value;
}

void SlideAnimation::Show() {
  // If we're already showing (or fully shown), we have nothing to do.
  if (showing_)
    return;

  showing_ = true;
  value_start_ = value_current_;
  value_end_ = 1.0;

  // Make sure we actually have something to do.
  if (slide_duration_ == 0) {
    AnimateToState(1.0);  // Skip to the end of the animation.
    return;
  } else if (value_current_ == value_end_)  {
    return;
  }

  // This will also reset the currently-occuring animation.
  SetDuration(static_cast<int>(slide_duration_ * (1 - value_current_)));
  Start();
}

void SlideAnimation::Hide() {
  // If we're already hiding (or hidden), we have nothing to do.
  if (!showing_)
    return;

  showing_ = false;
  value_start_ = value_current_;
  value_end_ = 0.0;

  // Make sure we actually have something to do.
  if (slide_duration_ == 0) {
    AnimateToState(0.0);  // Skip to the end of the animation.
    return;
  } else if (value_current_ == value_end_) {
    return;
  }

  // This will also reset the currently-occuring animation.
  SetDuration(static_cast<int>(slide_duration_ * value_current_));
  Start();
}

void SlideAnimation::AnimateToState(double state) {
  if (state > 1.0)
    state = 1.0;

  // Make our animation ease-out.
  switch (tween_type_) {
    case EASE_IN:
      state = pow(state, 2);
      break;
    case EASE_IN_OUT:
      if (state < 0.5)
        state = pow(state * 2, 2) / 2.0;
      else
        state = 1.0 - (pow((state - 1.0) * 2, 2) / 2.0);
      break;
    case FAST_IN_OUT:
      state = (pow(state - 0.5, 3) + 0.125) / 0.25;
      break;
    case NONE:
      // state remains linear.
      break;
    case EASE_OUT_SNAP:
      state = 0.95 * (1.0 - pow(1.0 - state, 2));
      break;
    case EASE_OUT:
    default:
      state = 1.0 - pow(1.0 - state, 2);
      break;
  }

  value_current_ = value_start_ + (value_end_ - value_start_) * state;

  // Implement snapping.
  if (tween_type_ == EASE_OUT_SNAP && fabs(value_current_ - value_end_) <= 0.06)
    value_current_ = value_end_;

  // Correct for any overshoot (while state may be capped at 1.0, let's not
  // take any rounding error chances.
  if ((value_end_ >= value_start_ && value_current_ > value_end_) ||
      (value_end_ < value_start_ && value_current_ < value_end_)) {
    value_current_ = value_end_;
  }
}
