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

#include "base/message_loop.h"
#include "chrome/common/animation.h"

Animation::Animation(int frame_rate,
                     AnimationDelegate* delegate)
  : animating_(false),
    frame_rate_(frame_rate),
    timer_interval_(CalculateInterval(frame_rate)),
    duration_(0),
    iteration_count_(0),
    current_iteration_(0),
    state_(0.0),
    delegate_(delegate),
    timer_(TimeDelta::FromMilliseconds(timer_interval_)) {
  timer_.set_unowned_task(this);
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
    delegate_(delegate),
    timer_(TimeDelta::FromMilliseconds(timer_interval_)) {
  timer_.set_unowned_task(this);

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
    timer_.Start();

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

bool Animation::IsAnimating() {
  return animating_;
}

void Animation::Run() {
  state_ = static_cast<double>(++current_iteration_) / iteration_count_;

  if (state_ >= 1.0)
    state_ = 1.0;

  AnimateToState(state_);
  if (delegate_)
    delegate_->AnimationProgressed(this);

  if (state_ == 1.0)
    Stop();
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

int Animation::CalculateInterval(int frame_rate) {
  int timer_interval = 1000 / frame_rate;
  if (timer_interval < 10)
    timer_interval = 10;
  return timer_interval;
}
