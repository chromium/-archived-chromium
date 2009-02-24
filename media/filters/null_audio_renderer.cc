// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "media/base/filter_host.h"
#include "media/filters/null_audio_renderer.h"

namespace media {

// How "long" our buffer should be in terms of milliseconds.  In OnInitialize
// we calculate the size of one second of audio data and use this number to
// allocate a buffer to pass to FillBuffer.
static const size_t kBufferSizeInMilliseconds = 100;

NullAudioRenderer::NullAudioRenderer()
    : AudioRendererBase(kDefaultMaxQueueSize),
      playback_rate_(0.0f),
      bytes_per_millisecond_(0),
      buffer_size_(0),
      thread_(NULL),
      shutdown_(false) {
}

NullAudioRenderer::~NullAudioRenderer() {
  Stop();
}

// static
bool NullAudioRenderer::IsMediaFormatSupported(
    const MediaFormat* media_format) {
  int channels;
  int sample_rate;
  int sample_bits;
  return ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits);
}

void NullAudioRenderer::SetPlaybackRate(float playback_rate) {
  playback_rate_ = playback_rate;
}

void NullAudioRenderer::SetVolume(float volume) {
  // Do nothing.
}

void NullAudioRenderer::ThreadMain() {
  // Loop until we're signaled to stop.
  while (!shutdown_) {
    float sleep_in_milliseconds = 0.0f;

    // Only consume buffers when actually playing.
    if (playback_rate_ > 0.0f)  {
      size_t bytes = FillBuffer(buffer_.get(), buffer_size_);

      // Calculate our sleep duration, taking playback rate into consideration.
      sleep_in_milliseconds =
          floor(bytes / static_cast<float>(bytes_per_millisecond_));
      sleep_in_milliseconds /= playback_rate_;
    }

    PlatformThread::Sleep(static_cast<int>(sleep_in_milliseconds));
  }
}

bool NullAudioRenderer::OnInitialize(const MediaFormat* media_format) {
  // Parse out audio parameters.
  int channels;
  int sample_rate;
  int sample_bits;
  if (!ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits)) {
    return false;
  }

  // Calculate our bytes per millisecond value and allocate our buffer.
  bytes_per_millisecond_ = (channels * sample_rate * sample_bits / 8)
      / base::Time::kMillisecondsPerSecond;
  buffer_size_ = bytes_per_millisecond_ * kBufferSizeInMilliseconds;
  buffer_.reset(new uint8[buffer_size_]);
  DCHECK(buffer_.get());

  // It's safe to start the thread now because it simply sleeps when playback
  // rate is 0.0f.
  return PlatformThread::Create(0, this, &thread_);
}

void NullAudioRenderer::OnStop() {
  shutdown_ = true;
  if (thread_)
    PlatformThread::Join(thread_);
}

}  // namespace media
