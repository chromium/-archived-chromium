// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility class for video renderers to help synchronize against a reference
// clock while also taking rendering duration into account.
//
// Video renderers should maintain an instance of this object for the entire
// lifetime since Synchronizer maintains internal state to improve playback.
//
// VideoRenderer usage is as follows:
//   Receive a new frame from the decoder
//   Call StartRendering
//   Perform colour space conversion and render the frame
//   Call StopRendering
//   Call CalculateDelay passing in the rendered frame and optional next frame
//   Issue delayed task or sleep on thread for returned amount of time

#ifndef MEDIA_BASE_SYNCHRONIZER_H_
#define MEDIA_BASE_SYNCHRONIZER_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "media/base/buffers.h"

namespace media {

class Synchronizer {
 public:
  Synchronizer();
  ~Synchronizer();

  // Starts the rendering timer.
  void StartRendering();

  // Stops the rendering timer.
  void StopRendering();

  // Calculates the appropriate amount of delay in microseconds given the
  // current time, the current sample and the next sample (may be NULL,
  // but is recommended to pass in for smoother playback).
  void CalculateDelay(base::TimeDelta time, const StreamSample* now,
                      const StreamSample* next, base::TimeDelta* delay_out,
                      bool* should_skip_out);

 private:
  static const int64 kMinFrameDelayUs;
  static const int64 kMaxFrameDelayUs;

  base::TimeTicks rendering_start_;
  base::TimeTicks rendering_stop_;
  base::TimeDelta last_time_;

  DISALLOW_COPY_AND_ASSIGN(Synchronizer);
};

}  // namespace media

#endif  // MEDIA_BASE_SYNCHRONIZER_H_
