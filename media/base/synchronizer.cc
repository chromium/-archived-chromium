// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/synchronizer.h"

namespace media {

const int64 Synchronizer::kMinFrameDelayUs = 0;
const int64 Synchronizer::kMaxFrameDelayUs = 250000;

Synchronizer::Synchronizer() {
}

Synchronizer::~Synchronizer() {
}

void Synchronizer::StartRendering() {
  rendering_start_ = base::TimeTicks::Now();
}

void Synchronizer::StopRendering() {
  rendering_stop_ = base::TimeTicks::Now();
}

void Synchronizer::CalculateDelay(base::TimeDelta time,
                                  const StreamSample* now,
                                  const StreamSample* next,
                                  base::TimeDelta* delay_out,
                                  bool* should_skip_out) {
  // How long rendering took.
  base::TimeDelta render_delta = rendering_stop_ - rendering_start_;

  // The duration to display the sample |now|.
  base::TimeDelta duration = now->GetDuration();

  // The presentation timestamp (pts) of the sample |now|.
  base::TimeDelta now_pts = now->GetTimestamp();

  // The presentation timestamp (pts) of the next sample.
  base::TimeDelta next_pts;

  // If we were provided the next sample in the stream |next|, use it to
  // calculate the actual sample duration as opposed to the expected duration
  // provided by the current sample |now|.
  //
  // We also use |next| to get the exact next timestamp as opposed to assuming
  // it will be |now| + |now|'s duration, which may not always be true.
  if (next) {
    next_pts = next->GetTimestamp();
    duration = next_pts - now_pts;

    // Timestamps appear out of order, so skip this frame.
    if (duration.InMicroseconds() < 0) {
      *delay_out = base::TimeDelta();
      *should_skip_out = true;
      return;
    }
  } else {
    // Assume next presentation timestamp is |now| + |now|'s duration.
    next_pts = now_pts + duration;
  }

  base::TimeDelta sleep;
  if (time == last_time_) {
    // The audio time has not changed. To avoid sudden bursts of video after
    // we get audio timing information, we try and guess the current time
    // using the duration of the frame.
    sleep = duration - render_delta;
    if (sleep.InMicroseconds() < kMinFrameDelayUs) {
      sleep = base::TimeDelta::FromMicroseconds(kMinFrameDelayUs);
    }
  } else {
    // The audio time has changed.  The amount of time to delay is equal to
    // the time until the next sample from now minus how long it takes to
    // render, clamped to within the min/max frame delay constants.
    sleep = next_pts - time - render_delta;
    if (sleep.InMicroseconds() < kMinFrameDelayUs) {
      sleep = base::TimeDelta::FromMicroseconds(kMinFrameDelayUs);
    } else if (sleep.InMicroseconds() > kMaxFrameDelayUs) {
      sleep = base::TimeDelta::FromMicroseconds(kMaxFrameDelayUs);
    }
  }
  last_time_ = time;

  *delay_out = sleep;
  *should_skip_out = false;
}

}  // namespace media
