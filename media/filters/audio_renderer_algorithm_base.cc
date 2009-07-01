// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm_base.h"

#include "media/base/buffers.h"

namespace media {

// The maximum size of the queue, which also acts as the number of initial reads
// to perform for buffering.  The size of the queue should never exceed this
// number since we read only after we've dequeued and released a buffer in
// callback thread.
//
// This is sort of a magic number, but for 44.1kHz stereo audio this will give
// us enough data to fill approximately 4 complete callback buffers.
const size_t kDefaultMaxQueueSize = 16;

AudioRendererAlgorithmBase::AudioRendererAlgorithmBase()
    : channels_(0),
      sample_bytes_(0),
      playback_rate_(0.0f) {
}

AudioRendererAlgorithmBase::~AudioRendererAlgorithmBase() {}

void AudioRendererAlgorithmBase::Initialize(int channels,
                                            int sample_bits,
                                            float initial_playback_rate,
                                            RequestReadCallback* callback) {
  DCHECK_GT(channels, 0);
  DCHECK_GT(sample_bits, 0);
  DCHECK(callback);
  DCHECK_EQ(sample_bits % 8, 0) << "We only support 8, 16, 32 bit audio.";

  channels_ = channels;
  sample_bytes_ = sample_bits / 8;
  request_read_callback_.reset(callback);

  set_playback_rate(initial_playback_rate);

  for (size_t i = 0; i < kDefaultMaxQueueSize; ++i)
    request_read_callback_->Run();
}

void AudioRendererAlgorithmBase::FlushBuffers() {
  // Clear the queue of decoded packets (releasing the buffers).
  queue_.clear();
}

void AudioRendererAlgorithmBase::EnqueueBuffer(Buffer* buffer_in) {
  // If we're at end of stream, |buffer_in| contains no data.
  if (!buffer_in->IsEndOfStream()) {
    queue_.push_back(buffer_in);
    DCHECK_LE(queue_.size(), kDefaultMaxQueueSize);
  }
}

float AudioRendererAlgorithmBase::playback_rate() {
  return playback_rate_;
}

void AudioRendererAlgorithmBase::set_playback_rate(float new_rate) {
  DCHECK_GE(new_rate, 0.0);
  playback_rate_ = new_rate;
}

bool AudioRendererAlgorithmBase::IsQueueEmpty() {
  return queue_.empty();
}

scoped_refptr<Buffer> AudioRendererAlgorithmBase::FrontQueue() {
  return queue_.front();
}

void AudioRendererAlgorithmBase::PopFrontQueue() {
  queue_.pop_front();
  request_read_callback_->Run();
}

int AudioRendererAlgorithmBase::channels() {
  return channels_;
}

int AudioRendererAlgorithmBase::sample_bytes() {
  return sample_bytes_;
}

}  // namespace media
