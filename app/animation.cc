// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/animation.h"

#include "base/message_loop.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#endif

using base::TimeDelta;

Animation::Animation(int frame_rate,
                     AnimationDelegate* delegate)
  : animating_(false),
    frame_rate_(frame_rate),
    timer_interval_(CalculateInterval(frame_rate)),
    duration_(0),
    iteration_count_(0),
    current_iteration_(0),
    state_(0.0),
    delegate_(delegate) {
}

Animation::Animation(int duration,
                     int frame_rate,
                     AnimationDelegate* delegate)
  : animating_(false),
    frame_rate_(frame_rate),
    timer_interval_(CalculateInterval(frame_rate)),
    duration_(0),
    iteration_count_(0),
    current_iteration_(0),
    state_(0.0),
    delegate_(delegate) {

  SetDuration(duration);
}

Animation::~Animation() {
}

void Animation::Reset() {
  current_iteration_ = 0;
}

double Animation::GetCurrentValue() const {
  // Default is linear relationship, subclass to adapt.
  return state_;
}

void Animation::Start() {
  if (!animating_) {
    timer_.Start(TimeDelta::FromMilliseconds(timer_interval_), this,
                 &Animation::Run);

    animating_ = true;
    if (delegate_)
      delegate_->AnimationStarted(this);
  }
}

void Animation::Stop() {
  if (animating_) {
    timer_.Stop();

    animating_ = false;
    if (delegate_) {
      if (state_ >= 1.0)
        delegate_->AnimationEnded(this);
      else
        delegate_->AnimationCanceled(this);
    }
  }
}

void Animation::End() {
  if (animating_) {
    timer_.Stop();

    animating_ = false;
    AnimateToState(1.0);
    if (delegate_)
      delegate_->AnimationEnded(this);
  }
}

bool Animation::IsAnimating() const {
  return animating_;
}

void Animation::SetDuration(int duration) {
  duration_ = duration;
  if (duration_ < timer_interval_)
    duration_ = timer_interval_;
  iteration_count_ = duration_ / timer_interval_;

  // Changing the number of iterations forces us to reset the
  // animation to the first iteration.
  current_iteration_ = 0;
}

void Animation::Step() {
  state_ = static_cast<double>(++current_iteration_) / iteration_count_;

  if (state_ >= 1.0)
    state_ = 1.0;

  AnimateToState(state_);
  if (delegate_)
    delegate_->AnimationProgressed(this);

  if (state_ == 1.0)
    Stop();
}

void Animation::Run() {
  Step();
}

int Animation::CalculateInterval(int frame_rate) {
  int timer_interval = 1000 / frame_rate;
  if (timer_interval < 10)
    timer_interval = 10;
  return timer_interval;
}

// static
bool Animation::ShouldRenderRichAnimation() {
#if defined(OS_WIN)
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    BOOL result;
    // Get "Turn off all unnecessary animations" value.
    if (::SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &result, 0)) {
      // There seems to be a typo in the MSDN document (as of May 2009):
      //   http://msdn.microsoft.com/en-us/library/ms724947(VS.85).aspx
      // The document states that the result is TRUE when animations are
      // _disabled_, but in fact, it is TRUE when they are _enabled_.
      return !!result;
    }
  }
  return !::GetSystemMetrics(SM_REMOTESESSION);
#endif
  return true;
}

